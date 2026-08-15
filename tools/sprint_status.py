#!/usr/bin/env python3
"""raceGPS sprint status collector.

Emits one AutomationOutput wrapper JSON object to stdout:

    {"artifact": { ...sprint status... }}

Usage:
    python tools/sprint_status.py            # Automation mode: read request JSON
                                             # from stdin, print wrapper to stdout.
    python tools/sprint_status.py --print    # Debug mode: print the artifact JSON
                                             # (no wrapper) to stdout.

Stdlib only. Repo root is resolved as the parent of this script's tools/ dir.
"""

import json
import os
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

# ---------------------------------------------------------------------------
# Sprint metadata — edit this dict when a new sprint starts.
# ---------------------------------------------------------------------------
SPRINT = {
    "sprint_name": "Sprint 1 — Cleveland Citypack",
    "sprint_goal": (
        "Ship the Cleveland 5.0km citypack alongside Akron: bridge/tunnel road "
        "layers, parametrized city pipeline, arcade tire model, and a UE5.7 "
        "runtime-readiness truth pass."
    ),
    "stories": [
        {"id": "S1", "title": "Cleveland 5.0km citypack", "status": "done", "owner": "TOOLS-1"},
        {"id": "S2", "title": "Parametrized city pipeline", "status": "done", "owner": "WG-2"},
        {"id": "S3", "title": "Citypack validator", "status": "done", "owner": "TOOLS-1"},
        {"id": "S4", "title": "Bridge/tunnel layers", "status": "done", "owner": "WG-1"},
        {"id": "S5", "title": "Route generator + loop closer", "status": "done", "owner": "WG-3"},
        {"id": "S9", "title": "Arcade tire model", "status": "done", "owner": "PHYS-1"},
        {"id": "S14", "title": "QA truth pass", "status": "done", "owner": "QA-1"},
        {"id": "S15", "title": "Runtime readiness checker", "status": "done", "owner": "TOOLS-1"},
        {"id": "W5", "title": "UE5 weekly build W5", "status": "done", "owner": "WG-1"},
        {"id": "S8", "title": "CARLA hero car (stretch)", "status": "pending", "owner": "unassigned"},
    ],
    "followups": [
        "Cleveland circuit route loop-closer is a stub (start/finish 873m apart)",
        "Water extractor found 0 rivers — Cuyahoga missing",
        "Akron: 1015 near-miss junction pairs need endpoint snapping",
        "BP_CheckpointGate Blueprint missing — checkpoints are placeholders",
        "S8 CARLA hero car (stretch, not started)",
    ],
}

REPO_ROOT = Path(__file__).resolve().parent.parent
MAPS_DIR = REPO_ROOT / "apps" / "unreal-akron-beta" / "Content" / "Maps"
CITYPACKS_DIR = REPO_ROOT / "citypacks"
PYTEST_TIMEOUT_S = 240
CMD_TIMEOUT_S = 120


def run_cmd(args, cwd=REPO_ROOT, timeout=CMD_TIMEOUT_S):
    """Run a command; return (returncode, stdout, stderr) or None on failure."""
    try:
        proc = subprocess.run(
            args,
            cwd=str(cwd),
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=timeout,
        )
        return proc.returncode, proc.stdout or "", proc.stderr or ""
    except Exception:
        return None


def collect_repo(notes):
    info = {"branch": "unknown", "last_commit": "", "clean": False, "ahead_of_origin": 0}
    res = run_cmd(["git", "branch", "--show-current"])
    if res:
        info["branch"] = res[1].strip() or "unknown"
    else:
        notes.append("auto: git branch query failed")

    res = run_cmd(["git", "log", "-1", "--format=%h %s"])
    if res:
        info["last_commit"] = res[1].strip()
    else:
        notes.append("auto: git log query failed")

    res = run_cmd(["git", "status", "--porcelain"])
    if res:
        # Ignore known-permission-denied noise; porcelain output on stdout
        # decides cleanliness. Filter stray warning lines defensively.
        lines = [
            ln for ln in res[1].splitlines()
            if ln.strip() and not ln.lstrip().startswith("warning:")
        ]
        info["clean"] = len(lines) == 0
    else:
        notes.append("auto: git status failed; reporting clean=false")

    res = run_cmd(["git", "rev-list", "--count", "origin/master..HEAD"])
    if res and res[0] == 0:
        try:
            info["ahead_of_origin"] = int(res[1].strip())
        except ValueError:
            info["ahead_of_origin"] = 0
    # else: no upstream / no origin — stay at 0.
    return info


def collect_tests(notes):
    tests = {"passed": 0, "failed": 0, "skipped": 0, "ok": False}
    res = run_cmd(
        [sys.executable, "-m", "pytest", "tests/", "-q", "-p", "no:cacheprovider"],
        timeout=PYTEST_TIMEOUT_S,
    )
    if res is None:
        tests["failed"] = 1
        notes.append("auto: pytest failed to run or timed out (240s)")
        return tests
    output = res[1] + "\n" + res[2]
    tail = "\n".join(output.splitlines()[-15:])
    m_passed = re.search(r"(\d+)\s+passed", tail)
    m_failed = re.search(r"(\d+)\s+failed", tail)
    m_skipped = re.search(r"(\d+)\s+skipped", tail)
    if m_passed is None and m_failed is None:
        # Pytest ran but produced no summary (collection error, crash...).
        tests["failed"] = 1
        notes.append("auto: pytest produced no summary line (exit %s)" % res[0])
        return tests
    tests["passed"] = int(m_passed.group(1)) if m_passed else 0
    tests["failed"] = int(m_failed.group(1)) if m_failed else 0
    tests["skipped"] = int(m_skipped.group(1)) if m_skipped else 0
    tests["ok"] = tests["failed"] == 0
    return tests


