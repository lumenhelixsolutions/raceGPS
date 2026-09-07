# Garage Upgrades + Abilities Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a built-in garage with Engine/Grip/Brake/Weight tiers (0–5) and a 2-slot active ability loadout (Nitro, Drift Stick, etc.) on top of arcade-fun Chaos defaults, after the 3-car Burke race loop works.

**Architecture:** Local `UGarageSaveGame` holds cash + per-car loadouts. At pawn spawn (Cleveland grid / CruiseSprint), `ApplyGarageLoadout` multiplies arcade Chaos params once. `UGarageAbilityComponent` runs two cooldown abilities from input. Rewards grant cash on `EndRace`. UI starts as debug/UMG shell, then a real garage panel.

**Tech Stack:** UE 5.7 C++ (`raceGPSAkronBeta`), Chaos vehicles, `USaveGame`, Enhanced Input or project axis bindings, optional UMG.

## Global Constraints

- Do **not** implement garage tasks until Cleveland AutoLap playtest shows cars at real arcade speed and preferably `EndRace`.
- Keep `GlobalDefaultGameMode=CruiseSprintGameMode` (Akron default unchanged).
- Arcade stock (all tiers 0) must remain fun — upgrades only buff.
- No git commit/push unless Chris explicitly asks.
- Out of scope: Cesium, Karla populate-on-fly, full sim builder, online economy.
- Never call `EnsureDefaultWheels` / `RecreatePhysicsState` in a per-frame garage path.

**Spec:** `docs/superpowers/specs/2026-09-03-garage-upgrades-abilities-design.md`

---

### Task G0: Save profile, cash, EndRace reward

**Files:**
- Create: `apps/unreal-akron-beta/Source/raceGPSAkronBeta/Public/Garage/GarageTypes.h`
- Create: `apps/unreal-akron-beta/Source/raceGPSAkronBeta/Public/Garage/GarageSaveGame.h`
- Create: `apps/unreal-akron-beta/Source/raceGPSAkronBeta/Private/Garage/GarageSaveGame.cpp`
- Create: `apps/unreal-akron-beta/Source/raceGPSAkronBeta/Public/Garage/GarageSubsystem.h`
- Create: `apps/unreal-akron-beta/Source/raceGPSAkronBeta/Private/Garage/GarageSubsystem.cpp`
- Modify: `apps/unreal-akron-beta/Source/raceGPSAkronBeta/Private/RaceSessionManager.cpp` (EndRace reward hook)
- Modify: `apps/unreal-akron-beta/Source/raceGPSAkronBeta/raceGPSAkronBeta.Build.cs` only if new module deps needed (prefer none)

**Interfaces:**
- Consumes: existing `EndRace` / session finished path in `RaceSessionManager` / Cleveland GameMode report
- Produces:
  - `struct FGarageCarLoadout` with `VehicleId`, `EngineTier`, `GripTier`, `BrakeTier`, `WeightTier`, `AbilitySlot0`, `AbilitySlot1`
  - `UGarageSaveGame` with `int32 Cash`, `TArray<FName> UnlockedAbilities`, `TMap<FName, FGarageCarLoadout> Loadouts`
  - `UGarageSubsystem::GetProfile()`, `AddCash(int32)`, `SaveProfile()`, `LoadOrCreateProfile()`, `GetLoadout(FName VehicleId)`

- [ ] **Step 1: Add types + SaveGame**

```cpp
// GarageTypes.h
USTRUCT(BlueprintType)
struct FGarageCarLoadout
{
	GENERATED_BODY()
	UPROPERTY(SaveGame, EditAnywhere) FName VehicleId = TEXT("DodgeCharger2024");
	UPROPERTY(SaveGame, EditAnywhere) int32 EngineTier = 0;
	UPROPERTY(SaveGame, EditAnywhere) int32 GripTier = 0;
	UPROPERTY(SaveGame, EditAnywhere) int32 BrakeTier = 0;
	UPROPERTY(SaveGame, EditAnywhere) int32 WeightTier = 0;
	UPROPERTY(SaveGame, EditAnywhere) FName AbilitySlot0;
	UPROPERTY(SaveGame, EditAnywhere) FName AbilitySlot1;
};
```

- [ ] **Step 2: Implement subsystem load/save** using `UGameplayStatics::SaveGameToSlot` / `LoadGameFromSlot` with slot name `GarageProfile_v1`.

- [ ] **Step 3: Hook EndRace** — on player finish, `AddCash` by place (e.g. 1st=1500, 2nd=900, 3rd=500, DNF=100) then `SaveProfile()`. Log: `raceGPS Garage: +Cash place=N total=T`.

- [ ] **Step 4: Verify** — run `LaunchCleveland.bat playtest` (or force EndRace path). Confirm log line and that slot file appears under `Saved/SaveGames/`.

- [ ] **Step 5: Commit only if Chris asks** — otherwise leave uncommitted on `feature/cleveland-showcase-demo`.

---

### Task G1: Stat tiers → Chaos arcade multipliers at spawn

