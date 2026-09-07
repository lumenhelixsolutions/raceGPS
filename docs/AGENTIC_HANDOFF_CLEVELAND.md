# AGENTIC_HANDOFF_CLEVELAND — raceGPS Burke Lakefront Showcase

**Purpose:** Complete handoff so a new agentic team can continue the Cleveland showcase **without chat history**.

**Canonical for:** Cleveland / Burke Lakefront / Budweiser 500 Historic Circuit work on branch `feature/cleveland-showcase-demo`.
**Repo:** https://github.com/lumenhelixsolutions/raceGPS.git
**Org path note:** GitHub org is `lumenhelixsolutions` (not LumenHelixLab).

Older long-form engineering handoff at repo root (`raceGPS — Cleveland Historic Circuit Showcase_ Engineering Handoff & Demo Sprint Technical Specification v1.0.md`) is **superseded for day-to-day agent work** — treat **this file** as canonical status + next actions. Keep the v1.0 spec as historical product intent only.

---

## Product goal

Deliver **Midnight Club / Midnight Run clone visual quality** for **Burke Lakefront Airport** (Budweiser 500 / Cleveland Historic Circuit).

Phased:

1. **Now / M2:** Working **3-car race** with arcade-fun **Chaos** physics + lighting (grid, AI, checkpoints, EndRace).
2. **Next:** **Garage** (stats + abilities) per superpowers garage plan.
3. **Then:** **GPS + Karla** populate-on-fly skyline realism (T10 HISM + procedural).

---

## Stack

| Layer | Choice |
|-------|--------|
| Engine | **Unreal Engine 5.7** |
| App / Target | `apps/unreal-akron-beta` / `raceGPSAkronBeta` (+ Editor) |
| Map | `/Game/Maps/Cleveland5_0KmWorld` |
| Cleveland GameMode | `ClevelandShowcaseGameMode` via `LaunchCleveland.bat` |
| **Hard constraint** | `GlobalDefaultGameMode` must stay **`CruiseSprintGameMode`** (Akron). Cleveland overrides only at launch / map / bat flags — do not change project default GM. |
| Vehicles | Chaos vehicles; CARLA Charger BP `/Game/Vehicles/DodgeCharger2024/BP_DodgeCharger2024` |
| City | T10 ~120k HISM + **Karla** procedural skyline; **Cesium spike paused** (needs ion token) |
| City tooling | Python citypack / Karla scripts under `scripts/` and `Content/Python/` |
| GitHub | `lumenhelixsolutions/raceGPS` |

See also: [`docs/STACK.md`](STACK.md) (one-pager).

---

## Architecture (key classes)

| Piece | Role |
|-------|------|
| `ClevelandShowcaseGameMode` | Showcase race lifecycle (intro → countdown → racing → end); playtest flags |
| `RaceGridManager` | 3-car grid spawn / placement |
| `RaceAIDriverController` | AI drivers on Chaos pawns |
| `ChaosVehiclePawn` | Player/AI Chaos vehicle; wheel CDO + recreate / torque fixes |
| `RaceGPSVehicleWheels` Front/Rear | Distinct wheel CDOs (`URaceGPSVehicleWheelFront` / `Rear`) so SetupVehicle gets engine torque |
| `RaceSessionManager` | Session / race state glue |
| `RaceTimerSystem` | Timing |
| `RacingLineComponent` | Racing line guidance / keep-on-line |
| `ClevelandEnvironmentActor` | Environment / night look setup |
| `ClevelandLookDirector` | Look / camera / presentation direction |
| Checkpoint gates | Lap CP sequence (12 CPs); EndRace depends on reliable crossing |
| `LaunchCleveland.bat` | Playtest launcher; flags `-ClevelandAutoLap -ClevelandSkipIntro` |

Supporting wireup notes in-module:

- `apps/unreal-akron-beta/Source/raceGPSAkronBeta/CLEVELAND_CPP_WIREUP.md`
- `apps/unreal-akron-beta/Source/raceGPSAkronBeta/CLEVELAND_ENV_WIREUP.md`
- `apps/unreal-akron-beta/Source/raceGPSAkronBeta/RACESESSIONMANAGER_PATCH.md`

---

## Current status (honest)

### Visuals
- Passes **V11–V15**: night materials, T10 roof-cloud hide (~109k instances), stills historically under `Temp/` (not committed — noisy binaries).
- Night mats / Python generators under `Content/Materials/` and `Content/Python/`.

### Race loop
- **Countdown → Racing works.**
- Recreate storm **FIXED**: distinct wheel CDOs + **one-shot** torque/recreate (`bAuthoredWheelsTorqueFixed`); cars hit ~**60–84 km/h**.
- Playtest outcome sample (`Temp/cleveland_playtest_lap.txt`): `PARTIAL_SPEED_OK_CHECKPOINT_SEEN_NO_ENDRACE`.

### BLOCKER — EndRace not reliable
- `nextCP` often stuck **1/12**.
- Stalls / apron hits; green-flag **gear=-1** flash.
- First-create still logs **empty torque curve** then recreate (functional after recreate, but noisy / fragile).
- Until EndRace is solid, do not claim "full lap demo done."

### Garage
- Design + plan written:
  - `docs/superpowers/specs/2026-09-03-garage-upgrades-abilities-design.md`
  - `docs/superpowers/plans/2026-09-03-garage-upgrades-abilities.md`

