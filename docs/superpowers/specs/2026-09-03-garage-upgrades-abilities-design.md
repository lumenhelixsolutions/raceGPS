# Garage Upgrades + Abilities — Design

**Date:** 2026-09-03  
**Project:** raceGPS (`apps/unreal-akron-beta`, UE 5.7)  
**Status:** Approved direction (stats + active abilities). Implement only after a working 3-car Burke AutoLap (`EndRace`).

## Goals

- Keep the **default car arcade-fun** (Midnight Club street feel). Upgrades never make the stock car worse or “more sim.”
- Give **advanced players** a built-in **garage**: spend race rewards on **stat tiers** and equip **active abilities**.
- Persist loadout between sessions (local first). Apply loadout when spawning Cleveland / CruiseSprint pawns.

## Non-goals (this design)

- Full suspension/gear-ratio/tire-compound builder
- Online economy, battle pass, or microtransactions
- Cesium / Karla skyline / GPS mesh population (separate visual track)
- Replacing Chaos with a different vehicle model

## Player fantasy

You leave a Burke lap, open the garage, drop cash into Engine and Grip, slot Nitro + Drift Stick, and the next grid feels like *your* build — still arcade, just meaner.

## Design units

| Unit | Responsibility |
|------|----------------|
| `UGarageSaveGame` / `FGarageProfile` | Currency, owned unlocks, per-car stat tiers, equipped ability IDs |
| `UGarageStatModel` | Tier 0–5 → Chaos multiplier tables (Engine/Grip/Brake/Weight) |
| `UGarageAbilityCatalog` | Ability defs: id, unlock rule, cooldown, duration, effect |
| `UGarageAbilityComponent` | Runtime: 2 slots, input, cooldown, apply/remove effects on pawn |
| `UGarageSubsystem` or `AGarageMenu` | Load/save, buy tier, equip ability, start race with loadout |
| Race reward hook | On `EndRace` / finish place → grant currency |

## Data model

```
FGarageCarLoadout
  VehicleId          // e.g. "DodgeCharger2024"
  EngineTier 0..5
  GripTier 0..5
  BrakeTier 0..5
  WeightTier 0..5
  AbilitySlot0       // FName or empty
  AbilitySlot1

FGarageProfile
  Cash int32
  UnlockedAbilities TArray<FName>
  Loadouts TMap<FName, FGarageCarLoadout>
```

### Stat tiers (arcade multipliers on Chaos)

Applied at spawn via existing tuning path (`SetTuningData` / movement setup), not per-frame.

| Tier | Engine (MaxTorque) | Grip (lat friction) | Brake | Weight (mass scale) |
|------|--------------------|---------------------|-------|---------------------|
| 0 | 1.00 | 1.00 | 1.00 | 1.00 |
| 1 | 1.08 | 1.05 | 1.06 | 0.98 |
| 2 | 1.16 | 1.10 | 1.12 | 0.96 |
| 3 | 1.26 | 1.16 | 1.20 | 0.94 |
| 4 | 1.38 | 1.22 | 1.28 | 0.92 |
| 5 | 1.52 | 1.30 | 1.38 | 0.90 |

Exact numbers may tune in playtest; keep monotonic and fun.

### Starting ability catalog

| Id | Role | Duration | Cooldown |
|----|------|----------|----------|
| `Nitro` | Forward impulse / temporary torque mult | ~1.5s | ~8s |
| `DriftStick` | Lower lat grip + yaw assist for slides | ~2.5s | ~10s |
| `QuickShift` | Instant upshift + brief torque bump | instant | ~6s |
| `BrakeStab` | Short max brake + stability | ~0.8s | ~7s |
| `HeatDump` | Clear or pause chase/heat meter (stub OK until heat exists) | instant | ~20s |

Player equips **two**. Locked abilities unlock by placing in Burke races or spending cash (pick one rule in G2; default: place top-2 once unlocks that ability’s purchase, then buy with cash).

## Apply-at-spawn flow

1. Grid / GameMode resolves pawn class (`BP_DodgeCharger2024`).
2. After Chaos bring-up (one-shot wheels/torque — no recreate storm), call `ApplyGarageLoadout(Pawn, Loadout)`.
3. `ApplyGarageLoadout` multiplies arcade base params by tier tables; does not replace arcade base with sim curves.
4. Attach/activate `UGarageAbilityComponent` with Slot0/Slot1.

## Ability runtime

- Input: Ability1 / Ability2 actions (project settings).
- On activate: if cooldown ready, apply effect, start duration timer, start cooldown.
- Effects are short arcade buffs on the same Chaos pawn (torque, friction, impulse) — not a second physics model.

## UI (G0 minimal → polish later)

- G0: debug console or simple UMG: show cash, tiers, equipped abilities; keys to buy/equip.
- Later: Midnight Club-style garage panel between Menu and Race.

## Phasing

0. **Prerequisite:** Cleveland 3-car AutoLap reaches `EndRace` with arcade-fun drive.
1. **G0:** SaveGame + cash + empty garage shell + reward on EndRace.
2. **G1:** Stat tiers apply at spawn; PIE/playtest shows torque/speed scale with EngineTier.
3. **G2:** Ability component + 2-slot equip + at least Nitro + DriftStick working in race.
4. **G3 (optional):** Full UMG garage; more unlock rules; heat integration.

## Success criteria

- Stock tier-0 car still easy and fun.
- Tier-5 Engine clearly faster in a straight on Burke.
- Two abilities fire with cooldowns without breaking AI grid cars (AI may use null loadout or scripted tiers).
- Loadout survives editor restart via SaveGame.
- No per-frame `RecreatePhysicsState` introduced by garage apply.

## Open knobs (defaults chosen)

- Currency name: **Rep** (street rep cash).
- AI: use fixed mid tiers (2/2/2/2), no abilities, unless showcase wants mirror loadouts.
- Save slot: `GarageProfile_v1`.