**Files:**
- Create: `apps/unreal-akron-beta/Source/raceGPSAkronBeta/Public/Garage/GarageStatModel.h`
- Create: `apps/unreal-akron-beta/Source/raceGPSAkronBeta/Private/Garage/GarageStatModel.cpp`
- Modify: `apps/unreal-akron-beta/Source/raceGPSAkronBeta/Private/ChaosVehiclePawn.cpp` (or small helper called from grid)
- Modify: `apps/unreal-akron-beta/Source/raceGPSAkronBeta/Private/RaceGridManager.cpp` after spawn/WakeForDrive one-shot
- Modify: `apps/unreal-akron-beta/Source/raceGPSAkronBeta/Private/CruiseSprintGameMode.cpp` `ApplyVehicleTuningToPlayer` path (optional parity)

**Interfaces:**
- Consumes: `FGarageCarLoadout`, arcade base already applied on pawn
- Produces: `GarageStats::ApplyLoadoutToChaosPawn(AChaosVehiclePawn* Pawn, const FGarageCarLoadout& Loadout)` — clamps tiers 0..5; multiplies MaxTorque, wheel friction, brake torque, mass once; logs tiers

- [ ] **Step 1: Stat tables** — implement float arrays size 6 matching the design doc multipliers.

- [ ] **Step 2: Apply function** — read `UChaosWheeledVehicleMovementComponent`, multiply `EngineSetup.MaxTorque`, friction on wheels, brake strength, `Mass` / inertia as available; **do not** recreate physics every frame; one recreate max only if Chaos requires it after mass change, then stop.

- [ ] **Step 3: Call after Cleveland grid spawn** for player (and optionally AI fixed 2/2/2/2).

- [ ] **Step 4: Debug cheat** — console or temporary `EngineTier=5` override to compare 0 vs 5 in playtest straight.

- [ ] **Step 5: Verify** — playtest: tier 5 reaches higher speed than tier 0 on same Burke segment; no `rebuilt authored wheels` spam.

---

### Task G2: Ability catalog + 2-slot component

**Files:**
- Create: `apps/unreal-akron-beta/Source/raceGPSAkronBeta/Public/Garage/GarageAbilityDefs.h`
- Create: `apps/unreal-akron-beta/Source/raceGPSAkronBeta/Public/Garage/GarageAbilityComponent.h`
- Create: `apps/unreal-akron-beta/Source/raceGPSAkronBeta/Private/Garage/GarageAbilityComponent.cpp`
- Modify: input config `Config/DefaultInput.ini` (Ability1 / Ability2)
- Modify: player pawn setup / `ChaosVehiclePawn::SetupPlayerInputComponent`
- Modify: `UGarageSubsystem` buy/equip APIs

**Interfaces:**
- Consumes: loadout slot FNames
- Produces:
  - `UGarageAbilityComponent::ConfigureSlots(FName A, FName B)`
  - `ActivateSlot(int32 Index)`
  - Cooldown remaining query for UI

- [ ] **Step 1: Define five abilities** in data (Nitro, DriftStick, QuickShift, BrakeStab, HeatDump) with duration/cooldown from design doc.

- [ ] **Step 2: Implement Nitro** — temporary MaxTorque mult (~1.35) + optional forward impulse for 1.5s.

- [ ] **Step 3: Implement DriftStick** — temporary lateral friction reduction + slight yaw assist for 2.5s.

- [ ] **Step 4: Bind Ability1/Ability2**; equip from save; block activate while cooldown > 0; log `GarageAbility fire id=X`.

- [ ] **Step 5: Verify** — PIE/playtest: fire Nitro twice, second blocked until cooldown; car speed spikes; AI unaffected without component.

- [ ] **Step 6: Unlock rule** — unlocking purchase after top-2 Burke finish OR flat cash cost; grant `UnlockedAbilities` and save.

---

### Task G3: Garage shell UI (minimal then polish)

**Files:**
- Create: `Content/UI/Garage/` WBP (or C++ debug HUD first)
- Create: `apps/unreal-akron-beta/Source/raceGPSAkronBeta/Public/Garage/GarageMenuPresenter.h` (+ cpp) if C++ driven
- Modify: Cleveland/CruiseSprint menu flow to open garage before `StartRace`

- [ ] **Step 1: Debug HUD** showing Cash, four tiers, two ability names (always-on during Menu state).

- [ ] **Step 2: Keys/buttons** to buy +1 tier (cost e.g. 500 * nextTier) and cycle ability slots among unlocked.

- [ ] **Step 3: Verify** — change loadout, StartRace, confirm Apply log matches UI.

- [ ] **Step 4: Optional UMG skin** — Midnight Club-ish panel; no blocker for G0–G2 gameplay.

---

### Task G4: Integration checklist (definition of done)

- [ ] Stock tiers 0 / empty abilities: Burke 3-car still completes.
- [ ] Engine 5 visibly faster than Engine 0.
- [ ] Nitro + DriftStick equipped and usable mid-lap.
- [ ] Save survives editor restart.
- [ ] No new Chaos recreate storm in logs.
- [ ] Docs remain accurate if numbers change in playtest (update tables in spec).

---

## Execution gate

**Do not start Task G0 until:** `LaunchCleveland.bat playtest` shows sustained speed (arcade), checkpoints advancing, and ideally `EndRace` / lap complete.

**Plan complete path:** `docs/superpowers/plans/2026-09-03-garage-upgrades-abilities.md`

## Execution options (for Chris)

1. **Subagent-driven** — one agent per task after race loop ships (recommended)
2. **Inline** — same chat, batched with checkpoints
