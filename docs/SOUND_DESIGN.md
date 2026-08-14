# Sound Design Document — raceGPS

> **Target Engine:** Unreal Engine 5.7  
> **Mix Format:** 48 kHz / 24-bit WAV (source)  
> **Runtime:** Ogg Vorbis 256 kbps + uncompressed WAV for short one-shots  
> **Spatial:** 7.1 surround base mix with HRTF fallback for headphones  

---

## 1. Vehicle Audio

### 1.1 Engine Layers

| Layer | Description | Trigger | Loop |
|-------|-------------|---------|------|
| **Idle** | Low-frequency rumble, 600–900 RPM | Vehicle ignition | Yes |
| **Acceleration** | RPM sweep 1,000 → 7,000+ | Throttle input > 10 % | Yes |
| **Deceleration** | Engine-braking tone, slight backfire pops | Throttle release + RPM > 2,000 | Yes |
| **Turbo Spool** | Whistle/whoosh 3–8 kHz | Throttle + turbo PSI > 0.2 bar | Yes (blend) |
| **Turbo Flutter / Blow-off** | High-pitch chirp / PSHHH | Throttle lift after boost | No |
| **Exhaust Note** | Tailpipe tone, varies by exhaust upgrade | Always on (mix) | Yes |
| **Rev Limiter** | Staccato cut/bounce at max RPM | RPM ≥ redline | Yes |
| **Gear Shift** | Mechanical clunk + transient dip | Gear change event | No |
| **Transmission Whine** | Straight-cut gear mesh | High load + high RPM | Yes |

**UE5 Implementation Notes**
- Use `UAudioComponent` with `SoundCue` crossfading between 3–5 engine RPM loops.
- Pitch modulation via `PitchMultiplier` driven by RPM normalized curve.
- Turbo layer blended via `SoundMix` volume override tied to turbo pressure float.
- Rev limiter: rapid amplitude gate (10–15 ms) synced to RPM bounce.

---

## 2. Tire Audio

| Event | Surface Variants | Trigger | Priority |
|-------|------------------|---------|----------|
| **Skid (long)** | Asphalt, wet asphalt, gravel, snow | Lateral slip > 0.6 for > 0.3 s | High |
| **Squeal (short)** | Asphalt, concrete | Lateral slip 0.3–0.6 | High |
| **Gravel spray** | Dirt, gravel, cobblestone | Wheelspin / slide on loose surface | Medium |
| **Wet pavement** | Standing water, rain-soaked road | Slip on wet surface + rain active | Medium |
| **Curb hit** | Concrete curb, metal rumble strip | Suspension compression + velocity | High |
| **Rumble strip** | Track edge, highway shoulder | Continuous drive-over | Medium |

**UE5 Implementation Notes**
- Surface-type lookup via Chaos Vehicle `WheelState` → physical material.
- Tire audio components use `Concurrency` rules: max 2 simultaneous skids per vehicle.
- Parameter-driven `SoundCue` for skid intensity (0–1) controlling filter cutoff and volume.

---

## 3. Environment Audio

| Layer | Description | Loop | Spatial |
|-------|-------------|------|---------|
| **City ambience** | Distant traffic, HVAC hum, reverberant street noise | Yes | 3D ambient box |
| **Highway ambience** | Constant tire drone, Doppler-pass vehicles | Yes | 3D ambient box |
| **Traffic** | Honks, sirens, pass-bys | No / random intervals | 3D point sources |
| **Birds / nature** | Morning birds, crickets (evening) | Yes (region) | 3D point sources |
| **Wind (environmental)** | Low roar increasing with altitude / speed | Yes | 2D stereo bed |

**UE5 Implementation Notes**
- Use `AmbientSound` actors with `Attenuation` shapes for districts.
- City ambience: layered 4–6 mono loops positioned at street intersections, mixed to 7.1 bed.
- Time-of-day system crossfades between bird (day) and cricket (night) layers.

---

## 4. UI Audio

