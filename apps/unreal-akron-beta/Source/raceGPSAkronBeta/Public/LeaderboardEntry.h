#pragma once

#include "CoreMinimal.h"
#include "LeaderboardEntry.generated.h"

USTRUCT(BlueprintType)
struct FLeaderboardEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString PlayerName;

    UPROPERTY(BlueprintReadOnly)
    float TimeSeconds = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    FString Medal;

    UPROPERTY(BlueprintReadOnly)
    FString Date;

    UPROPERTY(BlueprintReadOnly)
    FString VehicleUsed;

    UPROPERTY(BlueprintReadOnly)
    int32 Collisions = 0;

    UPROPERTY(BlueprintReadOnly)
    bool bIsPlayer = false;
};
