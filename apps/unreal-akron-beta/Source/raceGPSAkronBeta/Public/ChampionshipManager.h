// Copyright raceGPS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChampionshipManager.generated.h"

USTRUCT(BlueprintType)
struct FChampionshipEvent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString EventName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString RouteId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString WeatherPreference;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 PointsFirst = 25;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 PointsSecond = 18;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 PointsThird = 15;
};

USTRUCT(BlueprintType)
struct FChampionshipStanding
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString DriverName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 TotalPoints = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Wins = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Podiums = 0;
};

/**
 * Multi-event championship / career mode manager.
 */
UCLASS()
class RACEGPSAKRONBETA_API AChampionshipManager : public AActor
{
    GENERATED_BODY()

public:
    AChampionshipManager();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Championship")
    FString ChampionshipName = TEXT("World Tour");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Championship")
    TArray<FChampionshipEvent> Events;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Championship")
    int32 CurrentEventIndex = 0;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Championship")
    TArray<FChampionshipStanding> Standings;

    UFUNCTION(BlueprintCallable, Category = "Championship")
    void InitializeDefaultChampionship();

    UFUNCTION(BlueprintCallable, Category = "Championship")
    void RecordRaceResult(const FString& DriverName, int32 Position, float BestLapTime);

    UFUNCTION(BlueprintCallable, Category = "Championship")
    void AdvanceToNextEvent();

    UFUNCTION(BlueprintPure, Category = "Championship")
    bool IsChampionshipComplete() const;

    UFUNCTION(BlueprintCallable, Category = "Championship")
    void SaveChampionship(const FString& SlotName);

    UFUNCTION(BlueprintCallable, Category = "Championship")
    void LoadChampionship(const FString& SlotName);

protected:
    virtual void BeginPlay() override;

    UPROPERTY()
    FString SaveDirectory;
};