### Arcade research (stop endless Chaos iteration)
- Short-term prefer **Chaos Arcade Control + preset**.
- Marketplace options: **R-Tune 2.0** or **Fab Arcade Vehicle System**.
- OSS: **KinetiForge** (MIT).
- Do not burn more sprints only tweaking raw Chaos without a control layer / vendor spike.

---

## Recommended next work (priority order)

1. **Finish M2 EndRace** — CP crossing reliability + keep on racing line + gear 1 at green + torque curve before create.
2. **Apply Chaos Arcade Control** / or spike **R-Tune | Arcade Vehicle System | KinetiForge**.
3. **Garage G0–G2** per plan/spec above.
4. **Visual Midnight Club bar** — doors closed, bloom, ground polish; Cesium ion optional only with token.

---

## How to build / run

```text
# Build Editor (Win64 Development)
raceGPSAkronBetaEditor Win64 Development
# from apps/unreal-akron-beta (or your usual UBT/Build.bat path)

# Launch Cleveland showcase
apps/unreal-akron-beta\LaunchCleveland.bat
# or local playtest helper named playtest if present

# Useful flags (already wired via bat / GM):
-ClevelandAutoLap -ClevelandSkipIntro
```

**Playtest report path:** `apps/unreal-akron-beta/Temp/cleveland_playtest_lap.txt`
(PNG stills in same Temp folder are local-only; do not commit.)

**Constraint reminder:** never change `GlobalDefaultGameMode` away from `CruiseSprintGameMode`.

---

## File map

### Critical Source (`apps/unreal-akron-beta/Source/raceGPSAkronBeta/`)

**Public**
- `Public/ClevelandShowcaseGameMode.h`
- `Public/ClevelandShowcaseTypes.h`
- `Public/ClevelandEnvironmentActor.h`
- `Public/ClevelandLookDirector.h`
- `Public/RaceGridManager.h`
- `Public/RaceAIDriverController.h`
- `Public/RaceGPSVehicleWheels.h`
- `Public/RacingLineComponent.h`
- `Public/ChaosVehiclePawn.h`
- `Public/RaceTimerSystem.h`
- (also `RaceSessionManager` / related headers as already in tree)

**Private**
- `Private/ClevelandShowcaseGameMode.cpp`
- `Private/ClevelandEnvironmentActor.cpp`
- `Private/ClevelandLookDirector.cpp`
- `Private/ClevelandModuleCompat.h`
- `Private/RaceGridManager.cpp`
- `Private/RaceAIDriverController.cpp`
- `Private/RaceGPSVehicleWheels.cpp`
- `Private/RacingLineComponent.cpp`
- `Private/ChaosVehiclePawn.cpp`
- `Private/RaceSessionManager.cpp`
- `Private/RaceTimerSystem.cpp`
- `Private/DayNightCycle.cpp`, `PostProcessController.cpp`, `BuildingMeshGenerator.cpp` (visual / night)

**Launch / content / tools**
- `apps/unreal-akron-beta/LaunchCleveland.bat`
- `apps/unreal-akron-beta/Content/Materials/` (incl. `M_Night*`)
- `apps/unreal-akron-beta/Content/Python/` (night mats, ISM usage, stubs)
- `apps/unreal-akron-beta/citypacks/`, `apps/unreal-akron-beta/tools/`
- Vehicle BP path: `/Game/Vehicles/DodgeCharger2024/BP_DodgeCharger2024`

### Docs
- `docs/AGENTIC_HANDOFF_CLEVELAND.md` ← **this file (canonical)**
- `docs/STACK.md`
- `docs/CLEVELAND_*.md` (demo, environment, vehicles, visual bar, track provenance, test plans, paint fix, etc.)
- `docs/KARLA.md`
- `docs/superpowers/specs/…`, `docs/superpowers/plans/…`
- Root historical: `raceGPS — Cleveland Historic Circuit Showcase_ Engineering Handoff & Demo Sprint Technical Specification v1.0.md` (point here)

### Scripts / tests
- `scripts/build_cleveland_circuit.py`, `build_cleveland_environment.py`, `karla_visual_kernel.py`
- `tests/test_cleveland_*.py`, `test_karla_visual_kernel.py`, `test_race_ai_control.py`, `test_vehicle_looks.py`

### Reference imagery
- `burke_lakefront_airport.jpg` (include in repo)
- Optional Budweiser 500 reference: `bud500_1.jpg`

---

## Do / Don't

### Do
- Optimize for **arcade-fun** feel, not sim purity.
- Keep **`CruiseSprintGameMode`** as GlobalDefaultGameMode.
- Prefer Arcade Control / vendor spike over endless Chaos wheel CDO iteration.
- Follow garage plan when EndRace is green.
- Commit only when the user explicitly asks (except this handoff push).

### Don't
- Enable / depend on **Cesium** without a valid **ion** token.
- Ship **licensed Dodge branding** / trademarked badging in public builds.
- Call **per-frame `EnsureDefaultWheels`** (or any per-tick physics recreate storm).
- Commit `Temp/` binary stills, `cesium-request-cache.sqlite*`, Intermediate/Saved/Binaries noise, or unrelated root HTML dumps.

---

## Handoff checklist for next agent group

1. Pull `feature/cleveland-showcase-demo`; build Editor; run `LaunchCleveland.bat`.
2. Confirm cars move (~60+ km/h) and dump shows non-zero `drvTq` after green.
3. Reproduce EndRace failure (nextCP stuck / apron / gear flash); fix M2 first.
4. Only then Arcade Control / garage / MC visual bar.
5. Leave `GlobalDefaultGameMode` untouched.
