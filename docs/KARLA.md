# Karla

Karla is raceGPS’s **offline visual kernel**. It is not CARLA.

- **CARLA**: optional vehicle/prop meshes already in `Content/Carla`. Never a required server.
- **Karla**: turn geographic semantics (OSM / citypack) into camera-needed UE volumes.

First slice: **skyline**. Input buildings + racing line, output `skyline.json` that `AClevelandEnvironmentActor` already loads. Buildings north of the circuit are dropped so the airport stays the near field.

This is the start of Dynamic Geographic World Materialization. It is **not** city-scale streaming, Cesium, or a second race engine.

```
python scripts/karla_visual_kernel.py \
  --buildings citypacks/cleveland_5.0km/cleveland_5.0km_buildings.json \
  --racing-line citypacks/cleveland/burke_gp_1997/racing_line.json \
  --landmarks citypacks/cleveland/burke_gp_1997/skyline.json \
  --out citypacks/cleveland/burke_gp_1997/skyline.json
```

Named landmarks (Key Tower, Terminal Tower, …) stay; OSM masses fill the rest.
