// Copyright raceGPS. All Rights Reserved.

#include "MaterialProvider.h"

TMap<EMasterMaterialType, TSoftObjectPtr<UMaterialInterface>> UMaterialProvider::MaterialCache;
TMap<EMasterMaterialType, UMaterialInterface*> UMaterialProvider::MaterialOverrides;
bool UMaterialProvider::bCacheInitialized = false;

void UMaterialProvider::InitializeCache()
{
    if (bCacheInitialized) return;

    // CARLA-inspired material slot naming convention
    // These paths will resolve once .uasset master materials are imported
    MaterialCache.Add(EMasterMaterialType::Road_Asphalt,       TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_Master_Road_Asphalt.M_Master_Road_Asphalt"))));
    MaterialCache.Add(EMasterMaterialType::Road_Marking,       TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_Master_Road_Marking.M_Master_Road_Marking"))));
    MaterialCache.Add(EMasterMaterialType::Road_Curb,          TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_Master_Road_Curb.M_Master_Road_Curb"))));
    MaterialCache.Add(EMasterMaterialType::Road_Sidewalk,      TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_Master_Road_Sidewalk.M_Master_Road_Sidewalk"))));
    MaterialCache.Add(EMasterMaterialType::Building_Glass,     TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_Master_Building_Glass.M_Master_Building_Glass"))));
    MaterialCache.Add(EMasterMaterialType::Building_Concrete,  TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_Master_Building_Concrete.M_Master_Building_Concrete"))));
    MaterialCache.Add(EMasterMaterialType::Building_Brick,     TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_Master_Building_Brick.M_Master_Building_Brick"))));
    MaterialCache.Add(EMasterMaterialType::Building_Industrial, TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_Master_Building_Industrial.M_Master_Building_Industrial"))));
    MaterialCache.Add(EMasterMaterialType::Building_Residential, TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_Master_Building_Residential.M_Master_Building_Residential"))));
    MaterialCache.Add(EMasterMaterialType::Vehicle_Paint,      TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_Master_Vehicle_Paint.M_Master_Vehicle_Paint"))));
    MaterialCache.Add(EMasterMaterialType::Vehicle_Glass_Ext,  TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_Master_Vehicle_Glass_Ext.M_Master_Vehicle_Glass_Ext"))));
    MaterialCache.Add(EMasterMaterialType::Vehicle_Glass_Int,  TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_Master_Vehicle_Glass_Int.M_Master_Vehicle_Glass_Int"))));
    MaterialCache.Add(EMasterMaterialType::Vehicle_Lights,     TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_Master_Vehicle_Lights.M_Master_Vehicle_Lights"))));
    MaterialCache.Add(EMasterMaterialType::Vehicle_Rubber,     TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_Master_Vehicle_Rubber.M_Master_Vehicle_Rubber"))));
    MaterialCache.Add(EMasterMaterialType::Vehicle_Chrome,     TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_Master_Vehicle_Chrome.M_Master_Vehicle_Chrome"))));
    MaterialCache.Add(EMasterMaterialType::Vehicle_LicensePlate, TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_Master_Vehicle_LicensePlate.M_Master_Vehicle_LicensePlate"))));
    MaterialCache.Add(EMasterMaterialType::Vegetation_Grass,   TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_Master_Vegetation_Grass.M_Master_Vegetation_Grass"))));
    MaterialCache.Add(EMasterMaterialType::Vegetation_Tree,    TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_Master_Vegetation_Tree.M_Master_Vegetation_Tree"))));
    MaterialCache.Add(EMasterMaterialType::Water_Surface,      TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_Master_Water_Surface.M_Master_Water_Surface"))));
    MaterialCache.Add(EMasterMaterialType::Decal_Oil,          TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_Master_Decal_Oil.M_Master_Decal_Oil"))));
    MaterialCache.Add(EMasterMaterialType::Decal_RoadWear,     TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_Master_Decal_RoadWear.M_Master_Decal_RoadWear"))));

    bCacheInitialized = true;
}

UMaterialInterface* UMaterialProvider::GetMasterMaterial(EMasterMaterialType Type)
{
    InitializeCache();

    // Check overrides first
    if (UMaterialInterface** Override = MaterialOverrides.Find(Type))
    {
        if (*Override) return *Override;
    }

    // Try cached path
    if (TSoftObjectPtr<UMaterialInterface>* Cached = MaterialCache.Find(Type))
    {
        if (Cached->IsValid())
        {
            return Cached->Get();
        }
    }

    return GetDefaultMaterial();
}

UMaterialInterface* UMaterialProvider::GetDefaultMaterial()
{
    return UMaterial::GetDefaultMaterial(MD_Surface);
}

UMaterialInstanceDynamic* UMaterialProvider::CreateMaterialInstance(EMasterMaterialType Type, UObject* Outer)
{
    UMaterialInterface* Master = GetMasterMaterial(Type);
    if (!Master) return nullptr;
    return UMaterialInstanceDynamic::Create(Master, Outer);
}

void UMaterialProvider::PreloadMaterialAsync(EMasterMaterialType Type)
{
    InitializeCache();
    if (TSoftObjectPtr<UMaterialInterface>* Cached = MaterialCache.Find(Type))
    {
        Cached->LoadSynchronous();
    }
}

FString UMaterialProvider::GetMaterialPath(EMasterMaterialType Type)
{
    InitializeCache();
    if (TSoftObjectPtr<UMaterialInterface>* Cached = MaterialCache.Find(Type))
    {
        return Cached->ToSoftObjectPath().ToString();
    }
    return FString();
}

bool UMaterialProvider::IsMaterialAvailable(EMasterMaterialType Type)
{
    InitializeCache();
    if (MaterialOverrides.Contains(Type)) return true;
    if (TSoftObjectPtr<UMaterialInterface>* Cached = MaterialCache.Find(Type))
    {
        return Cached->IsValid();
    }
    return false;
}

void UMaterialProvider::RegisterMaterialOverride(EMasterMaterialType Type, UMaterialInterface* Override)
{
    if (Override)
    {
        MaterialOverrides.Add(Type, Override);
    }
    else
    {
        MaterialOverrides.Remove(Type);
    }
}
