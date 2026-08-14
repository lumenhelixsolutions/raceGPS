# raceGPS — UI Design Document

> Version: 0.1.0-beta  
> Engine: Unreal Engine 5.7  
> Target Resolution: 1920×1080 (16:9), scalable to 21:9 and 4:3  
> Design Language: Dark city-at-night, neon GPS green, pursuit red, electric route blue

---

## Table of Contents

1. [Main Menu](#1-main-menu)
2. [Garage Screen](#2-garage-screen)
3. [HUD (In-Race)](#3-hud-in-race)
4. [Pause Menu](#4-pause-menu)
5. [End Screen](#5-end-screen)
6. [Settings Screen](#6-settings-screen)
7. [Navigation Flow](#7-navigation-flow)

---

## 1. Main Menu

### 1.1 Title Screen

- **Background:** Full-screen video loop of Akron city timelapse (day→night transition, 15s loop). Darkened with a radial vignette (80% opacity black) to ensure text legibility.
- **Logo:** "raceGPS" wordmark centered top-third. Subtle pulse glow (Neon GPS Green, 2s loop).
- **Tagline:** "Drive the real world." below logo, Electric Route Blue, 18px.

### 1.2 Menu Options (Vertical Stack)

Centered at 55% screen height. Buttons are 400×60px with 16px gap.

| Option | Hotkey | Action |
|--------|--------|--------|
| **PLAY** | Enter / Gamepad A | Opens save slot selector, then route selection |
| **GARAGE** | G | Opens garage screen |
| **SETTINGS** | S | Opens settings screen |
| **CREDITS** | C | Opens credits scroll |
| **QUIT** | Q / Esc | Exit to desktop with confirmation dialog |

- **Focus State:** Button background expands 10px horizontally, Neon GPS Green border (2px), 0.15s ease-out.
- **Hover State:** Text shifts right 8px, arrow icon (`>`) fades in.

### 1.3 Save Slot Selector

Modal overlay triggered by "PLAY". Three slot cards in a row (320×180px each).

- **Slot Card Content:**
  - Slot number (01, 02, 03) — top-left, mono font
  - Player name — center, heading font
  - Total distance driven — bottom-left, body font
  - Current level + XP bar — bottom-right
  - "New Game" placeholder for empty slots (ghost text + `+` icon)
- **Interaction:** Click/accept loads slot and transitions to Route Selection (not detailed in this doc; uses same card pattern).
- **Delete Slot:** Hold `Shift + Click` on slot → confirmation dialog.

### 1.4 Player Profile Display

Anchored top-right corner (safe zone 48px from edges). Compact horizontal bar (400×80px).

- **Elements:**
  - Avatar circular frame (64×64px) — default silhouette, customizable in garage
  - Player name (max 16 chars)
  - Level badge (circular, Neon GPS Green ring)
  - Total distance (km, mono font)
  - XP progress bar (thin, 4px height, Electric Route Blue fill)

---

## 2. Garage Screen

### 2.1 Layout

Split-screen: Left 60% = 3D vehicle viewport. Right 40% = customization panel.

- **Background:** Cinematic showroom — dark floor with grid reflection, rotating environmental light rig.
- **3D Vehicle Preview:**
  - Render Target displayed in `UImage` widget
  - Mouse drag = orbit rotation (Yaw ±180°, Pitch -10° to 60°)
  - Mouse wheel = zoom (FOV 40° to 90°)
  - Double-click = reset camera
  - Auto-rotate when idle (5s delay, 2°/sec)

### 2.2 Part Selector Tabs

Horizontal tab bar at top of right panel (6 tabs).

| Tab | Icon | Contents |
|-----|------|----------|
| **Body Kit** | Car silhouette | Bumpers, hood, spoiler, side skirts |
| **Paint** | Paint bucket | Primary, secondary, trim colors; metallic/flat finish |
| **Wheels** | Wheel | Rim style, rim color, tire type |
| **Aero** | Wind | Front splitter, rear wing, diffuser, canards |
| **Lighting** | Headlight | Headlight color, underglow, neon strips |
| **Decals** | Sticker | Layer-based decal editor (position, rotate, scale) |

- **Tab Active State:** Bottom border 3px Neon GPS Green, text brightened.
- **Tab Inactive State:** 50% opacity, no border.

### 2.3 Performance Preset Selector

Dropdown (combo box) below tabs. Presets: "Stock", "Grip", "Drift", "Speed", "Balanced".
- Selecting a preset auto-adjusts all tunable parts to preconfigured values.
- Visual confirmation: stats bars animate to new values (0.4s ease-out).

### 2.4 Build Name + Save

- **Text Input:** Max 24 characters. Validates alphanumeric + spaces.
- **Save Button:** Primary style. On save:
  - Validates at least one part changed.
  - Writes to `Saved/builds/<BuildName>.json`.
  - Shows toast: "Build Saved ✓" (Success Green, 2s fade).

### 2.5 Performance Stats Display

Vertical stat bars (6 stats, 120px wide each).

| Stat | Range | Color |
|------|-------|-------|
| Top Speed | 0–300 km/h | Electric Route Blue |
| Acceleration | 0–100 (0-100 km/h time inverse) | Neon GPS Green |
| Handling | 0–100 | Electric Route Blue |
| Drift | 0–100 | Pursuit Red |
| Grip | 0–100 | Neon GPS Green |
| Weight | 800–2000 kg | Neutral Gray |

- **Bar Style:** Filled horizontal bar (8px height), rounded caps. Background = `Surface/800`.
- **Value Update:** Smooth lerp on change (0.3s).

### 2.6 Cost / Tier Indicators

- **Tier Badge:** Bronze / Silver / Gold / Platinum. Determined by total build value.
- **Cost Display:** If parts have in-game currency cost, show total (mono font, right-aligned).
- **Unlock Status:** Locked parts show padlock icon + required level.

---

## 3. HUD (In-Race)

> Minimal, fast, tactical. All HUD elements use `Translucent` blend mode with `HitTestInvisible` where possible.

### 3.1 Speedometer

- **Position:** Bottom-center, anchored with 32px bottom margin.
- **Analog Ring:** 240px diameter arc (270° sweep, bottom gap). Needle rotates 0–300 km/h.
  - Needle color: White → Neon GPS Green (0–80%) → Pursuit Red (80–100%).
- **Digital Readout:** Centered inside ring, 72px mono font, updates every frame.
- **Unit Label:** "km/h" below digital, 14px, 60% opacity.

### 3.2 Minimap

- **Position:** Top-right, 48px from edges.
- **Shape:** Circular, 200px diameter.
- **Contents:**
  - Simplified road mesh rendered to `RenderTarget2D` (top-down orthographic).
  - Route ribbon (Electric Route Blue, 4px line, animated dash pattern).
  - Player pip (Neon GPS Green triangle, rotated to heading).
  - Checkpoint pips (white diamonds, next checkpoint pulses).
  - Ghost pip (Pursuit Red, 60% opacity diamond).
  - Traffic pips (small gray dots).
- **Border:** 2px `Surface/600` ring.
- **Zoom:** Dynamic based on speed (slower = closer zoom).

### 3.3 Lap Timer + Best Lap

- **Position:** Top-center.
- **Current Lap:** `00:00.000` format, 48px mono font, Neon GPS Green.
- **Best Lap:** Below current, 24px, `PB 00:00.000`, Electric Route Blue.
- **Delta:** If current lap is faster/slower than best at last checkpoint, show `+0.42` (red) or `-0.38` (green), 20px.

### 3.4 Position Indicator

- **Position:** Top-left, below minimap vertical alignment.
- **Display:** Large ordinal — `1st`, `2nd`, `3rd`, etc.
- **Style:** 56px bold, white with subtle drop shadow.
- **Total Racers:** `/ 8` suffix, 24px, 60% opacity.
- **Note:** For Cruise Sprint (solo), this shows rank against ghost + AI seeds.

### 3.5 Drift Score + Multiplier

- **Position:** Bottom-left, above speedometer horizontal alignment.
- **Score:** Rolling counter, 36px mono font. Resets on race finish.
- **Multiplier:** `×2.5` badge, 28px, Pursuit Red background pill shape.
- **Combo Bar:** Thin bar below score (fills during drift, empties 2s after drift ends).
- **Pop Animation:** Score increments trigger a 1.05x scale bounce (0.1s).

### 3.6 Checkpoint Distance

- **Position:** Center-screen, 120px from top.
- **Display:** `324 m` with downward chevron icon.
- **Style:** 32px mono, white, fades out below 50m (arrival imminent).
- **Pulse:** Icon pulses at 1Hz when within 100m.

### 3.7 Weather Indicator

- **Position:** Top-right, below minimap (16px gap).
- **Icon:** Sun / Cloud / Rain / Night.
- **Label:** "CLEAR", "RAIN", "NIGHT", etc., 14px uppercase.
- **Color:** White (day), Electric Route Blue (night), muted gray (overcast).

### 3.8 Gear Indicator

- **Position:** Right of speedometer, 24px gap.
- **Display:** Large `4` (current gear), 64px mono font.
- **Shift Flash:** Background flashes Neon GPS Green on upshift, white on downshift (0.15s).
- **Reverse:** Displays `R` in Pursuit Red.

### 3.9 RPM Gauge

- **Position:** Bottom-center, behind/above speedometer (overlapping layout).
- **Style:** Horizontal bar (400×12px), rounded.
  - Fill: Electric Route Blue (0–70%) → Neon GPS Green (70–90%) → Pursuit Red (90–100%).
  - Background: `Surface/800`.
- **Redline:** Last 10% flashes at 2Hz when exceeded.
- **Shift Indicator:** Small triangle arrow at optimal shift RPM (per vehicle tune).

### 3.10 Nitro / Boost Meter

- **Position:** Bottom-center, above RPM gauge (stacked vertically).
- **Style:** Segmented bar (10 segments, 40×16px each, 4px gap).
  - Fill: Pursuit Red (segments fill left-to-right).
  - Empty: `Surface/700` outline only.
- **Activation:** Full bar + press `Shift` (or Gamepad B) drains bar over 2s.
- **Recharge:** Drift score and clean driving refill segments.

---

## 4. Pause Menu

### 4.1 Layout

Full-screen dim overlay (`Surface/900` at 85% opacity). Centered vertical button stack.

### 4.2 Options

| Option | Action |
|--------|--------|
| **RESUME** | Unpause, fade HUD back in (0.3s) |
| **RESTART** | Confirmation dialog → reload level |
| **SETTINGS** | Push settings widget (retains pause state) |
| **QUIT TO MENU** | Confirmation dialog → open Main Menu |

- **Buttons:** 360×56px, Ghost style (transparent bg, white border).
- **Hotkeys:** `Esc` / `P` = Resume (toggle). Arrow keys / D-pad navigate.

### 4.3 Current Lap Time Snapshot

- **Position:** Bottom-center, 48px above button stack.
- **Display:** `Current: 01:23.456` — frozen at pause moment.
- **Style:** 32px mono, 70% opacity white.

---

## 5. End Screen

### 5.1 Layout

Three-column layout:
- Left 30% = stats + medals
- Center 40% = vehicle build + ghost comparison
- Right 30% = XP/level + actions

Background: blurred race screenshot (captured at finish line) with dark overlay.

### 5.2 Final Time + Best Lap

- **Final Time:** Largest text on screen, `02:14.892`, 96px mono, white.
- **Best Lap:** `Best Lap: 00:42.103`, 32px, Electric Route Blue.
- **Medal:** Large medal icon below time (Gold / Silver / Bronze / None). Pulses once on appear (0.5s scale 0→1, elastic ease-out).

### 5.3 Drift Score Total

- **Display:** `128,450` with "DRIFT SCORE" label.
- **Position:** Left column, below medal.
- **Animation:** Counts up from 0 over 1.5s (ease-out).

### 5.4 Vehicle Build Display

- **Position:** Center column.
- **Content:**
  - 3D vehicle thumbnail (snapshot from garage viewport).
  - Build name (e.g., "Night Stalker").
  - Tier badge.
  - Mini stat summary (3 bars: Speed, Accel, Handling).

### 5.5 Ghost Comparison

- **Position:** Center column, below build display.
- **Display:** `Ghost Δ -1.24s` (green, faster) or `+2.18s` (red, slower).
- **Graph:** Mini sparkline showing time delta at each checkpoint (8px height, 200px wide).

### 5.6 XP / Level Progress

- **Position:** Right column, top.
- **Level:** Current level large, `Level 12`.
- **XP Bar:** Fills from pre-race value to new value (1s ease-out).
- **+XP Breakdown:**
  - Base completion: +150 XP
  - Medal bonus: +100/50/25 XP
  - Drift bonus: +1 XP per 100 drift score
  - Clean drive: +50 XP
- **Level Up:** If threshold crossed, show "LEVEL UP!" banner (Neon GPS Green, 0.6s slide-in).

### 5.7 Next Race Button

- **Position:** Right column, bottom.
- **Style:** Primary button, 320×64px.
- **Action:** Loads next unlocked route (or random if all complete).

### 5.8 Share Screenshot Button

- **Position:** Below Next Race.
- **Style:** Secondary button with camera icon.
- **Action:**
  - Captures `FScreenshotRequest`.
  - Saves to `Saved/Screenshots/`.
  - Copies path to clipboard.
  - Toast: "Screenshot saved!"

---

## 6. Settings Screen

> Tabbed interface. Left vertical tab list (200px wide), right content panel.

### 6.1 Graphics Quality

| Setting | Options | Default |
|---------|---------|---------|
| **Overall Quality** | Low / Medium / High / Epic | Epic |
| **Anti-Aliasing** | Off / FXAA / TSR | TSR |
| **Shadows** | Low / Medium / High / Epic | High |
| **Effects** | Low / Medium / High / Epic | High |
| **View Distance** | Near / Medium / Far / Epic | Epic |
| **Foliage** | Low / Medium / High / Epic | Medium |
| **Post-Processing** | Low / Medium / High / Epic | High |

- **Presets:** Selecting "Low/Medium/High/Epic" auto-sets sub-options.
- **Apply:** Requires engine restart for some changes; dialog warns user.

### 6.2 Audio

| Setting | Range | Default |
|---------|-------|---------|
| **Master** | 0–100% | 80% |
| **Music** | 0–100% | 70% |
| **SFX** | 0–100% | 90% |
| **Engine** | 0–100% | 85% |
| **UI Sounds** | On / Off | On |

- **Sliders:** Custom styled (8px track, 16px thumb, Neon GPS Green fill).
- **Test Buttons:** Small speaker icon next to each slider plays a test sound.

### 6.3 Controls

| Setting | Options |
|---------|---------|
| **Steering Sensitivity** | 0.5× – 2.0× (0.1 steps) |
| **Throttle Sensitivity** | 0.5× – 2.0× |
| **Brake Sensitivity** | 0.5× – 2.0× |
| **Invert Steering** | On / Off |
| **Keyboard Remap** | Opens remapping overlay |

- **Remap Overlay:** Grid of actions (Throttle, Brake, Steer L, Steer R, Handbrake, Reset, Pause). Click action → press new key → validates no conflict → saves to `Saved/Config/Input.ini`.
- **Gamepad:** Shows current gamepad glyph icons (via `Enhanced Input` action mapping).

### 6.4 Display

| Setting | Options | Default |
|---------|---------|---------|
| **Resolution** | Native monitor resolutions | Native |
| **Display Mode** | Fullscreen / Windowed / Borderless | Borderless |
| **V-Sync** | On / Off | Off |
| **Frame Rate Limit** | 30 / 60 / 120 / 144 / Unlimited | 144 |
| **FOV** | 70° – 110° (1° steps) | 90° |
| **HUD Scale** | 0.8× – 1.5× (0.05 steps) | 1.0× |
| **Safe Zone** | 0% – 10% (1% steps) | 5% |

- **Preview:** Small viewport thumbnail updates FOV in real-time (uses cached scene capture).

---

## 7. Navigation Flow

```
[Boot] ──► [Loading Screen] ──► [Main Menu]
                                    │
            ┌───────────────────────┼───────────────────────┐
            ▼                       ▼                       ▼
      [Save Slot Selector]    [Garage]              [Settings]
            │                       │                       │
            ▼                       ▼                       ▼
      [Route Selection]       [Build Saved]          [Apply / Revert]
            │
            ▼
      [Loading Screen]
            │
            ▼
      [Countdown] ──► [Racing] ──► [Pause Menu]
            │            │              │
            │            │              ├─► Resume
            │            │              ├─► Restart
            │            │              └─► Quit to Menu
            │            │
            └────────────┴─► [End Screen]
                                │
                                ├─► Next Race
                                ├─► Garage
                                ├─► Share Screenshot
                                └─► Main Menu
```

---

## Appendix: Safe Zones & Anchoring

- **Action Safe:** 97% of screen (3% margin on all sides).
- **Title Safe:** 90% of screen (5% margin on all sides).
- **All HUD elements anchor to corners/center with 48px base margin.**
- **Safe zone setting in Display settings scales margins 0–10%.**

## Appendix: Input Modes

| Screen | Input Mode | Mouse Visible |
|--------|-----------|---------------|
| Main Menu | UI Only | Yes |
| Garage | UI Only | Yes |
| HUD | Game Only | No |
| Pause | UI Only | Yes |
| End Screen | UI Only | Yes |
| Settings | UI Only | Yes |
