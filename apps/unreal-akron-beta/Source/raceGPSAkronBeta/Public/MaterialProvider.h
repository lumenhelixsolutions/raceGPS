// Copyright raceGPS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "MaterialProvider.generated.h"

UENUM(BlueprintType)
enum class EMasterMaterialType : uint8
{
    Road_Asphalt      UMETA(DisplayName = "Road Asphalt"),
    Road_Marking      UMETA(DisplayName = "Road Marking"),
    Road_Curb         UMETA(DisplayName = "Road Curb"),
    Road_Sidewalk     UMETA(DisplayName = "Road Sidewalk"),
    Building_Glass    UMETA(DisplayName = "Building Glass"),
    Building_Concrete UMETA(DisplayName = "Building Concrete"),
    Building_Brick    UMETA(DisplayName = "Building Brick"),
    Building_Industrial UMETA(DisplayName = "Building Industrial"),
    Building_Residential UMETA(DisplayName = "Building Residential"),
    Vehicle_Paint     UMETA(DisplayName = "Vehicle Paint"),
    Vehicle_Glass_Ext UMETA(DisplayName = "Vehicle Glass External"),
    Vehicle_Glass_Int UMETA(DisplayName = "Vehicle Glass Internal"),
    Vehicle_Lights    UMETA(DisplayName = "Vehicle Lights"),
    Vehicle_Rubber    UMETA(DisplayName = "Vehicle Rubber"),
    Vehicle_Chrome    UMETA(DisplayName = "Vehicle Chrome"),
    Vehicle_LicensePlate UMETA(DisplayName = "Vehicle License Plate"),
    Vegetation_Grass  UMETA(DisplayName = "Vegetation Grass"),
    Vegetation_Tree   UMETA(DisplayName = "Vegetation Tree"),
    Water_Surface     UMETA(DisplayName = "Water Surface"),
    Decal_Oil         UMETA(DisplayName = "Decal Oil"),
    Decal_RoadWear    UMETA(DisplayName = "Decal Road Wear"),
    Default_Fallback  UMETA(DisplayName = "Default Fallback"),
};

/**
 * Provides master materials by type, with automatic fallback to engine defaults.
 * Follows CARLA's material slot naming convention.
 */
UCLASS()
class RACEGPSAKRONBETA_API UMaterialProvider : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "MaterialProvider")
    static UMaterialInterface* GetMasterMaterial(EMasterMaterialType Type);

    UFUNCTION(BlueprintPure, Category = "MaterialProvider")
    static UMaterialInterface* GetDefaultMaterial();

    UFUNCTION(BlueprintCallable, Category = "MaterialProvider")
    static UMaterialInstanceDynamic* CreateMaterialInstance(EMasterMaterialType Type, UObject* Outer);

    UFUNCTION(BlueprintCallable, Category = "MaterialProvider")
    static void PreloadMaterialAsync(EMasterMaterialType Type);

    UFUNCTION(BlueprintPure, Category = "MaterialProvider")
    static FString GetMaterialPath(EMasterMaterialType Type);

    UFUNCTION(BlueprintPure, Category = "MaterialProvider")
    static bool IsMaterialAvailable(EMasterMaterialType Type);

    UFUNCTION(BlueprintCallable, Category = "MaterialProvider")
    static void RegisterMaterialOverride(EMasterMaterialType Type, UMaterialInterface* Override);

private:
    static TMap<EMasterMaterialType, TSoftObjectPtr<UMaterialInterface>> MaterialCache;
    static TMap<EMasterMaterialType, UMaterialInterface*> MaterialOverrides;
    static void InitializeCache();
    static bool bCacheInitialized;
};