| Event | Description | File Prefix | Notes |
|-------|-------------|-------------|-------|
| **Menu click** | Short tactile pop | `S_UI_Click_*` | Mono, 0.1–0.2 s |
| **Menu hover** | Subtle tick / blip | `S_UI_Hover_*` | Very low volume |
| **Menu select** | Confident confirm tone | `S_UI_Select_*` | Slight reverb tail |
| **Menu back** | Lower-pitch cancel | `S_UI_Back_*` | Descending pitch |
| **Countdown** | Beep (3-2-1) + go swish | `S_UI_Count_*` | Pitch rises, final go = sweep |
| **Checkpoint pass** | Positive chime + whoosh | `S_UI_Check_*` | Stinger, not looped |
| **Best lap** | Bright celebratory tone | `S_UI_Best_*` | Longer tail |
| **Error / invalid** | Dull buzz | `S_UI_Error_*` | Low frequency |

**UE5 Implementation Notes**
- UI sounds play through `UISoundMix` which ducks music by 3 dB during active menu navigation.
- All UI cues are 2D stereo (no spatialization) to ensure clarity.

---

## 5. Music

| Context | Style | BPM | Loop | Format |
|---------|-------|-----|------|--------|
| **Menu theme** | Synthwave / electronic, energetic | 120–128 | Yes | Ogg 256 kbps |
| **Race theme (day)** | Driving rock / electronic, high energy | 140–150 | Yes | Ogg 256 kbps |
| **Race theme (night)** | Darker, bass-heavy electronic | 130–140 | Yes | Ogg 256 kbps |
| **Win stinger** | Brass / synth fanfare, 3–4 s | — | No | WAV |
| **Lose stinger** | Descending tone, somber | — | No | WAV |
| **Podium / results** | Uplifting, moderate energy | 110 | Yes | Ogg 256 kbps |

**UE5 Implementation Notes**
- Music managed by custom `MusicManager` subsystem with:
  - Crossfade between tracks (2 s overlap).
  - Adaptive intensity layers (drums, bass, melody stems) mixed based on race position / speed.
  - Sidechain ducking: music drops 6 dB when countdown or checkpoint stinger plays.

---

## 6. Ghost Audio

| Element | Description | Priority |
|---------|-------------|----------|
| **Ghost engine** | Same engine layers as player but 40 % volume + high-pass filter | Low |
| **Doppler effect** | Pitch shift ± semitone as ghost approaches / recedes | Automatic (UE5) |
| **Ghost passage** | Wind rush as ghost car passes camera | Medium |

**UE5 Implementation Notes**
- Ghost pawn uses same `AudioComponent` setup as player but routed to "Ghost" `SoundClass`.
- `SoundClass` volume = 0.4 and high-pass cutoff = 400 Hz to create "ethereal" quality.
- Doppler enabled on `AttenuationSettings` with `DopplerIntensity = 1.5`.

---

## 7. Weather Audio

| Condition | Layer | Description | Loop |
|-----------|-------|-------------|------|
| **Light rain** | Ambience | Gentle patter on various surfaces | Yes |
| **Heavy rain** | Ambience | Dense downpour, low rumble | Yes |
| **Rain on windshield** | Interior | Rhythmic droplets, varies with speed | Yes |
| **Thunder** | One-shot | Distant rumble → close crack | No |
| **Snow wind** | Ambience | Muffled howl, low frequency | Yes |
| **Hail** | Impact | Sharp pellets on metal / glass | Yes (sparse) |

**UE5 Implementation Notes**
- Weather audio is part of the `WeatherSubsystem`.
- Rain intensity 0–1 drives volume and layer selection (light → heavy).
- Thunder triggered by lightning flash with randomized delay 0.1–3.0 s.
- Interior rain uses low-pass filter to simulate cabin isolation.

---

## 8. Spatial Audio Setup

### 8.1 Channel Configuration

- **Primary mix:** 7.1 surround (L, R, C, LFE, Ls, Rs, Lr, Rr)
- **Fallback:** Stereo downmix with HRTF (head-related transfer function) for headphone users
- **UE5 Plugin:** Enable **Steam Audio** or **Resonance Audio** plugin for HRTF and occlusion

