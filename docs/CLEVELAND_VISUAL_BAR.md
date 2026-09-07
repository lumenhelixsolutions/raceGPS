# Cleveland Visual Bar — Midnight Club / Midnight Run

Production target for the Cleveland Historic Circuit showcase. This is **not** a tech-demo checklist. The bar is a 2000s street-racing game still: readable city, wet-looking night, cars that photograph well, a camera that sells Cleveland.

**Map fact (do not ignore):** `Cleveland5_0KmWorld` already contains a T10 headless city import (`verify_t10_map.py`): **building_instances=119601**, **water_instances=7256**, **pois=200**. An empty-runway screenshot is almost always **camera / spawn heading / lighting**, not a missing downtown. Karla's ~79 silhouette boxes are an additive near-track skyline, not the city.

**Launch (does not change `GlobalDefaultGameMode`):** `apps/unreal-akron-beta/LaunchCleveland.bat`  
`Cleveland5_0KmWorld?game=/Script/raceGPSAkronBeta.ClevelandShowcaseGameMode`

**Geo:** X=east, Y=north, 1 uu = 1 cm. Downtown is **south** of Burke. Lake Erie is **north**. Racing-line start heading is ~247° WSW.

---

## Intended framing (V1)

- **Intro camera (3.4s hold, 1.6s cubic blend):** spawned **east** of the 3-car grid, ~62 m up, looking **WSW** along the cars. Downtown HISM + Karla towers land **left of frame**; Erie lands **right**; cars sit in the foreground.
- **Chase cam (pawn):** `ApplyClevelandShowcaseChaseFraming()` — arm 1100, raised socket, **3/4 left** (vehicle −Y). At WSW heading that left bias looks toward downtown instead of a tight north-facing runway shot. Akron/CruiseSprint spring-arm defaults are unchanged.
- A north-facing editor screenshot along the runway will keep putting the 120k-building mass **behind** the camera. That is a framing bug, not a content hole.

---

## Milestones

### V1 — Camera that sells Cleveland (code, this pass)

**Done when:** PIE/game boot shows downtown + lake + the 3-car grid in the first five seconds without the player hunting for a view.

Acceptance vs Midnight Club:

- Establishing shot reads as a **place** (city left, water right, cars center), not a gray slab.
- Blend to chase does not snap; chase stays high enough to keep towers on the horizon.
- Log: `skyline intro camera` then `blending intro camera to pawn chase`.

### V2 — Midnight Run lighting + grade (code, this pass)

**Done when:** default showcase look is night, one sun, no capture spam, Epic post.

Acceptance vs Midnight Club:

- `ClevelandLookDirector` default **MidnightRun** (22:00, cool moonlight, sky atmosphere + volumetric cloud).
- Extra directional lights disabled; unbuilt reflection captures hidden; `DisableAllScreenMessages`.
- Epic console: `r.VolumetricCloud`, `r.SkyAtmosphere`, `r.ShadowQuality`, `r.ReflectionMethod`, `r.BloomQuality`, `r.Tonemapper.Quality`.
- Night grade mutates `APostProcessController::EpicPreset` (Bloom, Contrast, Saturation, Chromatic, Vignette, SceneColorTint, AutoExposureBias) then `ApplyPresetForTier(Epic)`.
- SunnyDay remains callable; it is **not** the showcase default.

### V3 — Skyline craft (code, this pass)

**Done when:** Karla silhouette is a readable downtown ridge, not 79 identical gray boxes, and logs print the **real building count**.

Acceptance vs Midnight Club:

- Materials vary (glass vs concrete vs brick) by height/name.
- Tall named towers (Key, Terminal, Public Square, …) are slightly exaggerated and built via `ABuildingMeshGenerator::AddWorldBoxBuilding` when present.
- MidnightRun applies emissive window MIDs (dynamic instances; no new uassets required).
- Log: `skyline buildings=N named_towers=M` with N matching `skyline.json` count (~79), **not** `skyline=1`.
- Treat this as **additive** to the 120k T10 HISM city.

### V4 — Water that reads as Erie (code, this pass)

**Done when:** from the intro/chase cameras the north sheet is a lake, not a postage-stamp plane.

Acceptance vs Midnight Club:

- `Water_Surface` MID tint/opacity/roughness applied (existing params only).
- North edge expanded in code so the horizon fills from the raised chase.
- Night tint goes darker/cooler; day stays lake-blue.
- Log: `lake verts=N`.

### V5 — Cars that photograph (next)

Headlights/taillights at night, paint depth, tire/brake glow, 3-car grid readable as a race not three capsules. Hellcat vs AI looks must separate in a still.

### V6 — Track surface (next)

Runway/taxi paint, start lights, wet night specular, no floating cars. Hangar ISMs + markings already in env actor; push density and night self-illumination.

### V7 — Atmosphere / city glow (next)

Exponential height fog, distant HISM emissive, neon pockets along the south skyline, volumetric god-rays off the moon. The 120k city must **light the night**, not sit as a dark mass.

### V8 — World dressing density (next)

Cones, barriers, airport props, sparse night traffic beyond the 3-car grid. Enough junk that a screenshot does not look like a greybox, not so much that Chaos explodes.

### V9 — HUD / audio / speed (next)

Neon HUD, engine/tire audio, wind, a taste of city night bed. Speed FX (chromatic, motion blur) already hooks `ApplyRacingBoostEffect`; tune so 0 km/h intro is clean and 200 km/h sings.

### V10 — Cinematic close (next)

Photo-mode stills, replay camera, a 15-second title beat (`raceGPS — CLEVELAND HISTORIC CIRCUIT`) that a stranger would watch twice. If it still looks like a sample map, V10 is not done.

---

## Playtest (after this pass)

1. Confirm Unreal Editor is closed (this pass assumed it was).
2. UBT: `raceGPSAkronBetaEditor Win64 Development`.
3. `apps\unreal-akron-beta\LaunchCleveland.bat` (or PIE with ClevelandShowcaseGameMode on `Cleveland5_0KmWorld`).
4. Watch Output Log for:
   - `skyline intro camera`
   - `MidnightRun applied`
   - `suppressed extra directional lights=… reflection captures=…`
   - `skyline buildings=` **not** `skyline=1`
   - `grid spawned (3 pawns)`
5. First frame of `-game`: city left, lake right, cars center. After ~5s, raised 3/4 chase. F8/screenshot.
6. If the view is still empty runway looking north, the camera did not possess / intro failed — do not rebuild the T10 city.

---

## What this pass did **not** do

- Did **not** commit.
- Did **not** change `GlobalDefaultGameMode` (stays `CruiseSprintGameMode`).
- Did **not** replace the 120k HISM city with Karla boxes.
- V5–V10 remain open (cars, track, fog/glow, dressing, HUD/audio, cinematic close).