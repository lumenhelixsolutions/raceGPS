# Sprint 1 REBASELINED — AI Swarm plan mapped onto the real racegps codebase

Supersedes `sprint-1-plan-ai-swarm.md` (greenfield assumptions). Verified against the actual repo on 2026-08-14 (see "Evidence" per story).

## New Sprint Goal

**Ship Cleveland: bridge/tunnel-aware compile → validated citypack → UE5.7 import with an arcade-tuned car — reusing the existing pure-Python pipeline, not rebuilding it.**

Why this goal: the last 3 commits were all Cleveland (`c028964`, `b1a53fa`, `c8654d5`); the level spec exists but was never imported into UE5. This sprint finishes that arc and closes the two verified gaps that block it (bridges, tires).

## Story remap (verified status)

| Old # | Greenfield story | Verified reality | New story |
|---|---|---|---|
| 1 | Repo/CI bootstrap | DONE — 5 GitHub workflows exist | **S1 (2 SP):** Git hygiene: commit/stash 71 dirty files, sprint branch, `pip install -r tools/universal-city-compiler/requirements.txt`, pytest baseline green locally |
| 2 | OSM fetch + filter | DONE — `fetch_overpass.py` (retry, backoff, disk cache, road-type tiers) | **S2 (3 SP):** Chunked/streaming Overpass fetch for large extracts (current code reads whole XML into memory — real Cleveland-scale risk) |
| 3 | Osm2Odr harness | REPLACED — pure-Python compiler is canonical by design | **S3 (2 SP):** Compiler interface contract doc + fixture freeze (locks inputs/outputs so agents can work against it) |
| 5 | Bridge pre-filter | **MISSING** — zero bridge/tunnel handling anywhere; Cleveland has Cuyahoga river bridges | **S4 (5 SP):** Bridge/tunnel handling in compiler — elevate or explicitly filter, with tests. **Critical-path** |
| 6 | Spline import | REPLACED — procedural mesh + `ue5-import-level-spec.py` exist | **S5 (5 SP):** Wire Cleveland into UE5: `PreflightSystem.cpp:249` hardcodes Akron; parametrize + import Cleveland spec |
| 7 | Normal fix | DONE — explicit per-vertex normals, `RoadMeshGenerator.cpp:158` | — (closed, no work) |
| 8 | CARLA car export | MISSING — only naming-convention stubs | **S8 (5 SP): STRETCH** — requires CARLA pack download; stub vehicle acceptable for sprint goal |
| 9 | Arcade tires | **MISSING despite handoff claim** — no Friction/TireConfig anywhere in Source | **S9 (3 SP):** Tire friction config in `ChaosVehiclePawn::SetupWheel` (FWheelTuning exists at `:112`) |
| 14 | Validation suite | PARTIAL — schema+connectivity only; naive endpoint matching | **S14 (5 SP):** Extend `validate-citypack.py`: lane geometry, near-miss junction tolerance, route loop/length checks |
| — | (new, found by verification) | uproject says 5.7, Build.bat + docs say 5.5 | **S15 (2 SP):** Engine-version truth pass: fix Build.bat/docs/CI to 5.7, verify clean build |

**Committed: 27 SP** (S1, S2, S3, S4, S5, S9, S14, S15) · **Stretch: 5 SP** (S8)
Buffer well above 20% — first swarm run on a real legacy repo.

## Wave schedule

```
WAVE 0 (serial)   S1 git hygiene + deps + pytest baseline   ── gates everything
WAVE 1 (parallel) S4 bridges (WG-1) │ S9 tires (PHYS-1) │ S14 validation (QA-1) │ S2 chunked fetch (WG-2) │ S3 contract doc (TOOLS-1)
WAVE 2 (serial)   Recompile Cleveland with S4+S2 → gate through extended S14 suite
WAVE 3 (parallel) S5 Cleveland UE5 wiring (WG-3) │ S15 engine truth pass (TOOLS-1)
WAVE 4 (serial)   ORCHESTRATOR integration → QA-1 acceptance gate
STRETCH           S8 CARLA hero car (only if reserve intact)
```

**Critical path: W0 → S4 → W2 Cleveland compile → S5 → W4** (4 hops, unchanged depth)
**Parallel width: up to 5** in Wave 1 — all five stories touch disjoint files (compiler vs vehicle pawn vs validator vs fetcher vs docs).

## Acceptance gate (QA-1, Wave 4)

1. `pytest` green locally (not just CI) including new bridge/tunnel + validation tests
2. Cleveland citypack compiles AND passes extended validator (incl. near-miss junctions)
3. Cleveland loads in UE5.7 via parametrized preflight (no Akron hardcode)
4. Tire config present and tunable in ChaosVehiclePawn
5. Docs/Build.bat/CI all agree on engine version

## Risks (carried + new)

- **71 uncommitted files incl. deleted Target/Provider files** — W0 must resolve before any branch; biggest single risk to the sprint
- **UE5 command-line build may fail without human-in-editor** — S5 verification may need you to press build in the Editor; flagged, not blocking code work
- **CARLA pack size/licensing** — reason S8 is stretch, not committed
- Context-overflow rule unchanged: agents pass paths + summaries, never raw file bytes
- Max 2 retries per story, then escalate with a written failure report