### 8.2 Attenuation Curves

| Audio Category | Falloff | Min Radius | Max Radius | Occlusion |
|----------------|---------|------------|------------|-----------|
| Engine (player) | Custom | 2 m | 200 m | No |
| Engine (AI/ghost) | Logarithmic | 5 m | 500 m | Yes |
| Tire skid | Linear | 1 m | 80 m | No |
| Environment | Linear | 50 m | 2,000 m | Yes |
| UI | None (2D) | — | — | — |
| Music | None (2D) | — | — | — |

### 8.3 Occlusion & Propagation

- Buildings: `0.5` volume, `0.3` LP cutoff multiplier when raycast blocked.
- Tunnels: Reverb send to `ReverbEffect` with 1.5 s decay, −3 dB per bounce.
- Bridges: Open-air ambience, reduced occlusion.

---

## 9. Mixing Priorities & Ducking

### 9.1 Priority Levels (0 = highest)

| Priority | Category | Max Concurrent | Duck Others |
|----------|----------|----------------|-------------|
| 0 | UI stingers (countdown, checkpoint) | 3 | Music −6 dB, SFX −3 dB |
| 1 | Player vehicle engine + tire | 4 | — |
| 2 | Collision / crash impacts | 6 | — |
| 3 | AI vehicle engine (nearby) | 4 | — |
| 4 | Weather (heavy) | 2 | Ambience −2 dB |
| 5 | Environment ambience | 8 | — |
| 6 | Music | 2 | — |
| 7 | Ghost vehicle | 2 | — |
| 8 | Distant AI / ambient traffic | 12 | — |

### 9.2 Sidechain Routing

```
[Music Bus] ──►[Sidechain Detector]──►[Duck Gain]──► Music attenuation
                    ↑
[Voice / UI] ───────┘
```

- **Attack:** 10 ms
- **Release:** 500 ms
- **Ratio:** 4:1 (music drops 6 dB when UI stinger triggers)

### 9.3 UE5 Sound Classes

| Sound Class | Volume | Pitch | Child Classes |
|-------------|--------|-------|---------------|
| `S_Master` | 1.0 | 1.0 | `S_Music`, `S_SFX`, `S_UI`, `S_Voice` |
| `S_Music` | 0.8 | 1.0 | — |
| `S_SFX` | 1.0 | 1.0 | `S_Vehicle`, `S_Tire`, `S_Environment`, `S_Weather` |
| `S_Vehicle` | 1.0 | 1.0 | `S_Engine`, `S_Exhaust`, `S_Turbo` |
| `S_UI` | 0.9 | 1.0 | — |
| `S_Ghost` | 0.4 | 1.0 | Ghost engine routed here |

---

## 10. Asset Naming Convention

```
{S}_{Category}_{Subcategory}_{Variant}_{Index}

Examples:
  S_Engine_Idle_01.wav
  S_Engine_Accel_03.wav
  S_Tire_Skid_Asphalt_01.wav
  S_Tire_Skid_Wet_02.wav
  S_UI_Click_01.wav
  S_Music_Menu_01.ogg
  S_Ambience_City_Day_01.wav
  S_Weather_Rain_Light_01.wav
```

- `S_` = Sound
- Category = `Engine`, `Tire`, `UI`, `Music`, `Ambience`, `Weather`, `Ghost`
- Variant = surface type, intensity, or sub-type
- Index = `01`, `02`, … for variations

---

## 11. Production Checklist

- [ ] All source recordings delivered as **48 kHz / 24-bit WAV**
- [ ] Loop points verified with zero-crossing alignment
- [ ] Loudness normalized to **−23 LUFS** (environmental) / **−16 LUFS** (SFX)
- [ ] Naming convention validated by `import_pipeline.py`
- [ ] UE5 `SoundCue` assets created for all looping sounds
- [ ] `AttenuationSettings` configured per category
- [ ] `SoundMix` profiles built for menu, race, pause, replay states
- [ ] HRTF tested with headphones (Windows Sonic / Steam Audio)
- [ ] 7.1 mix validated on surround speaker array
