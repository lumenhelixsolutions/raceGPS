# raceGPS — UI Style Guide

> Version: 0.1.0-beta  
> Engine: Unreal Engine 5.7 UMG  
> Theme: Dark city-at-night, neon GPS green, pursuit red, electric route blue

---

## 1. Color Palette

### 1.1 Brand Colors

| Token | Hex | RGB | Usage |
|-------|-----|-----|-------|
| **Neon GPS Green** | `#00FF85` | `0, 255, 133` | Primary actions, positive states, XP/level, shift flash |
| **Pursuit Red** | `#FF2A2A` | `255, 42, 42` | Danger, warnings, drift multiplier, nitro, collisions |
| **Electric Route Blue** | `#00CCFF` | `0, 204, 255` | Information, route lines, best lap, minimap ribbon |
| **Caution Amber** | `#FFB800` | `255, 184, 0` | Caution, medium priority, alerts |

### 1.2 Neutral / Surface Colors

| Token | Hex | Usage |
|-------|-----|-------|
| **Surface/50** | `#F5F5F5` | Primary text on dark backgrounds |
| **Surface/100** | `#E0E0E0` | Secondary text, labels |
| **Surface/200** | `#BDBDBD` | Disabled text, placeholders |
| **Surface/400** | `#7A7A7A` | Borders, dividers |
| **Surface/600** | `#4A4A4A` | Inactive tab borders, minimap ring |
| **Surface/700** | `#2E2E2E` | Card backgrounds, panel fills |
| **Surface/800** | `#1A1A1A` | Elevated surfaces, stat bar backgrounds |
| **Surface/900** | `#0D0D0D` | Base background, full-screen overlays |
| **Pure Black** | `#000000` | Deep background, vignette |

### 1.3 Semantic Colors

| State | Color | Usage |
|-------|-------|-------|
| **Success** | `#00FF85` (Neon GPS Green) | Save confirmation, clean drive, faster delta |
| **Danger** | `#FF2A2A` (Pursuit Red) | Delete, quit without saving, collision penalty |
| **Warning** | `#FFB800` (Caution Amber) | Unsaved changes, low health/rarely used |
| **Info** | `#00CCFF` (Electric Route Blue) | Tooltips, ghost data, route info |

### 1.4 Gradient Presets

- **Neon Glow (text):** `LinearGradient` from `#00FF85` to `#00CCFF`, angle 135°. Used for highlighted headings.
- **Panel Depth:** `LinearGradient` from `#1A1A1A` (top) to `#0D0D0D` (bottom), angle 180°. Used for settings panels.
- **Danger Pulse:** `LinearGradient` from `#FF2A2A` to `#FFB800`, angle 0°. Used for critical warnings.

---

## 2. Typography

### 2.1 Font Families

| Role | Font | Fallback | Usage |
|------|------|----------|-------|
| **Headings** | `Rajdhani` (SemiBold, 600) | `Arial Black` | Screen titles, section headers, menu options |
| **Body** | `Inter` (Regular, 400) | `Arial` | Descriptions, labels, tooltips |
| **Numbers / Mono** | `JetBrains Mono` (Medium, 500) | `Consolas` | Timers, speed, scores, stats, gear |

### 2.2 Type Scale

| Token | Size | Line Height | Weight | Usage |
|-------|------|-------------|--------|-------|
| **Display/XL** | 96px | 1.0 | 600 | End-screen final time |
| **Display/L** | 72px | 1.0 | 600 | Speedometer digital readout |
| **Display/M** | 56px | 1.1 | 600 | Position ordinal, level number |
| **Heading/L** | 40px | 1.2 | 600 | Screen titles ("GARAGE", "SETTINGS") |
| **Heading/M** | 32px | 1.2 | 600 | Panel titles, modal headers |
| **Heading/S** | 24px | 1.3 | 600 | Card titles, section labels |
| **Body/L** | 20px | 1.5 | 400 | Menu descriptions, stat labels |
| **Body/M** | 16px | 1.5 | 400 | Default body text, button labels |
| **Body/S** | 14px | 1.5 | 400 | Captions, metadata, unit labels |
| **Caption** | 12px | 1.4 | 400 | Fine print, version numbers, key hints |
| **Mono/L** | 64px | 1.0 | 500 | Gear indicator |
| **Mono/M** | 48px | 1.0 | 500 | Lap timer, large scores |
| **Mono/S** | 32px | 1.0 | 500 | Speedometer, delta times |
| **Mono/XS** | 20px | 1.0 | 500 | Small counters, distances |

### 2.3 Text Effects

