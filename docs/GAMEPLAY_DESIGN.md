# Gameplay Design

## Physics Feel

Arcade street-racing: fast acceleration, forgiving steering, controlled drift, boost, dramatic camera, quick recovery, clean checkpoint timing.

The vehicle uses **Chaos Vehicles** (not PhysX) with custom tuning data:
- **Mass:** 1500 kg
- **Max Speed:** 220 km/h
- **Acceleration:** High — 0-100 km/h in ~4s
- **Grip:** Medium-low for drift-friendly handling
- **Drift Factor:** Tunable per vehicle class
- **Substepping:** 60 Hz for stable physics at high speeds

---

## Cruise Sprint Mode

The primary game mode for the Akron Beta. A checkpoint-to-checkpoint time trial on real-world roads.

### Race States

```
Loading ──► Countdown ──► Racing ──► Finished
   │            │            │           │
   │       Tutorial      Timer      Post-Race
   │       (1st time)    Running    Stats + Medal
   │                                  + Leaderboard
```

| State | Duration | Player Input | UI |
|-------|----------|--------------|-----|
| **Loading** | 2-3s | None | LoadingScreenWidget |
| **Countdown** | 3.0s | None (tutorial if 1st race) | NeonHUD + countdown |
| **Racing** | Variable | Full vehicle control | NeonHUD (timer, speed, checkpoints) |
| **Finished** | Until dismissed | Menu navigation | PostRaceStatsWidget |

### Scoring Formula

```
Base Time                = raw elapsed time (seconds)
Collision Penalty        = +2.0s per collision
Missed Checkpoint Penalty = +5.0s per missed checkpoint
Clean Driving Bonus      = -1.0s (if zero collisions)

Final Time = Base Time + Collision Penalty + Missed Checkpoint Penalty - Clean Driving Bonus
```

### Medal Thresholds

| Medal | Time | Color |
|-------|------|-------|
| **Gold** | ≤ 120s | #FFD700 |
| **Silver** | ≤ 150s | #C0C0C0 |
| **Bronze** | ≤ 200s | #CD7F32 |
| **No Medal** | > 200s | — |

### Checkpoints

- **Placement:** Automatically generated along route spline by semantic compiler
- **Visualization:** Glowing gate actors with overlap delegates
- **Progression:** Must be hit in order. Missing a checkpoint adds penalty but does not block progression.
- **Final Gate:** Crossing the last checkpoint triggers `FinishRace()`

### Collisions

- **Detection:** `UPrimitiveComponent::OnComponentHit` on vehicle mesh
- **Penalty:** +2.0s per collision, regardless of severity
- **Clean Driving Bonus:** -1.0s awarded only if **zero** collisions across the entire race
- **Telemetry:** Collision count displayed in post-race stats

---

## Replay / Ghost System

### Recording

- **Frequency:** 30 FPS
- **Data per frame:** Position (Vector), Rotation (Quat), Velocity (Vector)
- **Format:** Compressed JSON array
- **Storage:** `Saved/replays/<RouteId>.json`

### Playback

- **GhostVehicle:** A separate pawn that follows recorded waypoints
- **Interpolation:** Linear interpolation between recorded frames for smooth playback
- **Trigger:** Best replay loads automatically at race start and plays with a configurable delay
- **Delay:** Default 2.0s after countdown to avoid start-line clutter

---

## Tutorial

5-step interactive onboarding shown on the player's first race.

| Step | Title | Instruction | Input Required | Auto-Advance |
|------|-------|-------------|----------------|--------------|
| 1 | Getting Moving | Use W and S to accelerate and brake | Throttle | No |
| 2 | Steering | Use A and D to steer left and right | Steer | No |
| 3 | Drifting | Press Space to use the handbrake | Handbrake | No |
| 4 | Checkpoints | Drive through the glowing gates | — | 5 seconds |
| 5 | Good Luck! | Complete the route as fast as you can | — | 3 seconds |

- Steps 1-3 wait for the player to perform the action.
- Steps 4-5 auto-advance after their delay.
- The tutorial can be skipped at any time via the Skip button.

---

## Achievements

| ID | Title | Description | Unlock Condition |
|----|-------|-------------|------------------|
| first_race | First Steps | Complete your first race | Finish any race |
| gold_medalist | Gold Rush | Earn 3 gold medals | Earn Gold on 3 different routes |
| clean_driver | Clean Driver | Finish a race with zero collisions | 0 collisions in a finished race |
| speed_demon | Speed Demon | Reach 200 km/h | Hit 200 km/h at any time |
| checkpoint_master | Checkpoint Master | Hit every checkpoint in a race | 0 missed checkpoints |
| explorer | Explorer | Drive 50km total | Cumulative distance across all races |
| night_racer | Night Racer | Complete a race at night | Finish while DayNightCycle is night |
| traffic_dodger | Traffic Dodger | Avoid 100 traffic vehicles | Near-miss 100 traffic cars |
| replay_watcher | Ghost Buster | Beat your own ghost | Finish faster than your best replay |
| cartographer | Cartographer | Compile a new city | Run semantic compiler for a new city |

- All achievements persist to `Saved/achievements.json`
- Progress-based achievements (gold_medalist, explorer, traffic_dodger) accumulate across sessions

---

## Leaderboards

- **Scope:** Per-route (each route has its own leaderboard file)
- **Entries:** Player name, time, medal, date, vehicle used, collision count
- **AI Seeds:** Default entries for Gold, Silver, and Bronze thresholds pre-populated
- **Player Rank:** Calculated by inserting the player's time and counting how many entries are faster
- **Persistence:** JSON files in `Saved/leaderboards/`

---

## Traffic

- **Spawning:** Dynamic radius-based spawning around the player (200m radius)
- **Culling:** Traffic vehicles despawn when the player moves >250m away
- **AI:** Simple road-following behavior with basic lane adherence
- **Density:** Configurable max concurrent vehicles (default 20)

---

## Day / Night Cycle

- **Sun:** Rotating directional light with time-of-day angle
- **Sky:** HSV-adjusted atmospheric fog and sky sphere
- **Speed:** Configurable cycle duration (default 10 real minutes = 24 game hours)
- **Night Racing:** Achievements trigger when the sun angle indicates night

---

## Controls Reference

| Action | Keyboard | Gamepad | Context |
|--------|----------|---------|---------|
| Throttle | W | RT | Racing |
| Brake / Reverse | S | LT | Racing |
| Steer Left | A | Left Stick Left | Racing |
| Steer Right | D | Left Stick Right | Racing |
| Handbrake | Space | A | Racing |
| Reset Vehicle | R | — | Racing |
| Toggle Camera | C | — | Racing |
| Pause | P / Esc | Menu | Any |
| Developer Console | `~` (Tilde) | — | Any |
