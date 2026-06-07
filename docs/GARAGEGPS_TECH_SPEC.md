# garageGPS — Developer Technical Specification

**Feature:** Custom Car Kit + Garage Design Editor  
**Product:** raceGPS: Real World Driving  
**Beta target:** Akron Unreal/CARLA vertical slice  
**Spec version:** 0.1.0  
**Prepared for:** raceGPS developer handoff  

**Brand naming note:** This feature was previously referred to internally as “AutoForge.” The locked public/internal feature name is now **garageGPS**.  

---

## 1. Executive Summary

garageGPS is the custom car kit and design editor for the Akron beta. The feature allows players or developers to configure a car using a constrained semantic schema, then generate or assemble a validated Unreal/CARLA-compatible vehicle asset.

The feature is inspired by Articraft's programmatic articulated-asset concept: generate structured asset code against a domain-specific SDK, validate the result, and feed structured errors back into the pipeline. For raceGPS, the scope is narrower and more practical: cars only, one proven four-wheel chassis, modular parts, material customization, and Unreal/CARLA validation.

The first beta should not attempt unconstrained text-to-car generation. It should ship a polished editor over a controlled car-kit system.

**Locked beta rule:** fixed proven chassis, modular visual kit, controlled performance presets, validation before import.

---

## 2. Product Goal

The Akron beta should include a commercial-grade garage/customizer experience:

1. Player opens the Garage.
2. Player selects a base car.
3. Player changes body kit, paint, wheels, spoiler, decals, and performance preset.
4. The configuration is saved as a semantic JSON profile.
5. The selected car loads into Akron Cruise Sprint.
6. The end screen shows the vehicle build used for the run.

---

## 3. Non-Goals for Beta

Do not build these in the first implementation:

- Fully open-ended AI-generated vehicles.
- User-uploaded arbitrary meshes.
- Marketplace-style part sharing.
- Destructive vehicle damage.
- Multiplayer car trading.
- Full procedural physics generation.
- Complete CARLA vehicle factory automation for every generated body shape.
- Real licensed car replicas.

The beta should feel polished, not unlimited.

---

## 4. External Technical Basis

### 4.1 Articraft Basis

Articraft demonstrates the principle of generating articulated 3D assets by writing structured programs against a domain-specific SDK, with joints, part definitions, tests, validation, and structured feedback. garageGPS adopts that pattern, but narrows it to a car-specific SDK and deterministic validators.

### 4.2 CARLA Vehicle Basis

CARLA custom vehicles require detailed vehicle modeling, separated wheels, armature rigging, Unreal import, physics asset setup, animation blueprint setup, wheel blueprints, materials, glass, lights, and registration in the vehicle factory. This confirms that garageGPS must validate assets before they are accepted into the Unreal/CARLA beta.

### 4.3 Unreal Asset Basis

Unreal's FBX skeletal mesh pipeline supports skeletal meshes, materials/textures, animations, morph targets, multiple UV sets, smoothing groups, vertex colors, and LODs. Unreal's FBX pipeline currently expects FBX 2020.2 compatibility, so all export tooling must standardize on that target.

---

## 5. Architecture Overview

```text
garageGPS
  ├─ Garage UI inside Unreal
  ├─ Car semantic config schema
  ├─ Part catalog
  ├─ Paint/material catalog
  ├─ Performance preset catalog
  ├─ Python/Blender asset builder
  ├─ Validation harness
  ├─ Unreal import manifest
  ├─ CARLA compatibility manifest
  └─ Saved player car builds
```

### 5.1 Runtime vs Build-Time Split

**Runtime Unreal Garage:**

- Presents available parts.
- Allows user customization.
- Saves JSON build config.
- Applies material instances and part selections.
- Launches the Akron Cruise Sprint mode with the selected build.

**Build-time garageGPS tools:**

- Generate or assemble base vehicle assets.
- Export FBX/GLB where appropriate.
- Validate geometry, rigging, naming, scale, wheel centers, LODs, materials, and CARLA compatibility.
- Produce Unreal import manifest.

