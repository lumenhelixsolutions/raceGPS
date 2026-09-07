# Cleveland Historic Circuit — demo test plan

Checkboxes only. **No results filled in.** This is not a run log.

## A. Identity / branding

- [ ] HUD/menu shows **raceGPS: Cleveland Historic Circuit** (or product `raceGPS` + circuit `Cleveland Historic Circuit`)
- [ ] Pack id is `cleveland_burke_gp_1997`
- [ ] No title-sponsor strings in UI (historical event names must not be the display name)
- [ ] Copy does not describe the layout as the 1982 ~2.48 mi / 12-turn course

## B. Citypack data (offline)

- [ ] `manifest.json` has `offline: true`, `carla_required: false`, `cesium_required: false`, `engine: UE5`
- [ ] `metadata.json`: `official_length_mi` 2.106, `official_length_m` 3389, `turns` 10, `direction` clockwise, `surface` concrete, airport Burke Lakefront, lat/lon 41.51722 / −81.68306
- [ ] Measured haversine length within **5%** of 3389 m
- [ ] `racing_line.json`: `closed: true`, `frame: wgs84`, ~5–10 m samples, 10 `turn_index` values 1–10, curvature sign positive-left
- [ ] `checkpoints.json`: S/F is index 0, last gate is wrap S/F, `s` monotonic, ~10–12 gates
- [ ] `cleveland_burke_gp.xodr` is a closed loop, planView line/arc, driving lanes ~12 m total, elevation ~174 m AMSL
- [ ] Pit lane documented as 1990 extended exit (old T1/T2), not required to be a second road

## C. Pytest

- [ ] `pytest tests/test_cleveland_circuit.py` from repo/showcase root
- [ ] Closed loop, 10 turns, length ±5% of 3389 m, checkpoint `s` monotonic, Start/Finish present

## D. Unreal demo (UE 5.7 — do not downgrade to 5.5)

- [ ] Built via existing `apps/unreal-akron-beta/BUILD.md` and/or `Build.bat`
- [ ] No CARLA server started or required
- [ ] Garage not entered
- [ ] **RACE** selected from menu
- [ ] Game mode `AClevelandShowcaseGameMode` loads `citypacks/cleveland/burke_gp_1997`
- [ ] Grid is **1 player + 2 Chaos AI**
- [ ] Countdown then racing; AI throttle/steer gated until Racing
- [ ] Session is **one lap**; finished state when player completes lap
- [ ] Standings use spline progress, not Euclidean place
- [ ] Track is clockwise; T1 is the right-hand vortex at the end of the pit/start straight

## E. Geography sanity

- [ ] Course sits on Burke Lakefront pavement (runways/taxiways), lake north, downtown south
- [ ] S/F on the long south taxiway (G) that ends at T1
- [ ] Driven line does not use the 1982 left-right as racing surface

## F. Remaining milestones (expect fail / skip until implemented)

- [ ] M5 pit-lane XODR
- [ ] M6 packaged/PIE smoke signed off
- [ ] M7 HUD polish
- [ ] M8 replay/telemetry export
- [ ] M9 multi-lap weekend
