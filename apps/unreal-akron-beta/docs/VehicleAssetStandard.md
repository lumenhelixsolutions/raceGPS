# raceGPS Vehicle Asset Standard

> Based on CARLA Simulator asset pipeline. All vehicles MUST follow this standard for compatibility.

---

## Skeleton Hierarchy

Every vehicle must use this exact bone hierarchy. No additions, no removals, no renamed bones.

```
VehicleBase (root)
├── Body
│   ├── Wheel_Front_Left
│   ├── Wheel_Front_Right
│   ├── Wheel_Rear_Left
│   └── Wheel_Rear_Right
├── Door_Front_Left
├── Door_Front_Right
├── Door_Rear_Left
├── Door_Rear_Right
├── Hood
└── Trunk
```

### Bone Requirements

| Bone Name | Purpose | Axis Convention |
|-----------|---------|-----------------|
| `VehicleBase` | Root transform, zero position | Y-forward, Z-up |
| `Body` | Main chassis mesh | Same as root |
| `Wheel_Front_Left` | FL wheel pivot | Z-axis = rotation axis |
| `Wheel_Front_Right` | FR wheel pivot | Z-axis = rotation axis |
| `Wheel_Rear_Left` | RL wheel pivot | Z-axis = rotation axis |
| `Wheel_Rear_Right` | RR wheel pivot | Z-axis = rotation axis |
| `Door_Front_Left` | FL door hinge | Y-axis = hinge axis |
| `Door_Front_Right` | FR door hinge | Y-axis = hinge axis |
| `Door_Rear_Left` | RL door hinge | Y-axis = hinge axis |
| `Door_Rear_Right` | RR door hinge | Y-axis = hinge axis |
| `Hood` | Engine cover hinge | Y-axis = hinge axis |
| `Trunk` | Trunk/boot hinge | Y-axis = hinge axis |

### Scale
- 1 Unreal Unit = 1 cm
- Typical sedan: ~450u long, ~180u wide, ~140u tall
- Wheel radius: ~35u

---

## Material Slot Naming

Slots MUST be named exactly as follows. The `MaterialProvider` maps these to master materials.

| Slot Index | Slot Name | Master Material | Notes |
|------------|-----------|-----------------|-------|
| 0 | `Bodywork` | `M_Master_Vehicle_Paint` | Main body panels |
| 1 | `Glass_Ext` | `M_Master_Vehicle_Glass_Ext` | Windshield, side windows |
| 2 | `Glass_Int` | `M_Master_Vehicle_Glass_Int` | Interior glass surfaces |
| 3 | `Lights` | `M_Master_Vehicle_Lights` | Headlights, taillights, turn signals |
| 4 | `LicensePlate` | `M_Master_Vehicle_LicensePlate` | Front and rear plates |
| 5 | `Wheel` | `M_Master_Vehicle_Rubber` | Tire rubber |
| 6 | `Chassis` | `M_Master_Vehicle_Chrome` | Exhaust, trim, door handles |
| 7 | `Interior` | `M_Master_Vehicle_Paint` | Dashboard, seats (optional) |

---

## Collision Naming

### Physics Collider (simplified hulls for Chaos Vehicle)
Prefix all physics collision meshes with `UCX_`:
- `UCX_Body` — Main chassis convex hull
- `UCX_Wheel_FL` — Front left wheel collision
- `UCX_Wheel_FR` — Front right wheel collision
- `UCX_Wheel_RL` — Rear left wheel collision
- `UCX_Wheel_RR` — Rear right wheel collision

### Sensor Collider (detailed mesh for LiDAR/simulation)
Prefix with `SM_sc_`:
- `SM_sc_Body` — Detailed body mesh for raycasting
- `SM_sc_Wheel_FL` — Detailed wheel mesh

---

## Texture Standards

### Resolution
- Vehicle body: 2048×2048 (ORME packed)
- Wheels: 1024×1024
- Lights: 512×512
- Interior: 1024×1024

### ORME Packing
Single texture with channels:
- **R** — Ambient Occlusion
- **G** — Roughness
- **B** — Metallic
- **A** — Emissive mask

### Naming
```
T_<VehicleName>_<Part>_<Type>
T_Sedan_Body_BC.png      # Base color
T_Sedan_Body_ORME.png    # Packed ORME
T_Sedan_Body_N.png       # Normal map
T_Sedan_Wheel_BC.png
T_Sedan_Wheel_ORME.png
T_Sedan_Wheel_N.png
```

---

## Vehicle Classes

| Class | Poly Budget (LOD0) | Wheel Count | Example |
|-------|-------------------|-------------|---------|
| Sedan | 25,000 | 4 | Toyota Camry |
| Sports | 30,000 | 4 | Porsche 911 |
| Truck | 35,000 | 4-6 | Ford F-150 |
| SUV | 28,000 | 4 | Jeep Wrangler |
| Hatchback | 22,000 | 4 | VW Golf |

---

## LOD Requirements

| LOD | Triangle Reduction | Distance |
|-----|-------------------|----------|
| LOD0 | 100% | 0-10m |
| LOD1 | 50% | 10-30m |
| LOD2 | 25% | 30-100m |
| LOD3 | 10% | 100m+ |

---

## Import Checklist

- [ ] Skeleton matches standard hierarchy exactly
- [ ] Bone axes correct (Z = wheel rotation, Y = door hinge)
- [ ] Material slots named correctly (Bodywork, Glass_Ext, etc.)
- [ ] UCX_ physics meshes present
- [ ] ORME texture packed correctly
- [ ] Textures named with T_ prefix
- [ ] LODs generated (LOD0-LOD3)
- [ ] Scale correct (1 unit = 1 cm)
- [ ] Origin at ground level, centered on wheels