- **Drop Shadow:** Offset `(0, 2)`, blur `4`, color `#000000` at 80% opacity. Applied to all HUD text.
- **Outline:** 1px `#000000` at 100% opacity. Applied to minimap labels and small HUD text.
- **Glow:** `Outer Shadow` or `Glow` material effect. Neon GPS Green at 50% opacity, blur 8px. Applied to focused menu items and active tabs.

---

## 3. Button Styles

All buttons use `Border` + `TextBlock` composition. Min touch target: 48×48px (minimum clickable area).

### 3.1 Primary Button

- **Size:** 320×56px (default), scalable via `SizeBox`.
- **Background:** `#00FF85` (Neon GPS Green) fill.
- **Text:** `#0D0D0D` (Surface/900), 16px, `Rajdhani SemiBold`, ALL CAPS.
- **Border Radius:** 4px (rounded corners via `Border` brush).
- **Hover:** Background lightens to `#33FFA0`, scale 1.02x, 0.15s ease-out.
- **Pressed:** Background darkens to `#00CC6A`, scale 0.98x, 0.05s.
- **Disabled:** Background `#2E2E2E`, text `#7A7A7A`, no hover effects.

### 3.2 Secondary Button

- **Size:** Same as Primary.
- **Background:** `#1A1A1A` (Surface/800) fill.
- **Text:** `#F5F5F5` (Surface/50), 16px, `Rajdhani SemiBold`, ALL CAPS.
- **Border:** 2px `#4A4A4A` (Surface/600).
- **Hover:** Border changes to `#00FF85`, text shifts right 4px, 0.15s.
- **Pressed:** Background `#0D0D0D`, border `#00CC6A`.
- **Disabled:** Border `#2E2E2E`, text `#7A7A7A`.

### 3.3 Ghost Button

- **Size:** 360×56px (commonly used in pause menus).
- **Background:** Transparent (`#000000` at 0% opacity).
- **Text:** `#F5F5F5`, 16px, `Rajdhani SemiBold`, ALL CAPS.
- **Border:** 1px `#F5F5F5` at 40% opacity.
- **Hover:** Background `#FFFFFF` at 10% opacity, border 80% opacity, 0.2s.
- **Pressed:** Background `#FFFFFF` at 20% opacity.

### 3.4 Danger Button

- **Size:** Same as Primary.
- **Background:** `#FF2A2A` (Pursuit Red) fill.
- **Text:** `#FFFFFF`, 16px, `Rajdhani SemiBold`, ALL CAPS.
- **Hover:** Background `#FF5555`, scale 1.02x.
- **Pressed:** Background `#CC0000`, scale 0.98x.

### 3.5 Icon Button

- **Size:** 48×48px.
- **Background:** Transparent.
- **Icon:** 24×24px SVG/texture, `#F5F5F5` tint.
- **Hover:** Icon tint `#00FF85`, background `#FFFFFF` at 10%, 0.15s.
- **Pressed:** Scale 0.9x.

---

## 4. Card / Panel Styles

### 4.1 Standard Card

- **Background:** `#1A1A1A` (Surface/800) at 95% opacity.
- **Border:** 1px `#2E2E2E` (Surface/700).
- **Border Radius:** 8px.
- **Padding:** 24px internal.
- **Shadow:** `Box Shadow` — offset `(0, 4)`, blur `16`, color `#000000` at 60%.
- **Hover (interactive cards):** Border brightens to `#4A4A4A`, translate Y -2px, 0.2s.

### 4.2 Elevated Card

- **Background:** `#2E2E2E` (Surface/700) at 95% opacity.
- **Border:** 1px `#4A4A4A` (Surface/600).
- **Border Radius:** 8px.
- **Padding:** 24px.
- **Shadow:** `Box Shadow` — offset `(0, 8)`, blur `24`, color `#000000` at 80%.
- **Usage:** Modals, save slot cards, garage part cards.

### 4.3 Panel / Container

- **Background:** `#0D0D0D` (Surface/900) at 90% opacity.
- **Border:** 1px `#1A1A1A` (Surface/800) top/left, `#000000` bottom/right (inset depth).
- **Border Radius:** 0px (sharp edges for panels) or 4px (for floating panels).
- **Padding:** 32px.
- **Usage:** Settings content panel, garage right panel.

### 4.4 HUD Panel

- **Background:** `#000000` at 60% opacity.
- **Border:** 1px `#4A4A4A` at 40% opacity.
- **Border Radius:** 4px.
- **Padding:** 12px 16px.
- **Backdrop Blur:** `GaussianBlur` at 4px strength (material effect).
- **Usage:** Pause menu buttons, HUD info blocks.

