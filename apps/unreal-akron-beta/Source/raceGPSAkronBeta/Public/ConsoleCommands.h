#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ConsoleCommands.generated.h"

UCLASS()
class RACEGPSAKRONBETA_API UConsoleCommands : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(Exec, Category = "raceGPS|Cheats")
    void CheatGodMode(bool bEnable);

    UFUNCTION(Exec, Category = "raceGPS|Cheats")
    void CheatSetSpeed(float SpeedKmh);

    UFUNCTION(Exec, Category = "raceGPS|Cheats")
    void CheatTeleportCheckpoint(int32 CheckpointIndex);

    UFUNCTION(Exec, Category = "raceGPS|Cheats")
    void CheatFinishRace();

    UFUNCTION(Exec, Category = "raceGPS|Cheats")
    void CheatSetTime(float TimeSeconds);

    UFUNCTION(Exec, Category = "raceGPS|Debug")
    void DebugShowRoadGraph();

    UFUNCTION(Exec, Category = "raceGPS|Debug")
    void DebugHideRoadGraph();

    UFUNCTION(Exec, Category = "raceGPS|Debug")
    void DebugSpawnVehicle(const FString& VehicleClassName);

    UFUNCTION(Exec, Category = "raceGPS|Debug")
    void DebugSetTimeOfDay(float Hour);

    UFUNCTION(Exec, Category = "raceGPS|Debug")
    void DebugClearTraffic();

    UFUNCTION(Exec, Category = "raceGPS|Debug")
    void DebugReloadCity();

    UFUNCTION(Exec, Category = "raceGPS|Debug")
    void DebugDumpRaceState();
};
