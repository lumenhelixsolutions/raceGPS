# Sprint 2 — AI Swarm Plan: "Make Cleveland Real"

**Branch:** `sprint/cleveland-polish` (from master, post-Sprint-1 merge)
**Trigger:** plan approved after owner's editor drive of Cleveland (Sprint 1 demo)

## Sprint Goal

**Cleveland looks and plays like Cleveland: water under its bridges, closed circuit routes, snapped junctions, real checkpoint gates, and a hero car worth racing.**

## Inputs

- Velocity reference: Sprint 1 delivered 27 SP committed + 5 SP build wave across 7 agent-tasks, all first-pass except 2 integration fixes → effective swarm velocity ~32 SP
- Carryovers + follow-ups from Sprint 1 gate (verified, not guessed)
- Owner's editor-drive impressions (pending — may reorder priorities)

## Story Board (32 SP committed)

| # | Story | SP | Owner | Depends | Source |
|---|---|---|---|---|---|
| T1 | Route loop-closer: implement the `pass` stub in route_engine.py so circuit routes actually close (Cleveland circuit start/finish gap: 873 m) | 5 | WG-1 | — | S1 follow-up |
| T2 | Water extractor fix: Cuyahoga + Lake Erie shoreline missing (0 rivers extracted) — bridges currently span nothing | 5 | WG-2 | — | S1 follow-up |
| T3 | Junction endpoint snapping: merge near-miss pairs <2 m at compile time (Akron: 1,015 pairs; Cleveland: 726) | 5 | WG-1 | T1 | S1 follow-up |
| T4 | BP_CheckpointGate Blueprint from C++ CheckpointGate class via headless pythonscript commandlet; re-run Cleveland map import (idempotent) | 5 | ASSET-1 | — | S1 follow-up |
| T5 | CARLA hero car: download CARLA 0.10.0 UE5 asset pack, import 1 vehicle, CC-BY attribution file, wire to arcade tire config from S9 | 8 | ASSET-1 | T4 | S1 stretch S8 |
| T6 | Recompile Cleveland + Akron with T1–T3, gate through validator (expect: components ↓, near-miss pairs ~0, circuit loop closed) | 3 | QA-1 | T1,T2,T3 | |
| T7 | Validator → CI: wire validate-citypack.py + sprint_status.py into GitHub Actions so regressions block merges | 3 | TOOLS-1 | T6 | |
| T8 | Editor-drive fixes: fast-follow bucket for whatever the owner reports from the Cleveland drive (sized as 1 story; overflow → Sprint 3) | 3 | ORCH | — | step 3 |

**Stretch (5 SP):** T9 — MassEntity staggered traffic spawner (spec Bug #7, never implemented)

## Wave Schedule

```
WAVE 0 (serial)   branch + SPRINT dict update + dashboard refresh
WAVE 1 (parallel) T1 loop-closer │ T2 water extractor │ T4 checkpoint BP │ T5 CARLA download+import
WAVE 2 (serial)   T3 snapping (after T1 lands — same file family) 
WAVE 3 (serial)   T6 recompile both cities → QA gate
WAVE 4 (parallel) T7 CI wiring │ T8 editor-drive fixes │ T9 stretch
WAVE 5 (serial)   ORCH integration → acceptance gate → merge decision
```

Critical path: W0 → T1 → T3 → T6 → W5 (4 hops). Parallel width ≤ 4.

## Acceptance Gate

1. pytest green (baseline 202p/0f — new stories add tests)
2. Cleveland + Akron recompiled; validator: near-miss pairs ≈ 0, circuit loop closed (<50 m start/finish), water features > 0 in Cleveland
3. Cleveland map re-imported headlessly; checkpoints are BP_CheckpointGate instances, not placeholders
4. CARLA car drivable in editor with S9 arcade tires; attribution file in repo
5. CI workflow runs validator on PRs
6. Dashboard refreshed with Sprint 2 board

## Risks

- **CARLA pack size (~GB download)** — T5 is the schedule risk; download starts Wave 1 in parallel so it never blocks the critical path
- **T2 scope unknown** — water extraction may be a tag-filter fix (small) or a missing pipeline stage (big); spike first, cap at 5 SP, overflow to Sprint 3
- **Owner drive impressions** may inject scope — T8 absorbs up to 3 SP, beyond that re-plan
- Editor-drive may reveal the placeholder checkpoint gates are fine visually → T4 shrinks, pull T9 stretch earlier