---

## 5. Iconography

### 5.1 Icon Set

All icons are 24×24px vector (SVG imported as textures) or SDF icons. Rendered white (`#F5F5F5`) with `#00FF85` tint on hover/focus.

| Icon | Name | Usage |
|------|------|-------|
| `icon_play` | Right-facing triangle | Play, resume |
| `icon_garage` | Wrench + car silhouette | Garage |
| `icon_settings` | Gear/cog | Settings |
| `icon_credits` | Film clapper | Credits |
| `icon_quit` | Power / door | Quit |
| `icon_speed` | Speedometer arc | Speed stat, speedometer |
| `icon_acceleration` | Lightning bolt | Acceleration stat |
| `icon_handling` | Steering wheel | Handling stat |
| `icon_drift` | Skid marks | Drift stat, drift score |
| `icon_grip` | Tire tread | Grip stat |
| `icon_weight` | Weight scale | Weight stat |
| `icon_nitro` | Flame | Nitro/boost meter |
| `icon_gear` | Gear cog | Gear indicator |
| `icon_rpm` | Tachometer | RPM gauge |
| `icon_checkpoint` | Diamond gate | Checkpoint distance |
| `icon_medal_gold` | Gold star | Gold medal |
| `icon_medal_silver` | Silver star | Silver medal |
| `icon_medal_bronze` | Bronze star | Bronze medal |
| `icon_ghost` | Ghost silhouette | Ghost comparison |
| `icon_camera` | Camera | Share screenshot |
| `icon_share` | Upload arrow | Social share |
| `icon_lock` | Padlock | Locked content |
| `icon_unlock` | Open padlock | Unlocked content |
| `icon_sun` | Sun | Day weather |
| `icon_moon` | Crescent moon | Night weather |
| `icon_cloud` | Cloud | Overcast weather |
| `icon_rain` | Rain drops | Rain weather |
| `icon_arrow_left` | Chevron left | Back navigation |
| `icon_arrow_right` | Chevron right | Forward navigation |
| `icon_arrow_up` | Chevron up | Expand/collapse |
| `icon_arrow_down` | Chevron down | Expand/collapse |
| `icon_close` | X mark | Close modal, dismiss |
| `icon_check` | Checkmark | Confirm, success |
| `icon_warning` | Triangle exclamation | Warning |
| `icon_info` | Circle i | Information tooltip |
| `icon_sound_on` | Speaker | Audio on |
| `icon_sound_off` | Speaker muted | Audio off |
| `icon_fullscreen` | Expand corners | Fullscreen toggle |
| `icon_windowed` | Overlapping squares | Windowed toggle |
| `icon_reset` | Circular arrow | Reset to default |
| `icon_save` | Floppy disk | Save build/settings |
| `icon_delete` | Trash can | Delete slot/build |
| `icon_edit` | Pencil | Rename, edit |
| `icon_plus` | Plus sign | Add new |
| `icon_minus` | Minus sign | Remove |
| `icon_map` | Folded map | Minimap toggle |
| `icon_leaderboard` | Trophy | Leaderboards |
| `icon_achievement` | Ribbon badge | Achievements |

### 5.2 Icon Sizing

| Context | Size |
|---------|------|
| Button inline | 20×20px |
| Standalone icon button | 24×24px |
| HUD icon | 28×28px |
| Large stat icon | 32×32px |
| Medal / achievement | 48×48px |
| Featured illustration | 64×64px |

---

## 6. Animation Principles

### 6.1 Easing Definitions

| Name | Curve | Usage |
|------|-------|-------|
| **Ease-Out** | `Cubic(0.0, 0.0, 0.2, 1.0)` | Standard UI transitions, button hovers |
| **Ease-In-Out** | `Cubic(0.4, 0.0, 0.2, 1.0)` | Screen transitions, modal open/close |
| **Elastic** | `Elastic(0.5, 0.3)` | Medal pop, level up banner, celebratory elements |
| **Bounce** | `Bounce(0.5, 0.3)` | Achievement unlock toast |
| **Linear** | `Linear` | Progress bars, timers, continuous loops |
| **Snap** | `Cubic(0.4, 0.0, 1.0, 1.0)` | Quick HUD updates, gear shifts |

### 6.2 Durations

| Token | Duration | Usage |
|-------|----------|-------|
| **Instant** | 0.0s | Color changes, border changes, opacity toggles |
| **Fast** | 0.1s | Button press, gear shift flash, hover start |
| **Normal** | 0.2s | Hover end, tab switch, focus change |
| **Smooth** | 0.3s | Screen transitions, panel slides, modal open |
| **Dramatic** | 0.5s | Medal reveal, end-screen entry, level up |
| **Ambient** | 2.0s | Background pulse, auto-rotate hints, toast display |
| **Loop** | 15.0s | Title screen background video |

