# raceGPS — Cleveland Historic Circuit Showcase
## Engineering Handoff & Demo Sprint Technical Specification v1.0

**Project:** raceGPS  
**Organization:** Lumen Helix Lab  
**Repository:** `LumenHelixLab/raceGPS`  
**Target branch:** `feature/cleveland-showcase-demo`  
**Proposed milestone:** `v0.2.0-demo.1`  
**Engine:** Unreal Engine 5.5  
**Primary platform:** Windows 10/11 x64  
**Technical objective:** Produce a polished, reproducible, playable Cleveland racing demonstration suitable for recruiting developers, technical evaluation, video capture, and public demonstration.

---

# 1. Executive Directive

The immediate objective is **not** to build the complete raceGPS vision.

The objective is to demonstrate that the existing raceGPS architecture can become a compelling real-world racing game.

Build one exceptional vertical slice:

> **One historically grounded Cleveland circuit. One human-controlled car. Two physics-driven AI competitors. One complete race. One convincing demonstration.**

The demo must prove five things simultaneously:

1. Real geographic space can become a raceGPS course.
2. raceGPS vehicle physics are genuinely playable.
3. multiple autonomous competitors can race using the same physics substrate.
4. the project can produce a visually coherent real-world environment without first solving full city-scale streaming.
5. the existing architecture can support the later **“type a place → build it → drive it”** raceGPS vision.

Anything that does not materially improve those five proofs is outside the sprint.

---

> **CANONICAL AGENT HANDOFF:** For current status, stack, blockers, and next work, use [docs/AGENTIC_HANDOFF_CLEVELAND.md](docs/AGENTIC_HANDOFF_CLEVELAND.md) and [docs/STACK.md](docs/STACK.md). This v1.0 document remains historical product/sprint intent; day-to-day agent continuation should start from AGENTIC_HANDOFF_CLEVELAND.


# 2. Existing Technical Baseline

This sprint **extends the current project; it does not restart it**.

The repository already describes a UE5.5/C++ system combining OpenStreetMap, a Python semantic compiler, OpenDRIVE, generated citypacks, procedural road meshes, Chaos Vehicle physics, race state management, checkpoints, scoring, replay, leaderboards, HUD components, day/night behavior, and traffic systems. 

The current roadmap marks the following major systems as implemented:

- OSM → OpenDRIVE import
- procedural XODR road generation
- Chaos vehicle physics
- race state machine
- checkpoints and route splines
- race scoring
- ghost replay
- leaderboards
- HUD/minimap
- traffic
- tutorial
- achievements
- day/night cycle

Visual/environmental integration remains the major upcoming workstream. 

Therefore:

> **Do not replace working raceGPS systems with a new prototype stack.**

Reuse the existing UE5 module and selectively add the components required for the Cleveland showcase.

---

# 3. Historical Circuit Selection

Cleveland used several Burke Lakefront Airport layouts.

Historical records distinguish:

- **1982–1989:** approximately **2.48 miles**
- **1990–1996:** approximately **2.369 miles**
- **1997–2007:** approximately **2.106 miles**

The later circuit is a particularly useful demonstration target because it is a compact **2.106-mile, 10-turn, flat airport course** with excellent surviving layout documentation.

The event itself began at Burke Lakefront Airport in 1982 and was originally known as the **Budweiser-Cleveland 500**.

### DEMO-001 — Locked Course

For this sprint, use:

**Burke Lakefront Airport — 1997–2007 2.106-mile / 10-turn configuration**

Internal identifier:

`cleveland_burke_gp_1997`

Display name:

**Cleveland Historic Circuit**

This avoids falsely representing the later 10-turn geometry as the exact original 1982 Budweiser 500 configuration.

Future citypack variants may add:

`cleveland_burke_1982`

`cleveland_burke_1990`

Do not delay this sprint to implement those variants.

---

# 4. Demo User Experience

The target public demonstration should require almost no explanation.

### Sequence

**T+0 s**

raceGPS launches.

**T+3–10 s**

Title:

**raceGPS  
CLEVELAND HISTORIC CIRCUIT**

Cleveland skyline / Lake Erie visible beyond the circuit.

**T+10 s**

Player selects:

**RACE**

No garage configuration is required.

**T+15 s**

Three vehicles appear on the starting grid:

- Player
- AI Driver 01
- AI Driver 02

**3 — 2 — 1 — GO**

All three cars leave under vehicle physics.

The race runs for one demonstrable lap.

HUD displays:

- position: `1 / 3`
- lap: `1 / 1`
- elapsed time
- speed
- checkpoint progress

At finish:

- finishing position
- lap time
- opponent times
- restart
- return to menu

The full proof should be understandable in **under five minutes**.

---

# 5. Architecture

```text
                   HISTORICAL COURSE REFERENCE
                              │
                              ▼
                   Cleveland Track Definition
                              │
                 ┌────────────┴────────────┐
                 ▼                         ▼
          route geometry              metadata
                 │
                 ▼
         OpenDRIVE / raceGPS
          Route Representation
                 │
                 ▼
       ┌───────────────────────┐
       │    UE5.5 Runtime      │
       │                       │
       │ Procedural Track      │
       │ Checkpoint System     │
       │ Race Session Manager  │
       │ Racing Line Spline    │
       └───────────┬───────────┘
                   │
       ┌───────────┼────────────┐
       ▼           ▼            ▼
     PLAYER      AI #1        AI #2
       │           │            │
       └───────────┼────────────┘
                   ▼
             Chaos Vehicles
                   │
                   ▼
         Timing / Position / HUD
```

The existing repository already provides the underlying OSM → semantic compiler → OpenDRIVE → UE runtime architecture. 

---

# 6. Critical Architectural Decision: AI Cars

The existing `ATrafficVehicle` implementation is **not sufficient for racing opponents**.

It currently moves an `AActor` directly along predefined road points and explicitly disables simulated physics.  

Do **not** simply increase its speed and call those actors race competitors.

That would demonstrate animated traffic rather than autonomous racing.

The existing `AChaosVehiclePawn`, however, exposes programmatic:

- throttle
- steering
- brake
- handbrake
- speed
- RPM
- tuning

interfaces. 

Therefore the AI racers should control actual `AChaosVehiclePawn` instances.

## New subsystem

Create:

`ARaceAIDriverController`

Suggested files:

```text
apps/unreal-akron-beta/Source/raceGPSAkronBeta/Public/RaceAIDriverController.h
apps/unreal-akron-beta/Source/raceGPSAkronBeta/Private/RaceAIDriverController.cpp
```

### Responsibilities

The controller shall:

1. possess an `AChaosVehiclePawn`;
2. follow a racing-line spline;
3. calculate a configurable look-ahead target;
4. derive steering error from vehicle forward-vector vs. target vector;
5. control throttle using curvature and target velocity;
6. brake ahead of high-curvature sections;
7. recover after spins or course departure;
8. complete an entire lap without waypoint teleportation.

### Minimum controller telemetry

```text
CurrentSplineDistance
TargetSplineDistance
CrossTrackError
HeadingError
CurrentSpeed
TargetSpeed
ThrottleCommand
BrakeCommand
SteeringCommand
RecoveryState
LapProgress
```

Expose these values to the developer HUD.

---

# 7. AI Control Model

Do not implement reinforcement learning for this sprint.

Use deterministic control.

### Steering

Conceptually:

```text
target = spline(position + lookahead)

heading_error =
    signed_angle(vehicle_forward, direction_to_target)

steering =
    K_heading * heading_error
    + K_cross_track * cross_track_error
```

Clamp to:

```text
[-1, +1]
```

### Speed Planning

Associate a target velocity with local spline curvature.

Conceptually:

```text
target_speed =
    clamp(
        Vmax / (1 + Kcurve * curvature),
        Vmin,
        Vmax
    )
```

### Longitudinal control

```text
speed_error = target_speed - current_speed
```

Positive error → throttle.

Negative error → brake.

A modest PID controller is acceptable.

Avoid sophisticated tire-model optimization during this sprint.

---

# 8. Opponent Personality

The two AI racers should not behave identically.

Use configuration, not separate code.

### AI 01 — Conservative

```text
Skill           0.75
Aggression      0.40
CornerSpeed     0.88
ReactionNoise   low
```

### AI 02 — Aggressive

```text
Skill           0.88
Aggression      0.70
CornerSpeed     0.96
ReactionNoise   moderate
```

These values need tuning, not machine learning.

Initial overtaking may be implemented through limited racing-line lateral offsets.

