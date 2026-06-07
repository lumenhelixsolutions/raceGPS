# garageGPS — Custom Car Kit + Garage Editor

> Part of raceGPS v0.2.0

## Quick Start

```bash
# Validate a car build
python garagegps.py validate ../../assets/car-kit/builds/akron_street_coupe_001.json

# Build enriched manifest
python garagegps.py build ../../assets/car-kit/builds/akron_street_coupe_001.json

# Export Unreal import manifest
python garagegps.py export-unreal ../../assets/car-kit/builds/akron_street_coupe_001.json

# Export CARLA compatibility manifest
python garagegps.py export-carla ../../assets/car-kit/builds/akron_street_coupe_001.json
```

## Car Build Schema

See `packages/car-kit-schema/car-build.schema.json` for the full JSON Schema.

### Required Fields
- `schema_version`: "0.1.0"
- `car_id`: `rgps_<name>` format
- `display_name`: Human-readable name
- `base_chassis`: `rgps_chassis_4w_sport|sedan|hatch`
- `visual_parts`: Front bumper, rear bumper, side skirts, spoiler, wheels, hood
- `materials`: Body paint, glass tint, optional underglow
- `physics_preset`: `arcade_street|sprint|drift|pursuit|heavy`

## Sample Builds

- `assets/car-kit/builds/akron_street_coupe_001.json` — Akron Street Coupe (sprint preset)

## MVP Content

| Category | Count | Items |
|----------|-------|-------|
| Base cars | 3 | Akron Street Coupe, Pursuit Sedan, Rally Hatch |
| Body kits | 5 | Stock, Street, Sprint, Drift, Pursuit |
| Paint presets | 5 | Midnight Black, Signal Red, Electric Blue, Asphalt Grey, raceGPS White |
| Wheel sets | 3 | Five Spoke, Mesh Street, Pursuit Steelie |
| Spoilers | 3 | None, Ducktail, Sprint Wing |
| Decals | 4 | None, Dual Stripe, Race Number, raceGPS Side Mark |
| Performance presets | 2 | Street, Sprint |