### 6.3 Transition Patterns

- **Screen Transition:** Current screen fades to black (0.2s) → new screen fades in (0.3s). Total 0.5s.
- **Modal Overlay:** Background dims (0.2s, Ease-Out). Modal scales from 0.9→1.0 + fades in (0.3s, Elastic).
- **Slide-In (Left):** Used for settings panels. Translate X -100% → 0%, 0.3s, Ease-Out.
- **Slide-In (Bottom):** Used for toasts. Translate Y 100% → 0%, 0.3s, Ease-Out. Auto-dismiss: fade out after 2s.
- **Count-Up:** Numeric values interpolate from start to end over 1.5s, Ease-Out. Update text every frame.
- **Pulse:** Scale 1.0 → 1.05 → 1.0, loop. Duration 2s, Ease-In-Out. Used for next checkpoint indicator.
- **Flash:** Background opacity 0% → 100% → 0% over 0.15s. Used for shift indicator, collision warning.

### 6.4 Performance Rules

- **All animations use UMG `Widget Animation` timelines** (not tick-based transforms where possible).
- **HUD elements update via `BindWidget` delegates** or `Tick` at 60Hz max.
- **Avoid heavy layout rebuilds during race.** Pre-calculate sizes, use `SizeBox` with explicit sizes.
- **Minimap road mesh render target updates at 10Hz** (not every frame) unless in tight corner.
- **Retainer Boxes** used for complex HUD sections (speedometer, minimap) to render to buffer and reduce overdraw.

---

## 7. Responsive Breakpoints

### 7.1 Aspect Ratio Targets

| Ratio | Resolution Example | Scale Factor | Layout Notes |
|-------|-------------------|--------------|--------------|
| **16:9** | 1920×1080 | 1.0× | Reference layout. All specs written for this. |
| **21:9** | 2560×1080 | 1.0× | Horizontal stretch. HUD anchors to corners maintain spacing. Center elements stay center. Side panels gain extra padding. |
| **4:3** | 1600×1200 | 0.85× | Vertical compression. HUD elements scale down to 0.85×. Minimap shrinks to 170px. Speedometer shifts slightly up. Garage 3D viewport reduces to 50% width. |
| **16:10** | 1920×1200 | 1.0× | Minor vertical adjustment. Bottom HUD raised 24px. |

### 7.2 Scaling Strategy

- **DPI Scale:** UE5 `UserSettings` DPI curve maps:
  - 1080p → 1.0
  - 1440p → 1.25
  - 4K → 1.5
  - <1080p → 0.85
- **Custom HUD Scale Setting:** Overrides DPI scale for HUD only (0.8×–1.5×).
- **Safe Zone:** All anchor margins multiplied by `(1.0 + SafeZonePercent)`.

### 7.3 Minimum Supported

- **Resolution:** 1280×720 (720p)
- **Scale Factor:** 0.75×
- **Adjustments:**
  - Minimap: 140px
  - Speedometer: 180px diameter
  - Buttons: 280×48px
  - Font minimum: 12px (Body/S becomes 10px with outline for legibility)

---

## 8. Asset Naming Conventions

| Prefix | Type | Example |
|--------|------|---------|
| `WBP_` | Widget Blueprint | `WBP_MainMenu` |
| `WBP_` | Reusable Widget | `WBP_ButtonPrimary` |
| `T_UI_` | Texture | `T_UI_IconPlay` |
| `M_UI_` | Material | `M_UI_NeonGlow` |
| `MF_UI_` | Material Function | `MF_UI_RoundedRect` |
| `S_UI_` | Slate Brush / Style | `S_UI_ButtonPrimary` |
| `RT_` | Render Target | `RT_Minimap` |
| `Font_` | Font Asset | `Font_Rajdhani` |

---

## 9. Accessibility

- **Color Blind Mode:** Optional deuteranopia / protanopia / tritanopia filters shift brand colors:
  - Green → Cyan/Blue shift for protanopia.
  - Red → Orange/Pink shift for deuteranopia.
- **Text Contrast:** All text maintains ≥ 4.5:1 contrast against backgrounds.
- **Screen Reader:** All interactive widgets expose `AccessibleText` and `AccessibleSummary`.
- **Subtitles:** All voiceovers and tutorial steps support subtitles (Body/M, white on semi-black bar).