Perfect racecraft is explicitly **not required**.

---

# 9. Circuit Construction

Do not model downtown Cleveland.

The airport geometry is the advantage.

Build only what the camera needs.

### Required near-field geometry

- driving surface
- runway/taxiway texture differentiation
- grass/infield
- Lake Erie boundary
- concrete barriers
- tire barriers where appropriate
- cones
- starting grid
- start/finish markings
- pit lane visual treatment
- minimal airport infrastructure

### Required far-field geometry

- Cleveland skyline silhouette
- horizon
- Lake Erie
- atmospheric perspective

### Explicitly unnecessary

- interiors
- detailed airport terminals
- pedestrians
- fully modeled city blocks
- individual downtown storefronts
- enterable buildings
- active commercial airport simulation

---

# 10. Skyline Strategy

The baseline demo must work **without network connectivity**.

Therefore the minimum supported skyline should be generated or pre-baked from legally usable geometry/data.

Recommended hierarchy:

```text
Tier 0 — offline procedural / low-poly skyline
Tier 1 — OSM building-volume skyline
Tier 2 — optional Cesium enhancement
```

The existing roadmap already identifies Cesium/3D Tiles as a future visual-polish integration. 

Do not make a Cesium API token or network connection mandatory for the demo executable.

---

# 11. CARLA Boundary

CARLA remains relevant to the larger raceGPS architecture, but **CARLA shall not become a blocking runtime dependency for this sprint**.

Current raceGPS documentation deliberately describes its production semantic pipeline as:

```text
OSM
→ pure-Python semantic compiler
→ OpenDRIVE
→ citypack
→ UE5
```

rather than requiring CARLA. 

That is an asset.

Preserve it.

### Cleveland sprint

CARLA may be used for:

- reference assets
- interoperability experiments
- OpenDRIVE validation
- prop-development research
- future city-streaming work

But the Cleveland demo must launch without a separate CARLA server.

### Post-demo R&D target

After the showcase succeeds, open a dedicated workstream:

**Dynamic World Materialization**

```text
GPS position
      │
      ▼
semantic geographic window
      │
      ├── roads
      ├── structures
      ├── terrain
      ├── traffic semantics
      └── visual assets
      │
      ▼
local streaming/compiler layer
      │
      ▼
UE runtime world cells
```

That is where the larger CARLA-derived asset/runtime problem should be attacked.

Do not solve it inside this demo sprint.

---

# 12. Suggested Repository Additions

```text
citypacks/
└── cleveland/
    └── burke_gp_1997/
        ├── manifest.json
        ├── cleveland_burke_gp.xodr
        ├── racing_line.json
        ├── checkpoints.json
        └── metadata.json

apps/unreal-akron-beta/
└── Source/raceGPSAkronBeta/
    ├── Public/
    │   ├── RaceAIDriverController.h
    │   ├── RacingLineComponent.h
    │   └── RaceGridManager.h
    │
    └── Private/
        ├── RaceAIDriverController.cpp
        ├── RacingLineComponent.cpp
        └── RaceGridManager.cpp

docs/
├── CLEVELAND_DEMO.md
├── CLEVELAND_TRACK_PROVENANCE.md
└── CLEVELAND_DEMO_TEST_PLAN.md
```

Do not rename the existing Akron application during this sprint unless the existing hard-coded naming becomes a technical blocker.

Minimal-impact modifications are preferred.

---

# 13. Race Grid Manager

Implement a small race-grid layer supporting at least:

```text
Grid Slot 1
Grid Slot 2
Grid Slot 3
```

The system shall support arbitrary assignment of:

```text
PLAYER
AI
AI
```

All vehicles must exist before countdown completion.

No opponent may receive movement input before `GO`.

---

# 14. Position Calculation

For a three-car demonstration, race position should be calculated from:

```text
completed_laps
+
normalized_spline_progress
```

Conceptually:

```text
RaceProgress =
    LapIndex * TrackLength
    + CurrentSplineDistance
```

Sort descending.

Do not determine position solely by Euclidean distance to the finish line.

That fails on folded circuits.

---

# 15. Recovery

AI racers must recover from common failure states.

Required detection:

```text
speed < threshold
AND
throttle > threshold
AND
time > recovery_delay
```

or:

```text
distance_from_racing_line > maximum_cross_track_distance
```

