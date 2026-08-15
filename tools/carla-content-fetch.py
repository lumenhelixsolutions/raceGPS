#!/usr/bin/env python3
"""Fetch a closure of CARLA content-repo .uasset files with their /Game dependencies.

Usage:
    python tools/carla-content-fetch.py --ref 0.10.0 --out <dir> <repo-relative-path> [<path>...]

Paths are relative to the carla-content repo root, e.g.
    Static/Car/4Wheeled/DodgeCharger2024/SK_DodgeCharger2024.uasset

Every downloaded package is scanned for "/Game/Carla/..." references; any
referenced package not yet downloaded is fetched too (transitive closure),
so material parents / shared textures resolve when the files are dropped
into a UE project's Content/Carla/ directory.

Source repo (CC-BY 4.0): https://bitbucket.org/carla-simulator/carla-content
"""

import argparse
import re
import sys
import urllib.request
from pathlib import Path

API = "https://api.bitbucket.org/2.0/repositories/carla-simulator/carla-content/src"
RAW = "https://bitbucket.org/carla-simulator/carla-content/raw"
GAME_PREFIX = "/Game/Carla/"
MAX_BYTES = 600 * 1024 * 1024  # safety cap


def repo_path_to_game(repo_rel: str) -> str:
    return GAME_PREFIX + repo_rel[: -len(".uasset")]


def game_to_repo_path(game_path: str) -> str:
    assert game_path.startswith(GAME_PREFIX), game_path
    return game_path[len(GAME_PREFIX):] + ".uasset"


def find_refs(data: bytes) -> set:
    refs = set()
    for m in re.findall(rb"/Game/[ -~]{4,}", data):
        pkg = m.decode(errors="ignore").split(".")[0]
        if pkg.startswith(GAME_PREFIX):
            refs.add(pkg)
    return refs


def fetch(ref: str, repo_rel: str, out_root: Path, attempts: int = 4) -> bytes:
    import time

    dest = out_root / repo_rel
    dest.parent.mkdir(parents=True, exist_ok=True)
    urls = [f"{API}/{ref}/{repo_rel}", f"{RAW}/{ref}/{repo_rel}"]
    data = None
    last_exc = None
    for attempt in range(attempts):
        url = urls[attempt % len(urls)]  # alternate API / raw on retries
        req = urllib.request.Request(url, headers={"User-Agent": "carla-content-fetch"})
        try:
            with urllib.request.urlopen(req, timeout=120) as resp:
                data = resp.read()
            break
        except urllib.error.HTTPError as exc:
            last_exc = exc
            if exc.code == 429 and attempt < attempts - 1:
                time.sleep(5 * (attempt + 1))
                continue
            raise
    if data is None:
        raise last_exc
    if not data.startswith(b"\xc1\x83\x2a\x9e"):
        raise RuntimeError(f"{repo_rel}: not a UE package (got {len(data)} bytes)")
    dest.write_bytes(data)
    return data


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ref", default="0.10.0")
    ap.add_argument("--out", required=True)
    ap.add_argument("paths", nargs="+")
    args = ap.parse_args()

    out_root = Path(args.out)
    queue = [repo_path_to_game(p) for p in args.paths]
    done = set()
    total = 0

    while queue:
        game_path = queue.pop(0)
        if game_path in done:
            continue
        repo_rel = game_to_repo_path(game_path)
        dest = out_root / repo_rel
        if dest.exists():
            done.add(game_path)
            continue
        try:
            data = fetch(args.ref, repo_rel, out_root)
        except Exception as exc:  # missing optional deps are non-fatal
            print(f"[warn] {repo_rel}: {exc}")
            done.add(game_path)
            continue
        total += len(data)
        done.add(game_path)
        print(f"[ok] {repo_rel} ({len(data)/1e6:.1f} MB, total {total/1e6:.1f} MB)")
        if total > MAX_BYTES:
            print("[abort] size cap exceeded")
            return 2
        for ref_pkg in sorted(find_refs(data) - done):
            if ref_pkg not in queue:
                queue.append(ref_pkg)

    print(f"[done] {len(done)} packages, {total/1e6:.1f} MB")
    return 0


if __name__ == "__main__":
    sys.exit(main())