---

## 6. Recommended Repo Structure

```text
racegps/
  apps/
    unreal-akron-beta/
      Content/
        RaceGPS/
          Vehicles/
          Garage/
          Materials/
          UI/
      Source/
        RaceGPS/
          Garage/
          Vehicles/
          SaveGame/

  tools/
    garagegps/
      README.md
      garagegps.py
      car_schema.py
      car_dsl.py
      part_catalog.py
      material_catalog.py
      performance_presets.py
      blender_builder.py
      validators.py
      unreal_manifest.py
      carla_manifest.py
      export_fbx.py
      tests/

  packages/
    car-kit-schema/
      car-build.schema.json
      car-parts.schema.json
      car-materials.schema.json
      performance-presets.schema.json

  assets/
    car-kit/
      base_chassis/
      body_kits/
      wheels/
      spoilers/
      decals/
      materials/

  docs/
    GARAGEGPS_TECH_SPEC.md
    GARAGE_EDITOR_SPEC.md
    CARLA_VEHICLE_PIPELINE.md
```

---

## 7. Feature Modules

### 7.1 Garage UI

Unreal UI screen for car customization.

Required controls:

```text
Base Car
  - Akron Street Coupe
  - Akron Pursuit Sedan
  - Akron Rally Hatch

Body Kit
  - Stock
  - Street
  - Sprint
  - Drift
  - Pursuit

Paint
  - Base color
  - Metallic
  - Matte
  - Pearlescent
  - Clear coat
  - Accent color

Wheels
  - Rim style
  - Tire width preset
  - Tire profile preset
  - Finish

Aero
  - Front splitter
  - Side skirt
  - Rear diffuser
  - Spoiler

Lighting
  - Headlight tint
  - Underglow preset
  - Pursuit light bar, only for pursuit vehicles

Decals
  - Race number
  - Stripe pattern
  - raceGPS logo placement
  - sponsor block placeholders

Performance Preset
  - Street
  - Sprint
  - Drift
  - Pursuit
  - Heavy Cruiser
```

### 7.2 Car Semantic Schema

All car builds must be stored as semantic JSON, not hard-coded blueprint state.

Example:

```json
{
  "schema_version": "0.1.0",
  "car_id": "rgps_akron_coupe_001",
  "display_name": "Akron Street Coupe",
  "base_chassis": "rgps_chassis_4w_sport",
  "body_style": "coupe",
  "kit": "street",
  "wheelbase_m": 2.65,
  "track_width_m": 1.62,
  "mass_kg": 1420,
  "center_of_mass": [0.0, 0.0, -0.18],
  "visual_parts": {
    "front_bumper": "kit_street_front_01",
    "rear_bumper": "kit_street_rear_01",
    "side_skirts": "kit_street_side_01",
    "spoiler": "ducktail_01",
    "wheels": "five_spoke_01",
    "hood": "vented_01"
  },
  "materials": {
    "body": {
      "base_color": "#111827",
      "metallic": 0.7,
      "roughness": 0.24,
      "clearcoat": 0.9
    },
    "glass": {
      "tint": 0.35
    },
    "underglow": {
      "enabled": true,
      "color": "#00E5FF"
    }
  },
  "decals": {
    "race_number": "13",
    "stripe": "center_dual_01",
    "logo": "racegps_side_small"
  },
  "physics_preset": "arcade_sprint",
  "validation": {
    "requires_four_wheels": true,
    "requires_named_bones": true,
    "max_faces": 100000,
    "export_format": "fbx"
  }
}
```

### 7.3 Part Catalog

Parts are defined in a manifest so the Garage UI can populate itself.