def normalize_name(name):
    return re.sub(r"[^A-Za-z0-9]+", "_", name).strip("_").lower()


def load_json(path):
    try:
        with open(path, encoding="utf-8") as fh:
            return json.load(fh)
    except Exception:
        return None


def count_routes(city_dir, city_id):
    candidate = city_dir / ("%s_routes.json" % city_id)
    if not candidate.exists():
        matches = sorted(city_dir.glob("*_routes.json"))
        candidate = matches[0] if matches else None
    if candidate:
        data = load_json(candidate)
        if isinstance(data, list):
            return len(data)
        if isinstance(data, dict) and isinstance(data.get("routes"), list):
            return len(data["routes"])
    routes_dir = city_dir / "routes"
    if routes_dir.is_dir():
        return len(list(routes_dir.glob("*.json")))
    return 0


def collect_city(city_dir, notes):
    city_id = city_dir.name
    city = {
        "id": city_id,
        "roads": 0,
        "bridges": 0,
        "tunnels": 0,
        "routes": 0,
        "validator_errors": 0,
        "validator_warnings": 0,
        "readiness": "UNKNOWN",
        "has_map": False,
    }

    # --- road graph counts -------------------------------------------------
    graphs = sorted(city_dir.glob("*_road_graph.json"))
    if graphs:
        graph = load_json(graphs[0])
        roads = graph.get("roads") if isinstance(graph, dict) else None
        if isinstance(roads, list):
            city["roads"] = len(roads)
            city["bridges"] = sum(1 for r in roads if isinstance(r, dict) and r.get("is_bridge"))
            city["tunnels"] = sum(1 for r in roads if isinstance(r, dict) and r.get("is_tunnel"))
        else:
            notes.append("auto: %s road graph unreadable; counts set to 0" % city_id)
    else:
        notes.append("auto: %s has no *_road_graph.json; counts set to 0" % city_id)

    city["routes"] = count_routes(city_dir, city_id)

    # --- validator ----------------------------------------------------------
    res = run_cmd([sys.executable, str(REPO_ROOT / "tools" / "validate-citypack.py"), str(city_dir)])
    if res is not None:
        total = re.search(r"TOTAL\s+errors=(\d+)\s+warnings=(\d+)", res[1])
        if total:
            city["validator_errors"] = int(total.group(1))
            city["validator_warnings"] = int(total.group(2))
        else:
            notes.append("auto: %s validator output had no TOTAL line" % city_id)
    else:
        notes.append("auto: %s validator failed to run" % city_id)

    # --- runtime readiness --------------------------------------------------
    level_name = None
    res = run_cmd(
        [sys.executable, str(REPO_ROOT / "tools" / "verify-city-runtime-readiness.py"),
         "--city", city_id]
    )
    if res is not None:
        out = res[1]
        if re.search(r"^PASSED", out, re.MULTILINE):
            city["readiness"] = "PASSED"
        elif re.search(r"^FAILED", out, re.MULTILINE):
            city["readiness"] = "FAILED"
        else:
            notes.append("auto: %s readiness result not recognized" % city_id)
        m = re.search(r"level_name:\s*([A-Za-z0-9_.]+)", out)
        if m:
            level_name = m.group(1)
    else:
        notes.append("auto: %s readiness check failed to run" % city_id)

    # --- generated map ------------------------------------------------------
    try:
        if MAPS_DIR.is_dir():
            wanted = {normalize_name(city_id)}
            if level_name:
                wanted.add(normalize_name(level_name))
            for umap in MAPS_DIR.glob("*.umap"):
                if normalize_name(umap.stem) in wanted:
                    city["has_map"] = True
                    break
    except Exception:
        notes.append("auto: %s map lookup failed" % city_id)

    return city


def collect_cities(notes):
    if not CITYPACKS_DIR.is_dir():
        notes.append("auto: citypacks/ directory missing; no cities collected")
        return []
    cities = []
    for entry in sorted(CITYPACKS_DIR.iterdir()):
        if not entry.is_dir() or entry.name == "templates":
            continue
        try:
            cities.append(collect_city(entry, notes))
        except Exception as exc:  # never let one city kill the run
            notes.append("auto: %s collection crashed: %s" % (entry.name, exc))
            cities.append({
                "id": entry.name, "roads": 0, "bridges": 0, "tunnels": 0,
                "routes": 0, "validator_errors": 0, "validator_warnings": 0,
                "readiness": "UNKNOWN", "has_map": False,
            })
    return cities


def build_artifact():
    notes = []
    artifact = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "sprint_name": SPRINT["sprint_name"],
        "sprint_goal": SPRINT["sprint_goal"],
        "repo": collect_repo(notes),
        "tests": collect_tests(notes),
        "cities": collect_cities(notes),
        "stories": [dict(s) for s in SPRINT["stories"]],
        "followups": list(SPRINT["followups"]),
    }
    artifact["followups"].extend(notes)
    return artifact


def main():
    if "--print" in sys.argv[1:]:
        print(json.dumps(build_artifact(), indent=2))
        return 0

    # Automation mode: consume the request JSON from stdin (may be empty when
    # run manually), then emit the AutomationOutput wrapper on stdout.
    try:
        raw = sys.stdin.read()
        if raw.strip():
            json.loads(raw)  # request fields are not needed by this collector
    except Exception:
        pass
    print(json.dumps({"artifact": build_artifact()}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
