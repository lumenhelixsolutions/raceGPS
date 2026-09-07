# URaceSessionManager — additive patch only (do not rewrite)

The Cleveland layer **does not replace** `URaceSessionManager`. Keep existing
`StartSession`, `StartRace`, `TickSession`, `OnCheckpointReached`, states
Menu / Countdown / Racing / Paused / Finished, and `CountdownDuration` default 3.

## Required if missing (function-level)

```cpp
// Accessor used by ARaceAIDriverController / AClevelandShowcaseGameMode.
UFUNCTION(BlueprintPure, Category = "raceGPS|Session")
ERaceSessionState GetState() const { return /* existing state member */; }
```

If the member is already named `State` / `CurrentState` / `SessionState`, just
expose that value. Do not rename the enum.

## Optional (nice for HUD, not required to compile if GetState exists)

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRaceSessionStateChanged, ERaceSessionState, NewState);

UPROPERTY(BlueprintAssignable, Category = "raceGPS|Session")
FOnRaceSessionStateChanged OnStateChanged;
```

Broadcast from the existing transition sites only.

## Do not

- Do not add AI, grid, or Chaos input to this class.
- Do not change countdown timing unless it is not already 3 seconds.
- Do not call `ResetVehicle` / teleport from the session manager.