Recovery order:

1. attempt steering/brake correction;
2. reverse if necessary;
3. after timeout, reset to nearest safe spline position.

Teleport recovery is acceptable **only as a fault recovery mechanism**, never normal driving behavior.

---

# 16. Visual Quality Bar

The demo should communicate:

**“real place, real racing game.”**

Not:

**“procedural-engine test scene.”**

Priority:

```text
1. vehicle feel
2. camera feel
3. track scale
4. lighting
5. skyline silhouette
6. barriers / track dressing
7. VFX
8. secondary details
```

A good chase camera and believable motion perception are more important than modeling dozens of buildings.

---

# 17. Audio

Minimum audio:

- engine RPM response
- tire-slip sound
- collision sound
- countdown
- start cue
- finish cue
- restrained ambient environment

No soundtrack is required for engineering acceptance.

A soundtrack may be added for the public video separately.

---

# 18. Required Demo HUD

Display only information that reinforces the proof.

```text
POSITION      2 / 3
LAP           1 / 1
TIME          00:41.228
SPEED         117 MPH
```

Optional:

```text
BEST
DELTA
```

Developer mode may additionally expose:

```text
FPS
vehicle speed
AI steering
AI target speed
cross-track error
spline progress
```

---

# 19. Sprint Milestones

## M0 — Baseline Verification

Confirm current master:

- generates/builds successfully;
- launches UE project;
- player vehicle works;
- existing race loop works;
- existing tests remain green.

**Gate:** no Cleveland work begins until the existing baseline is documented.

---

## M1 — Circuit Data

Create canonical Cleveland circuit definition.

Deliver:

- circuit coordinates
- course centerline
- OpenDRIVE representation
- racing spline
- 10 turn markers
- start/finish
- checkpoint sequence
- provenance documentation

**Gate:** developer can render and inspect the entire circuit.

---

## M2 — Player Lap

Spawn existing player Chaos vehicle.

Player must:

- start correctly;
- complete entire circuit;
- trigger checkpoints sequentially;
- cross finish;
- produce valid lap time.

**Gate:** one complete manual lap.

---

## M3 — One AI Chaos Racer

Implement `ARaceAIDriverController`.

AI must drive the same circuit under Chaos physics.

**Gate:** one AI completes three consecutive autonomous laps.

---

## M4 — Three-Car Race

Spawn:

```text
1 human
2 AI
```

Add:

- synchronized countdown
- grid start
- race progress
- finishing order

**Gate:** player can race both AI vehicles through one complete event.

---

## M5 — Environment

Add:

- Lake Erie
- runway materials
- barriers
- cones
- track furniture
- Cleveland skyline
- coherent lighting

**Gate:** recognizable Cleveland/Burke presentation from gameplay camera.

---

## M6 — Polish

Add:

- camera tuning
- motion perception
- engine audio
- tire audio
- countdown presentation
- finish presentation
- basic VFX

**Gate:** internal capture candidate.

---

## M7 — Reliability

Run automated/manual test matrix.

Minimum:

- 10 cold launches
- 10 races
- AI lap completion tests
- collision tests
- reset tests
- restart tests
- frame-time capture

**Gate:** no P0 or P1 defects.

---

## M8 — Contributor Build

A clean machine must be able to:

```text
clone
configure
build
launch
race
```

using documented commands.

**Gate:** successful clean-machine reproduction.

---

## M9 — Showcase Capture

Produce:

- 60–90 second hero video
- uninterrupted race footage
- 3–5 screenshots
- architecture graphic
- README demo section
- contributor call

**Gate:** material is publishable.

---

# 20. Acceptance Criteria

The demo is **DONE** only when all of the following are true.

### Gameplay

- Human controls one Chaos vehicle.
- Two autonomous opponents use Chaos vehicle physics.
- Three cars begin from a synchronized grid.
- All vehicles can complete the circuit.
- One-lap race has deterministic start and finish.
- Position tracking works.
- Restart works.

### Circuit

- Uses the documented Cleveland Burke layout.
- Contains ten identifiable turns.
- Start/finish corresponds to reference layout.
- Track scale is geographically credible.

### Presentation

- Cleveland skyline visible.
- Lake Erie visually readable.
- Environment no longer resembles an empty Unreal test map.
- HUD communicates race state immediately.
- Demo footage is visually publishable.

