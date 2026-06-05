#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "BuildingMeshGenerator.generated.h"

USTRUCT()
struct FBuildingData
{
    GENERATED_BODY()

    UPROPERTY()
    FString Id;

    UPROPERTY()
    FString Type;

    UPROPERTY()
    FString Name;

    UPROPERTY()
    float Height = 8.0f;

    UPROPERTY()
    TArray<FVector2D> Footprint;

    UPROPERTY()
    float AreaM2 = 0.0f;
};

UCLASS()
class RACEGPSAKRONBETA_API ABuildingMeshGenerator : public AActor
{
    GENERATED_BODY()

public:
    ABuildingMeshGenerator();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Buildings")
    FString BuildingsJsonPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Buildings")
    int32 BuildingsPerFrame = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Buildings")
    float MaxDrawDistance = 5000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Buildings")
    UMaterialInterface* MaterialGlass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Buildings")
    UMaterialInterface* MaterialConcrete;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Buildings")
    UMaterialInterface* MaterialBrick;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Buildings")
    UMaterialInterface* MaterialIndustrial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Buildings")
    UMaterialInterface* MaterialResidential;

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Buildings")
    void GenerateBuildingsAsync();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Buildings")
    int32 GetTotalBuildings() const { return Buildings.Num(); }

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Buildings")
    int32 GetGeneratedCount() const { return GeneratedCount; }

protected:
    virtual void Tick(float DeltaTime) override;

    UPROPERTY()
    TObjectPtr<UProceduralMeshComponent> ProceduralMesh;

    UPROPERTY()
    TArray<FBuildingData> Buildings;

    int32 CurrentBuildingIndex = 0;
    int32 GeneratedCount = 0;
    bool bGenerating = false;

    void LoadBuildingsJson();
    void GenerateBuildingBatch(int32 Count);
    void CreateBuildingMesh(const FBuildingData& Building, int32 SectionIndex);
    UMaterialInterface* GetMaterialForType(const FString& Type) const;
};