```json
{
  "part_id": "ducktail_01",
  "slot": "spoiler",
  "display_name": "Ducktail Spoiler",
  "compatible_body_styles": ["coupe", "sedan"],
  "mesh_path": "/RaceGPS/Vehicles/Parts/Spoilers/SM_Ducktail_01",
  "lod_group": "vehicle_accessory",
  "triangle_budget": 2500,
  "material_slots": ["body_paint"],
  "gameplay_tags": ["street", "sprint"],
  "validation": {
    "collision": "none",
    "socket_required": "SpoilerSocket"
  }
}
```

### 7.4 Performance Presets

Performance presets should alter arcade handling values, not raw user-entered physics.

```json
{
  "preset_id": "arcade_sprint",
  "display_name": "Sprint",
  "mass_kg": 1420,
  "max_speed_kph": 220,
  "acceleration_curve": "sprint_01",
  "steering_response": 0.82,
  "drift_assist": 0.35,
  "traction_assist": 0.55,
  "brake_strength": 0.72,
  "camera_profile": "race_chase_smooth",
  "allowed_modes": ["cruise_sprint"]
}
```

---

## 8. Articraft-Style Car DSL

The build tool should support a Python DSL for deterministic procedural assembly.

```python
from garagegps import CarAsset

car = CarAsset("Akron Street Coupe")

car.set_chassis(
    chassis_id="rgps_chassis_4w_sport",
    wheelbase=2.65,
    track_width=1.62,
    mass_kg=1420,
)

car.add_body(style="coupe", length=4.35, width=1.82, height=1.28)
car.add_wheel("front_left", radius=0.34, width=0.24)
car.add_wheel("front_right", radius=0.34, width=0.24)
car.add_wheel("rear_left", radius=0.36, width=0.27)
car.add_wheel("rear_right", radius=0.36, width=0.27)

car.add_part("front_splitter", kit="street")
car.add_part("spoiler", style="ducktail")
car.add_material("body_paint", clearcoat=0.9, metallic=0.7)
car.add_decal("race_number", number=13)

car.validate_for_unreal()
car.validate_for_carla()
car.export_fbx("akron_street_coupe_001.fbx")
car.export_manifest("akron_street_coupe_001.vehicle.json")
```

---

## 9. Validation Requirements

### 9.1 Geometry Checks

```text
- Asset scale is correct.
- Vehicle faces positive X.
- Vehicle up axis is positive Z.
- Origin is centered near expected center of mass.
- Bounding box fits expected passenger car dimensions.
- Mesh is triangulated before export.
- Face count is within 50k–100k for full vehicle asset target.
- No missing mesh references.
- No non-manifold showstopper geometry for beta assets.
```

### 9.2 Rigging Checks

```text
- Root bone exists.
- Four wheel bones exist.
- Wheel bones are named predictably.
- Wheel bones are positioned at detected wheel centers.
- Bodywork is assigned to root/base bone.
- Each wheel mesh is assigned to its wheel bone.
- Pose test rotates wheels without moving body incorrectly.
```

Required bone names:

```text
Root
Wheel_FL
Wheel_FR
Wheel_RL
Wheel_RR
```

### 9.3 CARLA Compatibility Checks

```text
- Bodywork mesh exists.
- Glass_Ext and Glass_Int meshes are present if glass is enabled.
- Lights mesh/material slot exists if lights are enabled.
- LicensePlate mesh or plate socket exists.
- Wheel dimensions are exported.
- Wheel blueprint references are resolvable.
- Vehicle blueprint class can be generated or manually linked.
- VehicleFactory manifest entry can be produced.
```

### 9.4 Unreal Import Checks

```text
- FBX version target is compatible with Unreal FBX pipeline.
- Materials use expected slot names.
- LOD assets are present or explicitly disabled.
- Collision policy is defined.
- Physics asset policy is defined.
- Vehicle animation blueprint reference is defined.
- Skeletal mesh path is declared in manifest.
```

### 9.5 Gameplay Checks

```text
- Car fits Akron road scale.
- Chase camera socket is unobstructed.
- Vehicle can pass checkpoint gates.
- Vehicle does not clip common beta route geometry.
- Car can complete Akron Cruise Sprint route.
- End screen can show car build metadata.
```