### Reliability

- No required external CARLA server.
- No required cloud backend.
- No blocker crash during standard demo sequence.
- AI completion rate ≥95% during test runs.

### Reproducibility

A new developer can reach the demo from README instructions without undocumented steps.

---

# 21. Non-Goals

The following are specifically deferred:

- arbitrary-city generation UI
- city-scale streaming
- full CARLA runtime integration
- multiplayer
- police pursuit
- pedestrian AI
- weather simulation
- garage customization
- economic systems
- open-world exploration
- account backend
- Steam integration
- photorealistic downtown Cleveland
- advanced racing AI
- machine-learning opponents
- procedural generation of every building

Do not allow any of these items to consume sprint capacity.

---

# 22. Performance Targets

Initial engineering target:

```text
Resolution              1920 × 1080
Target frame rate       60 FPS
Minimum acceptable      30 FPS
Race vehicles           3
Active race circuit     1
Network requirement     none
CARLA server            none
Cloud requirement       none
```

All performance measurements must identify reference hardware.

Do not publish unsupported generalized performance claims.

---

# 23. Test Matrix

| Test | Requirement |
|---|---|
| Launch | Application reaches race menu |
| Player spawn | Player appears in correct grid slot |
| AI spawn | Two AI cars appear |
| Countdown | No premature start |
| Steering | AI follows racing line |
| Cornering | AI brakes before high-curvature turns |
| Collision | Chaos collision remains active |
| Recovery | AI can resume after minor spin |
| Checkpoints | Sequential validation |
| Position | Correct `1/3`, `2/3`, `3/3` |
| Finish | Correct finishing order |
| Restart | Clean session reset |
| Repeatability | 10 consecutive demo executions |
| Clean build | New checkout builds using documentation |

---

# 24. Engineering Guardrails

1. **Preserve working systems.**
2. **Prefer extending existing classes to replacing architecture.**
3. **Do not create a second race engine.**
4. **Do not introduce CARLA as a mandatory service.**
5. **Do not build city-scale streaming during this sprint.**
6. **Do not represent kinematic traffic actors as physics-based opponents.**
7. **Keep historical layout metadata explicit.**
8. **Keep external dependencies optional whenever possible.**
9. **Every new subsystem receives a test or deterministic verification path.**
10. **A visually impressive but unreproducible demo does not pass.**
11. **A technically correct but visually incomprehensible demo does not pass.**
12. **When scope conflicts arise, protect the playable three-car race first.**

---

# 25. Historical / Branding Guardrail

The project may accurately describe the circuit as inspired by or recreating Cleveland's historic Burke Lakefront Airport racing course.

Do not imply sponsorship, endorsement, licensing, or affiliation with historical race sponsors.

For public demo branding, prefer:

> **raceGPS: Cleveland Historic Circuit**

rather than presenting the project as an official revival of a particular sponsored event.

---

# 26. Definition of Success

A stranger who has never heard of raceGPS watches the demo for sixty seconds and understands:

> “They took a real historic racing location in Cleveland, turned it into a playable Unreal racing environment, put a real physics car and AI racers on it, and they have a system intended to eventually do this with locations anywhere.”

A developer watches the same demonstration and thinks:

> “I understand the architecture. I can clone this. I know what subsystem I could contribute to.”

That is the sprint objective.

---

# 27. Post-Demo Continuation

Once this milestone passes, the next engineering investigation should move directly toward the technically harder raceGPS thesis:

## Dynamic Geographic World Materialization

The research question becomes:

> **How quickly can raceGPS convert an arbitrary geographic window around the player into semantically correct, visually credible, drivable UE world cells without visible generation stalls?**

That workstream should evaluate:

- OSM semantic ingestion
- OpenDRIVE generation
- geographic chunking
- World Partition
- local asset caching
- procedural buildings
- CARLA-compatible assets
- Cesium/3D Tiles
- predictive streaming
- asynchronous generation
- persistent citypacks
- player-created local courses
- GPS/location authorization rules

But only **after the Cleveland demo establishes the product experience.**

---

# FINAL ENGINEERING ORDER

**Freeze expansion.**

**Build Cleveland.**

**Make three cars race.**

**Make it look convincing.**

**Make it reproducible.**

**Record it.**

**Publish it.**

Then attack the arbitrary-city problem.