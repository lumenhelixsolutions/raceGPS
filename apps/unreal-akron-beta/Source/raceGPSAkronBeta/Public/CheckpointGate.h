#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CheckpointGate.generated.h"

class UBoxComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCheckpointReachedDelegate, int32, CheckpointIndex);

UCLASS()
class RACEGPSAKRONBETA_API ACheckpointGate : public AActor
{
    GENERATED_BODY()

public:
    ACheckpointGate(const FObjectInitializer& ObjectInitializer);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Checkpoint")
    int32 CheckpointIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Checkpoint")
    FLinearColor GateColor = FLinearColor::Green;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Checkpoint")
    float GateWidth = 1600.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Checkpoint")
    float GateHeight = 400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Checkpoint")
    float GateDepth = 200.0f;

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Checkpoint")
    void ActivateGate();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Checkpoint")
    void DeactivateGate();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Checkpoint")
    bool IsActive() const { return bIsActive; }

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Checkpoint")
    void SetGateColor(const FLinearColor& Color);

    UPROPERTY(BlueprintAssignable, Category = "raceGPS|Checkpoint")
    FCheckpointReachedDelegate OnCheckpointReached;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Checkpoint")
    TObjectPtr<UBoxComponent> OverlapBox;

    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                        bool bFromSweep, const FHitResult& SweepResult);

    void BuildVisuals();
    void UpdateVisuals();

private:
    bool bIsActive = false;
    TArray<TObjectPtr<UStaticMeshComponent>> VisualComponents;
};