---

## 10. Unreal Integration

### 10.1 Content Paths

```text
/Content/RaceGPS/Vehicles/Base/
/Content/RaceGPS/Vehicles/Parts/
/Content/RaceGPS/Vehicles/Materials/
/Content/RaceGPS/Vehicles/Blueprints/
/Content/RaceGPS/Garage/UI/
/Content/RaceGPS/Garage/Data/
```

### 10.2 Required Unreal Classes

```text
URGCarBuildConfig
URGCarPartDefinition
URGCarMaterialPreset
URGPerformancePreset
ARGVehiclePawn
ARGGarageManager
URGGarageSaveGame
URGGarageWidget
ARGVehiclePreviewActor
```

### 10.3 Runtime Flow

```text
Game starts
  ↓
Load car part catalog
  ↓
Load saved player car config or default config
  ↓
Garage UI previews vehicle
  ↓
Player edits config
  ↓
Validate config against allowed catalog
  ↓
Save config to SaveGame JSON
  ↓
Spawn ARGVehicePawn with selected mesh/material/parts
  ↓
Start Akron Cruise Sprint
```

---

## 11. CARLA Integration

### 11.1 CARLA Vehicle Registration Output

garageGPS should emit a CARLA-facing manifest:

```json
{
  "make": "raceGPS",
  "model": "AkronStreetCoupe",
  "generation": 2,
  "number_of_wheels": 4,
  "blueprint_class": "/Game/Carla/Blueprints/Vehicles/RaceGPS/BP_AkronStreetCoupe",
  "skeletal_mesh": "/Game/RaceGPS/Vehicles/Base/SK_AkronStreetCoupe",
  "wheel_blueprints": {
    "front_left": "BP_RGPS_Wheel_Front",
    "front_right": "BP_RGPS_Wheel_Front",
    "rear_left": "BP_RGPS_Wheel_Rear",
    "rear_right": "BP_RGPS_Wheel_Rear"
  }
}
```

### 11.2 Manual Beta Compromise

For beta, developers may manually create the CARLA vehicle blueprint once, then garageGPS only swaps material instances and modular parts. Full automated VehicleFactory registration can be v0.2.

---

## 12. Build Pipeline

```text
Developer edits car kit schema/parts
  ↓
Run garageGPS validation
  ↓
Generate or assemble asset
  ↓
Export FBX/manifest
  ↓
Import into Unreal project
  ↓
Create/update data assets
  ↓
Package Akron beta build
```

Suggested CLI:

```bash
python tools/garagegps/garagegps.py validate assets/car-kit/base_chassis/akron_coupe.json
python tools/garagegps/garagegps.py build assets/car-kit/builds/akron_street_coupe_001.json
python tools/garagegps/garagegps.py export-unreal assets/car-kit/builds/akron_street_coupe_001.json
python tools/garagegps/garagegps.py export-carla assets/car-kit/builds/akron_street_coupe_001.json
```

---

## 13. Beta Deliverables

### 13.1 Player-Facing

```text
- Garage menu screen.
- Vehicle preview camera.
- Paint picker with preset colors.
- Body kit selector.
- Wheel selector.
- Spoiler selector.
- Decal selector.
- Performance preset selector.
- Save build.
- Launch Akron Cruise Sprint with selected build.
```

### 13.2 Developer-Facing

```text
- Car build JSON schema.
- Part catalog JSON schema.
- Material preset JSON schema.
- Performance preset JSON schema.
- garageGPS validation CLI.
- Sample Akron Street Coupe config.
- Unreal import manifest.
- CARLA compatibility manifest.
- Documentation for adding new parts.
```

---

## 14. MVP Content Requirements

Ship with:

```text
Base cars:
  1. Akron Street Coupe
  2. Akron Pursuit Sedan
  3. Akron Rally Hatch

Body kits:
  1. Stock
  2. Street
  3. Sprint

Paint presets:
  1. Midnight Black
  2. Signal Red
  3. Electric Blue
  4. Asphalt Grey
  5. raceGPS White

Wheel sets:
  1. Five Spoke
  2. Mesh Street
  3. Pursuit Steelie

Spoilers:
  1. None
  2. Ducktail
  3. Sprint Wing

Decals:
  1. None
  2. Dual Stripe
  3. Race Number
  4. raceGPS Side Mark

Performance presets:
  1. Street
  2. Sprint
```

---

## 15. Security and Safety Rules

```text
- No arbitrary user mesh import in beta.
- No remote executable code generation at runtime.
- No unvalidated Python scripts from users.
- No real licensed vehicle trademarks unless authorized.
- No copyrighted car replicas unless cleared.
- No network download of unknown 3D assets during gameplay.
- All public car configs must pass schema validation.
- All asset paths must resolve inside approved project directories.
```

---

## 16. Acceptance Criteria

```text
A1: Garage loads from main menu.
A2: User can select one of three beta vehicles.
A3: User can change paint, wheels, spoiler, decals, and performance preset.
A4: Config saves as JSON-backed SaveGame data.
A5: Selected car appears in Akron Cruise Sprint.
A6: Vehicle can complete the beta route.
A7: End screen displays selected vehicle name and performance preset.
A8: garageGPS validates sample car configs from CLI.
A9: Invalid part combinations fail with clear errors.
A10: Unreal import manifest is generated for at least one vehicle.
A11: CARLA compatibility manifest is generated for at least one vehicle.
A12: No arbitrary or unvalidated asset loading is allowed in beta.
```

---

## 17. Development Milestones

### Milestone 1 — Schema and Catalog

- Create `car-build.schema.json`.
- Create `car-parts.schema.json`.
- Create `car-materials.schema.json`.
- Create `performance-presets.schema.json`.
- Add sample configs.

### Milestone 2 — Unreal Garage UI

- Build preview screen.
- Load part catalog.
- Apply material presets.
- Save build config.

### Milestone 3 — Beta Car Runtime

- Spawn selected car in Akron Cruise Sprint.
- Apply selected paint/parts.
- Route completion works.

### Milestone 4 — garageGPS CLI

- Validate config.
- Validate part compatibility.
- Export Unreal manifest.
- Export CARLA manifest.

### Milestone 5 — Asset Pipeline Hardening

- Add Blender/FBX builder.
- Add geometry/rigging checks.
- Add CI validation for sample car configs.

---

## 18. Open Questions for Developers

1. Will beta use pure Unreal Chaos Vehicle, CARLA BaseVehiclePawn, or a compatibility wrapper?
2. Should modular parts be separate skeletal attachments, static mesh attachments, or merged at build time?
3. Should paint presets use Material Instance parameters only, or support decal texture generation?
4. Should vehicle configs be stored as Unreal DataAssets, JSON files, or both?
5. What triangle budget should be enforced for the packaged beta target hardware?
6. Will CARLA registration be automated in v0.1 or manually configured once?

---

## 19. Recommended Beta Decision

For the first developer pass:

```text
- Use one proven vehicle pawn.
- Use static mesh attachments for visible kit parts.
- Use Material Instance parameters for paint.
- Use JSON schema as source of truth.
- Mirror JSON into Unreal DataAssets for editor convenience.
- Keep CARLA factory integration manual until the vehicle is stable.
```

This keeps the feature commercial-grade while avoiding a research trap.

---

## 20. Source References

- Articraft paper: https://arxiv.org/abs/2605.15187
- Articraft project page: https://articraft3d.github.io/
- Articraft GitHub page: https://github.com/articraft3d/articraft3d.github.io
- CARLA custom vehicle authoring: https://carla.readthedocs.io/en/latest/tuto_content_authoring_vehicles/
- Unreal FBX skeletal mesh pipeline: https://dev.epicgames.com/documentation/en-us/unreal-engine/fbx-skeletal-mesh-pipeline-in-unreal-engine

