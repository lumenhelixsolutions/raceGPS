#include "ClevelandEnvironmentActor.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "ProceduralMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"
#include "UObject/Class.h"
#include "UObject/UObjectIterator.h"
#include "Materials/MaterialInterface.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/UObjectGlobals.h"
#include "Materials/MaterialInstanceDynamic.h"

#if __has_include("RacingLineComponent.h")
#include "RacingLineComponent.h"
#endif

#if __has_include("RaceGPSMaterialProvider.h")
#include "RaceGPSMaterialProvider.h"
#define RACEGPS_HAS_MATERIAL_PROVIDER 1
#else
#define RACEGPS_HAS_MATERIAL_PROVIDER 0
#endif

#if __has_include("DayNightCycle.h")
#include "DayNightCycle.h"
#define RACEGPS_HAS_DAYNIGHT 1
#else
#define RACEGPS_HAS_DAYNIGHT 0
#endif

#if __has_include("BuildingMeshGenerator.h")
#include "BuildingMeshGenerator.h"
#define RACEGPS_HAS_BUILDING_GEN 1
#else
#define RACEGPS_HAS_BUILDING_GEN 0
#endif

namespace
{
	double JsonD(const TSharedPtr<FJsonObject>& Obj, std::initializer_list<const TCHAR*> Keys, double DefaultValue)
	{
		for (const TCHAR* Key : Keys)
		{
			if (Obj.IsValid() && Obj->HasTypedField<EJson::Number>(Key))
			{
				return Obj->GetNumberField(Key);
			}
		}
		return DefaultValue;
	}

	FString JsonS(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, const FString& DefaultValue)
	{
		if (Obj.IsValid() && Obj->HasTypedField<EJson::String>(Key))
		{
			return Obj->GetStringField(Key);
		}
		return DefaultValue;
	}

	void ParseLatLonArray(const TArray<TSharedPtr<FJsonValue>>& Arr, TArray<TPair<double, double>>& Out)
	{
		Out.Reset();
		for (const TSharedPtr<FJsonValue>& Value : Arr)
		{
			const TSharedPtr<FJsonObject> P = Value->AsObject();
			if (!P.IsValid())
			{
				continue;
			}
			Out.Add(TPair<double, double>(
				JsonD(P, {TEXT("lat"), TEXT("latitude")}, 0.0),
				JsonD(P, {TEXT("lon"), TEXT("lng"), TEXT("longitude")}, 0.0)));
		}
	}
}

AClevelandEnvironmentActor::AClevelandEnvironmentActor()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	auto MakeMesh = [this, Root](const TCHAR* Name) -> UProceduralMeshComponent*
	{
		UProceduralMeshComponent* Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(Name);
		Mesh->SetupAttachment(Root);
		Mesh->bUseAsyncCooking = true;
		return Mesh;
	};

	LakeMesh = MakeMesh(TEXT("LakeMesh"));
	GrassMesh = MakeMesh(TEXT("GrassMesh"));
	BarrierMesh = MakeMesh(TEXT("BarrierMesh"));
	ConeMesh = MakeMesh(TEXT("ConeMesh"));
	MarkingMesh = MakeMesh(TEXT("MarkingMesh"));
	SkylineMesh = MakeMesh(TEXT("SkylineMesh"));
	HangarMesh = MakeMesh(TEXT("HangarMesh"));

	HangarPropISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("HangarPropISM"));
	HangarPropISM->SetupAttachment(Root);
	HangarPropISM->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	HangarPropISM->SetCastShadow(true);
	HangarPropISM->SetMobility(EComponentMobility::Static);
	HangarPropISM->NumCustomDataFloats = 0;
}

FVector AClevelandEnvironmentActor::GeoToWorld(double LatDeg, double LonDeg, double RefLatDeg, double RefLonDeg, float HeightCm)
{
#if __has_include("RacingLineComponent.h")
	return URacingLineComponent::GeoToWorld(LatDeg, LonDeg, RefLatDeg, RefLonDeg, HeightCm);
#else
	constexpr double MetersPerDegLat = 111320.0;
	const double RefLatRad = FMath::DegreesToRadians(RefLatDeg);
	const double MetersPerDegLon = MetersPerDegLat * FMath::Cos(RefLatRad);
	const double EastM = (LonDeg - RefLonDeg) * MetersPerDegLon;
	const double NorthM = (LatDeg - RefLatDeg) * MetersPerDegLat;
	return FVector(static_cast<float>(EastM * 100.0), static_cast<float>(NorthM * 100.0), HeightCm);
#endif
}

FString AClevelandEnvironmentActor::ResolveCityPackFile(const FString& FileName) const
{
	const TArray<FString> Roots = {
		FPaths::Combine(FPaths::ProjectDir(), CityPackRelativeDir),
		FPaths::Combine(FPaths::ProjectContentDir(), CityPackRelativeDir),
		FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Dir"), CityPackRelativeDir),
		FPaths::Combine(FPaths::ProjectContentDir(), TEXT("citypacks/cleveland/burke_gp_1997")),
		FPaths::ProjectContentDir()
	};
	for (const FString& RootPath : Roots)
	{
		const FString Path = FPaths::Combine(RootPath, FileName);
		if (FPaths::FileExists(Path))
		{
			return Path;
		}
	}
	return FPaths::Combine(FPaths::ProjectDir(), CityPackRelativeDir, FileName);
}

bool AClevelandEnvironmentActor::LoadJsonFile(const FString& FileName, TSharedPtr<FJsonObject>& OutRoot) const
{
	OutRoot.Reset();
	const FString Path = ResolveCityPackFile(FileName);
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *Path))
	{
		UE_LOG(LogTemp, Warning, TEXT("raceGPS Cleveland env: missing %s"), *Path);
		return false;
	}
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, OutRoot) || !OutRoot.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("raceGPS Cleveland env: invalid JSON %s"), *Path);
		return false;
	}
	return true;
}

UMaterialInterface* AClevelandEnvironmentActor::ResolveMaterial(const TCHAR* SlotName) const
{
#if RACEGPS_HAS_MATERIAL_PROVIDER
	// Map showcase slot names onto EMasterMaterialType + GetMasterMaterial.
	const FString S(SlotName);
	EMasterMaterialType Type = EMasterMaterialType::Default_Fallback;
	if (S.Equals(TEXT("Water_Surface"), ESearchCase::IgnoreCase)) Type = EMasterMaterialType::Water_Surface;
	else if (S.Equals(TEXT("Vegetation_Grass"), ESearchCase::IgnoreCase)) Type = EMasterMaterialType::Vegetation_Grass;
	else if (S.Equals(TEXT("Road_Asphalt"), ESearchCase::IgnoreCase)) Type = EMasterMaterialType::Road_Asphalt;
	else if (S.Equals(TEXT("Road_Marking"), ESearchCase::IgnoreCase)) Type = EMasterMaterialType::Road_Marking;
	else if (S.Equals(TEXT("Building_Concrete"), ESearchCase::IgnoreCase)) Type = EMasterMaterialType::Building_Concrete;
	else if (S.Equals(TEXT("Building_Glass"), ESearchCase::IgnoreCase)) Type = EMasterMaterialType::Building_Glass;
	if (UMaterialInterface* Mat = URaceGPSMaterialProvider::GetMasterMaterial(Type))
	{
		return Mat;
	}
#endif
	(void)SlotName;
	return nullptr;
}

void AClevelandEnvironmentActor::EnsureDayNightCycle()
{
#if RACEGPS_HAS_DAYNIGHT
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	TArray<AActor*> Existing;
	UGameplayStatics::GetAllActorsOfClass(World, ADayNightCycle::StaticClass(), Existing);
	if (Existing.Num() > 0)
	{
		return;
	}
	ADayNightCycle* Cycle = World->SpawnActor<ADayNightCycle>(ADayNightCycle::StaticClass());
	if (!Cycle)
	{
		UE_LOG(LogTemp, Warning, TEXT("raceGPS Cleveland env: failed to spawn ADayNightCycle"));
		return;
	}
	// 14:00 day demo. Names vary by branch; try the common setters.
	Cycle->SetTimeOfDay(DayNightHours);
	UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland env: spawned ADayNightCycle at %.1f h"), DayNightHours);
#else
	UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland env: ADayNightCycle header not in this compile; skip spawn (document noon lighting in level)."));
#endif
}


namespace
{
	bool SetObjBool(UObject* Obj, const FName Name, bool Value)
	{
		if (!Obj)
		{
			return false;
		}
		if (FBoolProperty* Prop = FindFProperty<FBoolProperty>(Obj->GetClass(), Name))
		{
			Prop->SetPropertyValue_InContainer(Obj, Value);
			return true;
		}
		return false;
	}

	bool SetObjInt64(UObject* Obj, const FName Name, int64 Value)
	{
		if (!Obj)
		{
			return false;
		}
		if (FInt64Property* Prop = FindFProperty<FInt64Property>(Obj->GetClass(), Name))
		{
			Prop->SetPropertyValue_InContainer(Obj, Value);
			return true;
		}
		if (FIntProperty* Prop32 = FindFProperty<FIntProperty>(Obj->GetClass(), Name))
		{
			Prop32->SetPropertyValue_InContainer(Obj, static_cast<int32>(Value));
			return true;
		}
		return false;
	}

	bool CallSetOriginLLH(AActor* Geo, double Lon, double Lat, double HeightM)
	{
		if (!Geo)
		{
			return false;
		}
		UFunction* Fn = Geo->FindFunction(TEXT("SetOriginLongitudeLatitudeHeight"));
		if (!Fn)
		{
			return false;
		}
		struct FLLHParams
		{
			FVector TargetLongitudeLatitudeHeight;
		};
		FLLHParams Params;
		Params.TargetLongitudeLatitudeHeight = FVector(Lon, Lat, HeightM);
		Geo->ProcessEvent(Fn, &Params);
		return true;
	}

	bool CallSetIonAssetID(AActor* Tileset, int64 AssetId)
	{
		if (!Tileset)
		{
			return false;
		}
		if (UFunction* Fn = Tileset->FindFunction(TEXT("SetIonAssetID")))
		{
			struct FIdParams { int64 InAssetID; };
			FIdParams Params{AssetId};
			Tileset->ProcessEvent(Fn, &Params);
			return true;
		}
		return SetObjInt64(Tileset, TEXT("IonAssetID"), AssetId);
	}

	bool CallSetCreatePhysics(AActor* Tileset, bool bCreate)
	{
		if (!Tileset)
		{
			return false;
		}
		if (UFunction* Fn = Tileset->FindFunction(TEXT("SetCreatePhysicsMeshes")))
		{
			struct FBoolParams { bool bCreatePhysicsMeshes; };
			FBoolParams Params{bCreate};
			Tileset->ProcessEvent(Fn, &Params);
			return true;
		}
		return SetObjBool(Tileset, TEXT("CreatePhysicsMeshes"), bCreate);
	}
}

void AClevelandEnvironmentActor::EnsureCesiumSpike()
{
	if (!bEnableCesiumSkyline)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			if (UClass* TilesetClass = LoadClass<AActor>(nullptr, TEXT("/Script/CesiumRuntime.Cesium3DTileset")))
			{
				TArray<AActor*> Tilesets;
				UGameplayStatics::GetAllActorsOfClass(World, TilesetClass, Tilesets);
				for (AActor* T : Tilesets)
				{
					if (!T) { continue; }
					T->SetActorTickEnabled(false);
					T->SetActorHiddenInGame(true);
					T->SetActorEnableCollision(false);
					if (FBoolProperty* Sus = FindFProperty<FBoolProperty>(T->GetClass(), TEXT("SuspendUpdate")))
					{
						Sus->SetPropertyValue_InContainer(T, true);
					}
				}
				UE_LOG(LogTemp, Warning, TEXT("raceGPS Cesium: V15 disabled; quieted existing tilesets=%d"), Tilesets.Num());
			}
		}
		UE_LOG(LogTemp, Log, TEXT("raceGPS Cesium: skipped spawn (bEnableCesiumSkyline=0)"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UClass* GeoClass = LoadClass<AActor>(nullptr, TEXT("/Script/CesiumRuntime.CesiumGeoreference"));
	UClass* TilesetClass = LoadClass<AActor>(nullptr, TEXT("/Script/CesiumRuntime.Cesium3DTileset"));
	if (!GeoClass || !TilesetClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("raceGPS Cesium: plugin classes missing (Geo=%d Tileset=%d). Enable CesiumForUnreal in .uproject and restart editor."),
			GeoClass ? 1 : 0, TilesetClass ? 1 : 0);
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Name = TEXT("ClevelandCesiumGeoreference");
	Params.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;

	AActor* Geo = nullptr;
	{
		TArray<AActor*> Existing;
		UGameplayStatics::GetAllActorsOfClass(World, GeoClass, Existing);
		if (Existing.Num() > 0)
		{
			Geo = Existing[0];
		}
	}
	if (!Geo)
	{
		Geo = World->SpawnActor<AActor>(GeoClass, FVector::ZeroVector, FRotator::ZeroRotator, Params);
	}
	if (!Geo)
	{
		UE_LOG(LogTemp, Error, TEXT("raceGPS Cesium: failed to spawn CesiumGeoreference"));
		return;
	}

	const bool bOrigin = CallSetOriginLLH(Geo, OriginLon, OriginLat, OriginHeightMeters);
	UE_LOG(LogTemp, Warning,
		TEXT("raceGPS Cesium: georef origin lat=%.5f lon=%.5f height=%.1fm (citypack Burke) set=%d actor=%s"),
		OriginLat, OriginLon, OriginHeightMeters, bOrigin ? 1 : 0, *Geo->GetName());

	// Do not spawn CesiumSunSky: MidnightRun + ADayNightCycle stay hero lighting.
	if (UClass* SunSkyClass = LoadClass<AActor>(nullptr, TEXT("/Script/CesiumRuntime.CesiumSunSky")))
	{
		TArray<AActor*> Suns;
		UGameplayStatics::GetAllActorsOfClass(World, SunSkyClass, Suns);
		for (AActor* S : Suns)
		{
			if (!S)
			{
				continue;
			}
			S->SetActorHiddenInGame(true);
			S->SetActorTickEnabled(false);
			S->SetActorEnableCollision(false);
			UE_LOG(LogTemp, Log, TEXT("raceGPS Cesium: subdued CesiumSunSky %s (keep DayNightCycle)"), *S->GetName());
		}
	}

	Params.Name = TEXT("ClevelandCesiumOsmBuildings");
	AActor* Tileset = nullptr;
	{
		TArray<AActor*> Existing;
		UGameplayStatics::GetAllActorsOfClass(World, TilesetClass, Existing);
		for (AActor* A : Existing)
		{
			if (A && A->GetName().Contains(TEXT("Osm"), ESearchCase::IgnoreCase))
			{
				Tileset = A;
				break;
			}
		}
		if (!Tileset && Existing.Num() > 0)
		{
			Tileset = Existing[0];
		}
	}
	if (!Tileset)
	{
		Tileset = World->SpawnActor<AActor>(TilesetClass, FVector::ZeroVector, FRotator::ZeroRotator, Params);
	}
	if (!Tileset)
	{
		UE_LOG(LogTemp, Error, TEXT("raceGPS Cesium: failed to spawn Cesium3DTileset"));
		return;
	}

	const bool bId = CallSetIonAssetID(Tileset, CesiumOsmBuildingsAssetId);
	CallSetCreatePhysics(Tileset, false);
	SetObjBool(Tileset, TEXT("ShowCreditsOnScreen"), false);
	if (UFunction* Sse = Tileset->FindFunction(TEXT("SetMaximumScreenSpaceError")))
	{
		struct FSSE { double InMaximumScreenSpaceError; };
		FSSE P{12.0};
		Tileset->ProcessEvent(Sse, &P);
	}

	FString Token;
	if (FStrProperty* TokProp = FindFProperty<FStrProperty>(Tileset->GetClass(), TEXT("IonAccessToken")))
	{
		Token = TokProp->GetPropertyValue_InContainer(Tileset);
	}
	const bool bServerToken = false;

	UE_LOG(LogTemp, Warning,
		TEXT("raceGPS Cesium: OSM Buildings asset=%lld setId=%d physicsOff tileset=%s tokenChars=%d serverToken=%d"),
		static_cast<long long>(CesiumOsmBuildingsAssetId), bId ? 1 : 0, *Tileset->GetName(),
		Token.Len(), bServerToken ? 1 : 0);

	if (Token.IsEmpty() && !bServerToken)
	{
		UE_LOG(LogTemp, Error,
			TEXT("raceGPS Cesium: BLOCKER - no Cesium ion token. Chris must open Unreal Editor (not -game), Window > Cesium, click Connect to Cesium ion, sign in, then create/select a token that can access OSM Buildings (asset 96188) and optionally Google Photorealistic 3D Tiles (2275207). Token is stored at /Game/CesiumSettings/CesiumIonServers/CesiumIonSaaS. Do not invent a token. Georeference is already set to Burke." ));
	}

	if (UFunction* Refresh = Tileset->FindFunction(TEXT("RefreshTileset")))
	{
		Tileset->ProcessEvent(Refresh, nullptr);
	}
}

void AClevelandEnvironmentActor::BeginPlay()
{
	Super::BeginPlay();
	EnsureDayNightCycle();
	LoadAndBuild();
	EnsureCesiumSpike();
}

bool AClevelandEnvironmentActor::LoadAndBuild()
{
	TSharedPtr<FJsonObject> Env;
	TSharedPtr<FJsonObject> Water;
	TSharedPtr<FJsonObject> Skyline;
	TSharedPtr<FJsonObject> Dressing;
	LoadJsonFile(TEXT("environment.json"), Env);
	const bool bWater = LoadJsonFile(TEXT("water.json"), Water);
	const bool bSky = LoadJsonFile(TEXT("skyline.json"), Skyline);
	const bool bDress = LoadJsonFile(TEXT("track_dressing.json"), Dressing);

	if (Env.IsValid())
	{
		const TSharedPtr<FJsonObject>* Origin = nullptr;
		const TSharedPtr<FJsonObject>* Geo = nullptr;
		if (Env->TryGetObjectField(TEXT("geo"), Geo) && Geo && (*Geo)->TryGetObjectField(TEXT("origin"), Origin) && Origin)
		{
			OriginLat = JsonD(*Origin, {TEXT("lat")}, OriginLat);
			OriginLon = JsonD(*Origin, {TEXT("lon")}, OriginLon);
		}
	}

	if (bWater)
	{
		BuildLake(Water);
	}
	if (bDress)
	{
		BuildGrass(Dressing);
		BuildBarriers(Dressing);
		BuildCones(Dressing);
		BuildStartFinish(Dressing);
		BuildHangars(Dressing);
		BuildRunwayTaxiwayDecals(Dressing);
	}
	if (bSky)
	{
		BuildSkyline(Skyline);
	}

	UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland env: built lake=%d skyline_buildings=%d named_towers=%d dressing=%d (Karla additive; T10 HISM city is in-map)"),
		bWater ? 1 : 0, LastSkylineBuildingCount, LastNamedTowerCount, bDress ? 1 : 0);
	return bWater || bSky || bDress;
}

void AClevelandEnvironmentActor::CommitSection(UProceduralMeshComponent* Mesh, int32 Section,
	const TArray<FVector>& Verts, const TArray<int32>& Tris, const TArray<FVector>& Normals,
	const TArray<FVector2D>& UV, const TCHAR* MaterialSlot)
{
	if (!Mesh || Verts.Num() == 0 || Tris.Num() < 3)
	{
		return;
	}
	TArray<FProcMeshTangent> EmptyTangents;
	TArray<FLinearColor> EmptyColors;
	Mesh->CreateMeshSection_LinearColor(Section, Verts, Tris, Normals, UV, EmptyColors, EmptyTangents, true);
	if (UMaterialInterface* Mat = ResolveMaterial(MaterialSlot))
	{
		Mesh->SetMaterial(Section, Mat);
	}
}

void AClevelandEnvironmentActor::AppendPolygonXZ(TArray<FVector>& Verts, TArray<int32>& Tris,
	TArray<FVector>& Normals, TArray<FVector2D>& UV, const TArray<FVector>& Ring, float ZCm)
{
	if (Ring.Num() < 3)
	{
		return;
	}
	const int32 Base = Verts.Num();
	for (int32 i = 0; i < Ring.Num(); ++i)
	{
		const FVector P(Ring[i].X, Ring[i].Y, ZCm);
		Verts.Add(P);
		Normals.Add(FVector::UpVector);
		UV.Add(FVector2D(P.X * 0.001f, P.Y * 0.001f));
	}
	for (int32 i = 1; i + 1 < Ring.Num(); ++i)
	{
		Tris.Add(Base);
		Tris.Add(Base + i);
		Tris.Add(Base + i + 1);
	}
}

void AClevelandEnvironmentActor::AppendBox(TArray<FVector>& Verts, TArray<int32>& Tris, TArray<FVector>& Normals,
	TArray<FVector2D>& UV, const FVector& Center, const FVector& Extent, float YawDeg)
{
	const float Yaw = FMath::DegreesToRadians(YawDeg);
	const float C = FMath::Cos(Yaw);
	const float S = FMath::Sin(Yaw);
	auto Rot = [C, S](float X, float Y) {
		return FVector(X * C - Y * S, X * S + Y * C, 0.f);
	};
	const FVector Corners[8] = {
		Center + Rot(-Extent.X, -Extent.Y) + FVector(0, 0, -Extent.Z),
		Center + Rot( Extent.X, -Extent.Y) + FVector(0, 0, -Extent.Z),
		Center + Rot( Extent.X,  Extent.Y) + FVector(0, 0, -Extent.Z),
		Center + Rot(-Extent.X,  Extent.Y) + FVector(0, 0, -Extent.Z),
		Center + Rot(-Extent.X, -Extent.Y) + FVector(0, 0,  Extent.Z),
		Center + Rot( Extent.X, -Extent.Y) + FVector(0, 0,  Extent.Z),
		Center + Rot( Extent.X,  Extent.Y) + FVector(0, 0,  Extent.Z),
		Center + Rot(-Extent.X,  Extent.Y) + FVector(0, 0,  Extent.Z),
	};
	const int32 Faces[6][4] = {
		{0, 1, 2, 3}, {4, 7, 6, 5}, {0, 4, 5, 1}, {1, 5, 6, 2}, {2, 6, 7, 3}, {3, 7, 4, 0}
	};
	const FVector FaceN[6] = {
		FVector(0, 0, -1), FVector(0, 0, 1),
		Rot(0, -1).GetSafeNormal(), Rot(1, 0).GetSafeNormal(),
		Rot(0, 1).GetSafeNormal(), Rot(-1, 0).GetSafeNormal()
	};
	for (int32 f = 0; f < 6; ++f)
	{
		const int32 Base = Verts.Num();
		for (int32 k = 0; k < 4; ++k)
		{
			Verts.Add(Corners[Faces[f][k]]);
			Normals.Add(FaceN[f]);
			UV.Add(FVector2D(static_cast<float>(k % 2), static_cast<float>(k / 2)));
		}
		Tris.Add(Base); Tris.Add(Base + 1); Tris.Add(Base + 2);
		Tris.Add(Base); Tris.Add(Base + 2); Tris.Add(Base + 3);
	}
}

void AClevelandEnvironmentActor::AppendCone(TArray<FVector>& Verts, TArray<int32>& Tris, TArray<FVector>& Normals,
	TArray<FVector2D>& UV, const FVector& Base, float RadiusCm, float HeightCm, int32 Sides)
{
	const int32 ApexIndex = Verts.Num();
	const FVector Apex = Base + FVector(0.f, 0.f, HeightCm);
	Verts.Add(Apex);
	Normals.Add(FVector::UpVector);
	UV.Add(FVector2D(0.5f, 1.f));
	const int32 RingStart = Verts.Num();
	for (int32 i = 0; i < Sides; ++i)
	{
		const float A = 2.f * PI * static_cast<float>(i) / static_cast<float>(Sides);
		const FVector P = Base + FVector(FMath::Cos(A) * RadiusCm, FMath::Sin(A) * RadiusCm, 0.f);
		Verts.Add(P);
		Normals.Add((P - Base).GetSafeNormal());
		UV.Add(FVector2D(static_cast<float>(i) / static_cast<float>(Sides), 0.f));
	}
	for (int32 i = 0; i < Sides; ++i)
	{
		const int32 I0 = RingStart + i;
		const int32 I1 = RingStart + ((i + 1) % Sides);
		Tris.Add(ApexIndex);
		Tris.Add(I0);
		Tris.Add(I1);
	}
}

void AClevelandEnvironmentActor::BuildLake(const TSharedPtr<FJsonObject>& Water)
{
	TArray<TPair<double, double>> Pts;
	if (Water->HasTypedField<EJson::Array>(TEXT("points")))
	{
		ParseLatLonArray(Water->GetArrayField(TEXT("points")), Pts);
	}
	TArray<FVector> Ring;
	for (const TPair<double, double>& P : Pts)
	{
		Ring.Add(GeoToWorld(P.Key, P.Value, OriginLat, OriginLon, -20.f));
	}

	// Expand the north (+Y) edge so Erie still reads from the raised 3/4 chase / intro cam.
	if (Ring.Num() >= 3)
	{
		float MeanY = 0.f;
		for (const FVector& V : Ring)
		{
			MeanY += V.Y;
		}
		MeanY /= static_cast<float>(Ring.Num());
		for (FVector& V : Ring)
		{
			if (V.Y > MeanY)
			{
				V.Y += 140000.f; // +1.4 km offshore horizon fill
				V.X *= 1.18f;    // slightly wider east-west sheet
			}
		}
	}

	TArray<FVector> Verts, Normals;
	TArray<int32> Tris;
	TArray<FVector2D> UV;
	AppendPolygonXZ(Verts, Tris, Normals, UV, Ring, -20.f);
	CommitSection(LakeMesh, 0, Verts, Tris, Normals, UV, TEXT("Water_Surface"));
	if (LakeMesh)
	{
		LakeMesh->SetCastShadow(false);
		LakeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (UMaterialInstanceDynamic* Mid = MakeLookMID(TEXT("Water_Surface"), false, false))
	{
		ApplyCommonMIDParams(Mid, true, false, false);
		if (LakeMesh)
		{
			LakeMesh->SetMaterial(0, Mid);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland env: lake verts=%d (expanded north for chase/cinematic read)"), Verts.Num());
}

void AClevelandEnvironmentActor::BuildGrass(const TSharedPtr<FJsonObject>& Dressing)
{
	if (!Dressing->HasTypedField<EJson::Array>(TEXT("infield_grass")))
	{
		return;
	}
	TArray<FVector> Verts, Normals;
	TArray<int32> Tris;
	TArray<FVector2D> UV;
	int32 SectionFan = 0;
	(void)SectionFan;
	for (const TSharedPtr<FJsonValue>& Value : Dressing->GetArrayField(TEXT("infield_grass")))
	{
		const TSharedPtr<FJsonObject> Poly = Value->AsObject();
		if (!Poly.IsValid() || !Poly->HasTypedField<EJson::Array>(TEXT("points")))
		{
			continue;
		}
		TArray<TPair<double, double>> Pts;
		ParseLatLonArray(Poly->GetArrayField(TEXT("points")), Pts);
		TArray<FVector> Ring;
		for (const TPair<double, double>& P : Pts)
		{
			Ring.Add(GeoToWorld(P.Key, P.Value, OriginLat, OriginLon, 2.f));
		}
		AppendPolygonXZ(Verts, Tris, Normals, UV, Ring, 2.f);
	}
	CommitSection(GrassMesh, 0, Verts, Tris, Normals, UV, TEXT("Vegetation_Grass"));
}

void AClevelandEnvironmentActor::BuildBarriers(const TSharedPtr<FJsonObject>& Dressing)
{
	if (!Dressing->HasTypedField<EJson::Array>(TEXT("barriers")))
	{
		return;
	}
	TArray<FVector> VertsC, NormalsC, VertsT, NormalsT;
	TArray<int32> TrisC, TrisT;
	TArray<FVector2D> UVC, UVT;
	for (const TSharedPtr<FJsonValue>& Value : Dressing->GetArrayField(TEXT("barriers")))
	{
		const TSharedPtr<FJsonObject> B = Value->AsObject();
		if (!B.IsValid())
		{
			continue;
		}
		const double Lat = JsonD(B, {TEXT("lat")}, 0.0);
		const double Lon = JsonD(B, {TEXT("lon")}, 0.0);
		const float Yaw = static_cast<float>(JsonD(B, {TEXT("yaw_deg")}, 0.0));
		const float Len = static_cast<float>(JsonD(B, {TEXT("length_m")}, 7.5) * 100.0 * 0.5);
		const float Wid = static_cast<float>(JsonD(B, {TEXT("width_m")}, 0.55) * 100.0 * 0.5);
		const float Ht = static_cast<float>(JsonD(B, {TEXT("height_m")}, 1.0) * 100.0);
		const FVector Center = GeoToWorld(Lat, Lon, OriginLat, OriginLon, Ht * 0.5f);
		const FVector Extent(Len, Wid, Ht * 0.5f);
		const FString Type = JsonS(B, TEXT("type"), TEXT("concrete"));
		if (Type.Equals(TEXT("tire"), ESearchCase::IgnoreCase))
		{
			AppendBox(VertsT, TrisT, NormalsT, UVT, Center, Extent, Yaw);
		}
		else
		{
			AppendBox(VertsC, TrisC, NormalsC, UVC, Center, Extent, Yaw);
		}
	}
	CommitSection(BarrierMesh, 0, VertsC, TrisC, NormalsC, UVC, TEXT("Building_Concrete"));
	CommitSection(BarrierMesh, 1, VertsT, TrisT, NormalsT, UVT, TEXT("Road_Asphalt"));
}

void AClevelandEnvironmentActor::BuildCones(const TSharedPtr<FJsonObject>& Dressing)
{
	if (!Dressing->HasTypedField<EJson::Array>(TEXT("cones")))
	{
		return;
	}
	TArray<FVector> Verts, Normals;
	TArray<int32> Tris;
	TArray<FVector2D> UV;
	int32 ConeCount = 0;
	const int32 ConeCap = 360;
	for (const TSharedPtr<FJsonValue>& Value : Dressing->GetArrayField(TEXT("cones")))
	{
		const TSharedPtr<FJsonObject> C = Value->AsObject();
		if (!C.IsValid())
		{
			continue;
		}
		++ConeCount;
		if (ConeCount > ConeCap)
		{
			break;
		}
		const FVector Base = GeoToWorld(
			JsonD(C, {TEXT("lat")}, 0.0), JsonD(C, {TEXT("lon")}, 0.0), OriginLat, OriginLon, 0.f);
		const float Ht = static_cast<float>(JsonD(C, {TEXT("height_m")}, 0.5) * 100.0);
		AppendCone(Verts, Tris, Normals, UV, Base, 18.f, Ht, 8);
	}
	CommitSection(ConeMesh, 0, Verts, Tris, Normals, UV, TEXT("Road_Marking"));
}

void AClevelandEnvironmentActor::BuildStartFinish(const TSharedPtr<FJsonObject>& Dressing)
{
	const TSharedPtr<FJsonObject>* SF = nullptr;
	if (!Dressing->TryGetObjectField(TEXT("start_finish"), SF) || !SF)
	{
		return;
	}
	const double Lat = JsonD(*SF, {TEXT("lat")}, 0.0);
	const double Lon = JsonD(*SF, {TEXT("lon")}, 0.0);
	const float Yaw = static_cast<float>(JsonD(*SF, {TEXT("heading_deg")}, 0.0));
	const float Width = static_cast<float>(JsonD(*SF, {TEXT("width_m")}, 12.0) * 100.0 * 0.5);
	const float Depth = static_cast<float>(JsonD(*SF, {TEXT("depth_m")}, 1.2) * 100.0 * 0.5);
	const FVector Center = GeoToWorld(Lat, Lon, OriginLat, OriginLon, 3.f);
	TArray<FVector> Verts, Normals;
	TArray<int32> Tris;
	TArray<FVector2D> UV;
	AppendBox(Verts, Tris, Normals, UV, Center, FVector(Depth, Width, 3.f), Yaw);
	CommitSection(MarkingMesh, 0, Verts, Tris, Normals, UV, TEXT("Road_Marking"));
}

void AClevelandEnvironmentActor::BuildHangars(const TSharedPtr<FJsonObject>& Dressing)
{
	if (!Dressing->HasTypedField<EJson::Array>(TEXT("airport_boxes")))
	{
		return;
	}

	// Offline CARLA static meshes only (no CARLA server). Content/Carla/Static
	// currently ships Car / GenericMaterials / Truck — hangar uassets are absent
	// so this LoadObject path falls through to procedural boxes.
	static const TCHAR* kCarlaHangarCandidates[] = {
		TEXT("/Game/Carla/Static/Buildings/SM_Hangar_01"),
		TEXT("/Game/Carla/Static/Buildings/SM_HangarLarge"),
		TEXT("/Game/Carla/Static/Props/SM_AirportHangar"),
		TEXT("/Game/Carla/Static/StaticMesh/SM_Hangar"),
		TEXT("/Game/Carla/Static/Architecture/SM_Hangar"),
	};
	auto TryLoadMesh = [](const FString& Path) -> UStaticMesh*
	{
		if (Path.IsEmpty())
		{
			return nullptr;
		}
		return LoadObject<UStaticMesh>(nullptr, *Path);
	};

	UStaticMesh* SharedCarla = nullptr;
	if (Dressing->HasTypedField<EJson::Array>(TEXT("hangar_mesh_candidates")))
	{
		for (const TSharedPtr<FJsonValue>& V : Dressing->GetArrayField(TEXT("hangar_mesh_candidates")))
		{
			SharedCarla = TryLoadMesh(V->AsString());
			if (SharedCarla)
			{
				break;
			}
		}
	}
	if (!SharedCarla)
	{
		for (const TCHAR* Cand : kCarlaHangarCandidates)
		{
			SharedCarla = TryLoadMesh(FString(Cand));
			if (SharedCarla)
			{
				break;
			}
		}
	}
	if (HangarPropISM)
	{
		HangarPropISM->ClearInstances();
		if (SharedCarla)
		{
			HangarPropISM->SetStaticMesh(SharedCarla);
		}
	}

	TArray<FVector> Verts, Normals;
	TArray<int32> Tris;
	TArray<FVector2D> UV;
	int32 Count = 0;
	int32 Instanced = 0;
	const int32 Cap = FMath::Clamp(HangarBoxCap, 4, 24);
	for (const TSharedPtr<FJsonValue>& Value : Dressing->GetArrayField(TEXT("airport_boxes")))
	{
		const TSharedPtr<FJsonObject> H = Value->AsObject();
		if (!H.IsValid())
		{
			continue;
		}
		++Count;
		if (Count > Cap)
		{
			break;
		}
		const float Ht = static_cast<float>(JsonD(H, {TEXT("height_m")}, 10.0) * 100.0);
		const float W = static_cast<float>(JsonD(H, {TEXT("width_m")}, 40.0) * 100.0 * 0.5);
		const float D = static_cast<float>(JsonD(H, {TEXT("depth_m")}, 24.0) * 100.0 * 0.5);
		const float Yaw = static_cast<float>(JsonD(H, {TEXT("yaw_deg")}, 0.0));
		const FVector Center = GeoToWorld(
			JsonD(H, {TEXT("lat")}, 0.0), JsonD(H, {TEXT("lon")}, 0.0), OriginLat, OriginLon, Ht * 0.5f);

		UStaticMesh* Mesh = TryLoadMesh(JsonS(H, TEXT("mesh_path"), FString()));
		if (!Mesh)
		{
			Mesh = SharedCarla;
		}
		if (Mesh && HangarPropISM)
		{
			if (HangarPropISM->GetStaticMesh() != Mesh && Instanced == 0)
			{
				HangarPropISM->SetStaticMesh(Mesh);
			}
			if (HangarPropISM->GetStaticMesh() == Mesh)
			{
				const FBoxSphereBounds B = Mesh->GetBounds();
				FVector Scale(1.f);
				if (B.BoxExtent.X > 1.f)
				{
					Scale.X = W / B.BoxExtent.X;
				}
				if (B.BoxExtent.Y > 1.f)
				{
					Scale.Y = D / B.BoxExtent.Y;
				}
				if (B.BoxExtent.Z > 1.f)
				{
					Scale.Z = (Ht * 0.5f) / B.BoxExtent.Z;
				}
				FTransform Xf;
				Xf.SetLocation(Center);
				Xf.SetRotation(FQuat(FRotator(0.f, Yaw, 0.f)));
				Xf.SetScale3D(Scale);
				HangarPropISM->AddInstance(Xf);
				++Instanced;
				continue;
			}
		}
		AppendBox(Verts, Tris, Normals, UV, Center, FVector(W, D, Ht * 0.5f), Yaw);
	}
	CommitSection(HangarMesh, 0, Verts, Tris, Normals, UV, TEXT("Building_Concrete"));
	UE_LOG(LogTemp, Log, TEXT("[raceGPS] hangars instanced=%d boxed=%d carla_mesh=%s"),
		Instanced, Count - Instanced, SharedCarla ? TEXT("yes") : TEXT("no"));
}

void AClevelandEnvironmentActor::BuildRunwayTaxiwayDecals(const TSharedPtr<FJsonObject>& Dressing)
{
	auto BuildAabb = [this](const TArray<TSharedPtr<FJsonValue>>& Arr, UProceduralMeshComponent* Mesh, int32 Section, const TCHAR* Slot, float ZCm)
	{
		TArray<FVector> Verts, Normals;
		TArray<int32> Tris;
		TArray<FVector2D> UV;
		for (const TSharedPtr<FJsonValue>& Value : Arr)
		{
			const TSharedPtr<FJsonObject> R = Value->AsObject();
			if (!R.IsValid())
			{
				continue;
			}
			const double MinLat = JsonD(R, {TEXT("min_lat")}, 0.0);
			const double MaxLat = JsonD(R, {TEXT("max_lat")}, 0.0);
			const double MinLon = JsonD(R, {TEXT("min_lon")}, 0.0);
			const double MaxLon = JsonD(R, {TEXT("max_lon")}, 0.0);
			TArray<FVector> Ring;
			Ring.Add(GeoToWorld(MinLat, MinLon, OriginLat, OriginLon, ZCm));
			Ring.Add(GeoToWorld(MinLat, MaxLon, OriginLat, OriginLon, ZCm));
			Ring.Add(GeoToWorld(MaxLat, MaxLon, OriginLat, OriginLon, ZCm));
			Ring.Add(GeoToWorld(MaxLat, MinLon, OriginLat, OriginLon, ZCm));
			AppendPolygonXZ(Verts, Tris, Normals, UV, Ring, ZCm);
		}
		CommitSection(Mesh, Section, Verts, Tris, Normals, UV, Slot);
	};
	if (Dressing->HasTypedField<EJson::Array>(TEXT("runway_regions")))
	{
		BuildAabb(Dressing->GetArrayField(TEXT("runway_regions")), MarkingMesh, 1, TEXT("Road_Asphalt"), 1.f);
	}
	if (Dressing->HasTypedField<EJson::Array>(TEXT("taxiway_regions")))
	{
		BuildAabb(Dressing->GetArrayField(TEXT("taxiway_regions")), MarkingMesh, 2, TEXT("Road_Asphalt"), 1.5f);
	}
	if (Dressing->HasTypedField<EJson::Array>(TEXT("taxiway_markings")))
	{
		TArray<FVector> Verts, Normals;
		TArray<int32> Tris;
		TArray<FVector2D> UV;
		int32 MarkCount = 0;
		const int32 MarkCap = FMath::Clamp(TaxiwayMarkingCap, 32, 400);
		for (const TSharedPtr<FJsonValue>& Value : Dressing->GetArrayField(TEXT("taxiway_markings")))
		{
			const TSharedPtr<FJsonObject> M = Value->AsObject();
			if (!M.IsValid())
			{
				continue;
			}
			++MarkCount;
			if (MarkCount > MarkCap)
			{
				break;
			}
			const float Yaw = static_cast<float>(JsonD(M, {TEXT("yaw_deg")}, 0.0));
			const float Len = static_cast<float>(JsonD(M, {TEXT("length_m"), TEXT("depth_m")}, 4.0) * 100.0 * 0.5);
			const float Wid = static_cast<float>(JsonD(M, {TEXT("width_m")}, 0.18) * 100.0 * 0.5);
			const float Z = 4.f;
			const FVector Center = GeoToWorld(
				JsonD(M, {TEXT("lat")}, 0.0), JsonD(M, {TEXT("lon")}, 0.0), OriginLat, OriginLon, Z);
			AppendBox(Verts, Tris, Normals, UV, Center, FVector(Len, Wid, 2.f), Yaw);
		}
		CommitSection(MarkingMesh, 3, Verts, Tris, Normals, UV, TEXT("Road_Marking"));
		UE_LOG(LogTemp, Log, TEXT("[raceGPS] taxiway markings %d (cap %d)"), MarkCount, MarkCap);
	}
}

void AClevelandEnvironmentActor::BuildSkyline(const TSharedPtr<FJsonObject>& Skyline)
{
	if (!Skyline->HasTypedField<EJson::Array>(TEXT("buildings")))
	{
		return;
	}
	TArray<FVector> VertsC, NormalsC, VertsG, NormalsG;
	TArray<int32> TrisC, TrisG;
	TArray<FVector2D> UVC, UVG;
	LastSkylineBuildingCount = 0;
	LastNamedTowerCount = 0;

#if RACEGPS_HAS_BUILDING_GEN
	if (UWorld* World = GetWorld())
	{
		if (!NamedTowerGenerator)
		{
			NamedTowerGenerator = World->SpawnActor<ABuildingMeshGenerator>(ABuildingMeshGenerator::StaticClass());
		}
		if (NamedTowerGenerator)
		{
			NamedTowerGenerator->MaxDrawDistance = 700000.f;
#if RACEGPS_HAS_MATERIAL_PROVIDER
			NamedTowerGenerator->MaterialGlass = URaceGPSMaterialProvider::GetMasterMaterial(EMasterMaterialType::Building_Glass);
			NamedTowerGenerator->MaterialConcrete = URaceGPSMaterialProvider::GetMasterMaterial(EMasterMaterialType::Building_Concrete);
			NamedTowerGenerator->MaterialBrick = URaceGPSMaterialProvider::GetMasterMaterial(EMasterMaterialType::Building_Brick);
			NamedTowerGenerator->MaterialIndustrial = URaceGPSMaterialProvider::GetMasterMaterial(EMasterMaterialType::Building_Industrial);
			NamedTowerGenerator->MaterialResidential = URaceGPSMaterialProvider::GetMasterMaterial(EMasterMaterialType::Building_Residential);
#endif
		}
	}
#endif

	for (const TSharedPtr<FJsonValue>& Value : Skyline->GetArrayField(TEXT("buildings")))
	{
		const TSharedPtr<FJsonObject> B = Value->AsObject();
		if (!B.IsValid())
		{
			continue;
		}
		const FString Name = JsonS(B, TEXT("name"), FString());
		const double Lat = JsonD(B, {TEXT("lat")}, 0.0);
		const double Lon = JsonD(B, {TEXT("lon")}, 0.0);
		float HeightM = static_cast<float>(JsonD(B, {TEXT("height_m")}, 40.0));
		const FString MatIn = JsonS(B, TEXT("material"), FString());
		const bool bLandmark = !Name.IsEmpty() && (HeightM >= 80.f
			|| Name.Contains(TEXT("Tower")) || Name.Contains(TEXT("Center"))
			|| Name.Contains(TEXT("Terminal")) || Name.Contains(TEXT("Key")));
		if (bLandmark && HeightM >= 120.f)
		{
			HeightM *= 1.28f; // exaggerate Key / Terminal / Public Square for camera silhouette
		}
		else if (HeightM >= 90.f)
		{
			HeightM *= 1.16f;
		}
		const float Ht = HeightM * 100.0f;
		const float W = static_cast<float>(JsonD(B, {TEXT("width_m")}, 30.0) * 100.0 * 0.5);
		const float D = static_cast<float>(JsonD(B, {TEXT("depth_m")}, 30.0) * 100.0 * 0.5);
		const float Yaw = static_cast<float>(JsonD(B, {TEXT("yaw_deg")}, 0.0));
		const bool bGlass = MatIn.Contains(TEXT("Glass")) || HeightM >= 90.f
			|| Name.Contains(TEXT("Tower")) || Name.Contains(TEXT("Center"));
		const bool bBrick = Name.Contains(TEXT("Courthouse")) || Name.Contains(TEXT("Justice"));
		const FVector Center = GeoToWorld(Lat, Lon, OriginLat, OriginLon, Ht * 0.5f);
		const FVector Extent(W, D, Ht * 0.5f);
		++LastSkylineBuildingCount;

		bool bUsedGenerator = false;
#if RACEGPS_HAS_BUILDING_GEN
		if (bLandmark && NamedTowerGenerator)
		{
			FString Type = TEXT("concrete");
			if (bGlass)
			{
				Type = TEXT("office");
			}
			else if (bBrick)
			{
				Type = TEXT("education");
			}
			NamedTowerGenerator->AddWorldBoxBuilding(Name, Type, Center, FVector2D(W, D), Ht, Yaw);
			++LastNamedTowerCount;
			bUsedGenerator = true;
		}
#endif
		if (!bUsedGenerator)
		{
			if (bGlass)
			{
				AppendBox(VertsG, TrisG, NormalsG, UVG, Center, Extent, Yaw);
			}
			else
			{
				AppendBox(VertsC, TrisC, NormalsC, UVC, Center, Extent, Yaw);
			}
		}
	}
	CommitSection(SkylineMesh, 0, VertsC, TrisC, NormalsC, UVC, TEXT("Building_Concrete"));
	CommitSection(SkylineMesh, 1, VertsG, TrisG, NormalsG, UVG, TEXT("Building_Glass"));
	UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland env: skyline buildings=%d named_towers=%d (Karla additive silhouette south of Burke)"),
		LastSkylineBuildingCount, LastNamedTowerCount);
}

UMaterialInstanceDynamic* AClevelandEnvironmentActor::MakeLookMID(const TCHAR* SlotName, bool bMidnightRun, bool bGlass)
{
	const FString S(SlotName);

	// V12: midnight uses local night masters first. Provider M_Master_* assets are
	// typically missing; GetMasterMaterial then returns the engine default surface,
	// which ate Karla (dead black) and skipped M_NightAsphalt on the runway.
	if (bMidnightRun)
	{
		UMaterialInterface* NightBase = nullptr;
		if (S.Equals(TEXT("Road_Asphalt"), ESearchCase::IgnoreCase))
		{
			NightBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_NightAsphalt.M_NightAsphalt"));
		}
		else if (S.Equals(TEXT("Water_Surface"), ESearchCase::IgnoreCase))
		{
			NightBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Water_Blue.M_Water_Blue"));
		}
		else if (bGlass || S.Contains(TEXT("Glass")) || S.Contains(TEXT("Window"))
			|| S.Contains(TEXT("Concrete")) || S.Contains(TEXT("Building")))
		{
			NightBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_NightWindow.M_NightWindow"));
		}
		if (NightBase)
		{
			if (UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(NightBase, this))
			{
				LookMIDs.Add(Mid);
				const bool bWaterSlot = S.Equals(TEXT("Water_Surface"), ESearchCase::IgnoreCase);
				const bool bGlassSlot = bGlass || S.Contains(TEXT("Glass")) || S.Contains(TEXT("Window"));
				ApplyCommonMIDParams(Mid, bWaterSlot, true, bGlassSlot);
				return Mid;
			}
		}
	}

#if RACEGPS_HAS_MATERIAL_PROVIDER
	EMasterMaterialType Type = EMasterMaterialType::Default_Fallback;
	if (S.Equals(TEXT("Water_Surface"), ESearchCase::IgnoreCase)) Type = EMasterMaterialType::Water_Surface;
	else if (S.Equals(TEXT("Road_Asphalt"), ESearchCase::IgnoreCase)) Type = EMasterMaterialType::Road_Asphalt;
	else if (bGlass || S.Contains(TEXT("Glass"))) Type = EMasterMaterialType::Building_Glass;
	else if (S.Contains(TEXT("Concrete"))) Type = EMasterMaterialType::Building_Concrete;
	if (UMaterialInstanceDynamic* Mid = URaceGPSMaterialProvider::CreateMaterialInstance(Type, this))
	{
		LookMIDs.Add(Mid);
		ApplyCommonMIDParams(Mid, Type == EMasterMaterialType::Water_Surface, bMidnightRun, bGlass);
		return Mid;
	}
#endif
	// V11 fallback: local night masters so Karla / ground still light when provider path fails.
	UMaterialInterface* Base = nullptr;
	if (S.Equals(TEXT("Road_Asphalt"), ESearchCase::IgnoreCase))
	{
		Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_NightAsphalt.M_NightAsphalt"));
	}
	else if (bGlass || S.Contains(TEXT("Glass")))
	{
		Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_NightWindow.M_NightWindow"));
		if (!Base)
		{
			Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Building_glass.M_Building_glass"));
		}
	}
	else if (S.Contains(TEXT("Concrete")) || S.Contains(TEXT("Building")))
	{
		Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Building_concrete.M_Building_concrete"));
	}
	else if (S.Equals(TEXT("Water_Surface"), ESearchCase::IgnoreCase))
	{
		Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Water_Blue.M_Water_Blue"));
	}
	if (!Base)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, this);
	if (!Mid)
	{
		return nullptr;
	}
	LookMIDs.Add(Mid);
	ApplyCommonMIDParams(Mid, S.Equals(TEXT("Water_Surface"), ESearchCase::IgnoreCase), bMidnightRun, bGlass);
	return Mid;
}

void AClevelandEnvironmentActor::ApplyCommonMIDParams(UMaterialInstanceDynamic* Mid, bool bWater, bool bMidnightRun, bool bGlass) const
{
	if (!Mid)
	{
		return;
	}
	if (bWater)
	{
		const FLinearColor Tint = bMidnightRun
			? FLinearColor(0.012f, 0.035f, 0.070f)
			: FLinearColor(0.030f, 0.110f, 0.165f);
		Mid->SetVectorParameterValue(TEXT("BaseColor"), Tint);
		Mid->SetVectorParameterValue(TEXT("Color"), Tint);
		Mid->SetVectorParameterValue(TEXT("Tint"), Tint);
		Mid->SetVectorParameterValue(TEXT("WaterColor"), Tint);
		Mid->SetScalarParameterValue(TEXT("Opacity"), bMidnightRun ? 0.84f : 0.70f);
		Mid->SetScalarParameterValue(TEXT("Roughness"), 0.12f);
		Mid->SetScalarParameterValue(TEXT("Metallic"), 0.08f);
		Mid->SetScalarParameterValue(TEXT("Specular"), 0.85f);
		return;
	}
	const FLinearColor Base = bGlass
		? FLinearColor(0.10f, 0.12f, 0.16f)
		: FLinearColor(0.16f, 0.15f, 0.14f); // V11: lift so Karla isn't dead black
	Mid->SetVectorParameterValue(TEXT("BaseColor"), Base);
	Mid->SetScalarParameterValue(TEXT("Roughness"), bGlass ? 0.18f : 0.72f);
	Mid->SetScalarParameterValue(TEXT("Metallic"), bGlass ? 0.35f : 0.02f);
	// V13 City Sample pins: tint separate from strength (no double-multiply sheets).
	const float EmissiveStr = bMidnightRun ? (bGlass ? 2.60f : 1.35f) : 0.f;
	const FLinearColor Window = bGlass
		? FLinearColor(1.0f, 0.86f, 0.52f)
		: FLinearColor(1.0f, 0.78f, 0.42f);
	Mid->SetVectorParameterValue(TEXT("EmissiveColor"), Window);
	Mid->SetVectorParameterValue(TEXT("Emissive"), Window);
	Mid->SetVectorParameterValue(TEXT("EmissiveColor2"), Window);
	Mid->SetVectorParameterValue(TEXT("InteriorTint"), Window);
	Mid->SetScalarParameterValue(TEXT("EmissiveStrength"), EmissiveStr);
	Mid->SetScalarParameterValue(TEXT("EmissiveIntensity"), EmissiveStr);
	Mid->SetScalarParameterValue(TEXT("EmissiveMultiplier"), EmissiveStr);
	Mid->SetScalarParameterValue(TEXT("WindowLight"), EmissiveStr);
	Mid->SetScalarParameterValue(TEXT("WindowTile"), bGlass ? 12.0f : 8.0f);
	Mid->SetScalarParameterValue(TEXT("AmountOff"), bGlass ? 0.28f : 0.45f);
	Mid->SetScalarParameterValue(TEXT("InteriorExposure"), bGlass ? 1.30f : 1.00f);
	Mid->SetScalarParameterValue(TEXT("InteriorDepth"), bGlass ? 0.70f : 0.50f);
	Mid->SetScalarParameterValue(TEXT("LumaVariation"), 0.34f);
}


void AClevelandEnvironmentActor::ApplyLookMode(bool bMidnightRun)
{
	bLastMidnightLook = bMidnightRun;
	LookMIDs.Reset();
	if (UMaterialInstanceDynamic* LakeMid = MakeLookMID(TEXT("Water_Surface"), bMidnightRun, false))
	{
		if (LakeMesh)
		{
			LakeMesh->SetMaterial(0, LakeMid);
		}
	}
	if (UMaterialInstanceDynamic* ConcreteMid = MakeLookMID(TEXT("Building_Concrete"), bMidnightRun, false))
	{
		if (SkylineMesh)
		{
			SkylineMesh->SetMaterial(0, ConcreteMid);
		}
	}
	if (UMaterialInstanceDynamic* GlassMid = MakeLookMID(TEXT("Building_Glass"), bMidnightRun, true))
	{
		if (SkylineMesh)
		{
			SkylineMesh->SetMaterial(1, GlassMid);
		}
	}
	if (bMidnightRun)
	{
		if (UMaterialInstanceDynamic* HangarMid = MakeLookMID(TEXT("Building_Concrete"), true, false))
		{
			if (HangarMesh)
			{
				HangarMesh->SetMaterial(0, HangarMid);
			}
			if (BarrierMesh)
			{
				BarrierMesh->SetMaterial(0, HangarMid);
			}
		}
		ApplyNightGroundMaterials();
	}
#if RACEGPS_HAS_BUILDING_GEN
	if (NamedTowerGenerator)
	{
		if (UProceduralMeshComponent* Mesh = NamedTowerGenerator->GetBuildingMesh())
		{
			const int32 Num = Mesh->GetNumSections();
			for (int32 i = 0; i < Num; ++i)
			{
				// V13: force M_NightWindow path on every named-tower section (denser/brighter).
				const bool bGlassSlot = true;
				if (UMaterialInstanceDynamic* Mid = MakeLookMID(
					TEXT("Building_Glass"),
					bMidnightRun, bGlassSlot))
				{
					if (bMidnightRun)
					{
						Mid->SetScalarParameterValue(TEXT("WindowTile"), 14.0f);
						Mid->SetScalarParameterValue(TEXT("AmountOff"), 0.20f);
						Mid->SetScalarParameterValue(TEXT("EmissiveStrength"), 3.10f);
						Mid->SetScalarParameterValue(TEXT("InteriorExposure"), 1.45f);
						Mid->SetScalarParameterValue(TEXT("InteriorDepth"), 0.80f);
						Mid->SetVectorParameterValue(TEXT("InteriorTint"), FLinearColor(1.0f, 0.84f, 0.48f));
						Mid->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(1.0f, 0.90f, 0.55f));
						Mid->SetScalarParameterValue(TEXT("WindowSeed"), float(i) * 0.17f);
					}
					Mesh->SetMaterial(i, Mid);
				}
			}
		}
	}
#endif
	// V11: skyline PMC always gets a night glass pass so additive silhouette isn't dead black.
	if (bMidnightRun && SkylineMesh)
	{
		if (UMaterialInstanceDynamic* GlassMid = MakeLookMID(TEXT("Building_Glass"), true, true))
		{
			GlassMid->SetScalarParameterValue(TEXT("WindowTile"), 13.0f);
			GlassMid->SetScalarParameterValue(TEXT("AmountOff"), 0.24f);
			GlassMid->SetScalarParameterValue(TEXT("EmissiveStrength"), 2.85f);
			GlassMid->SetScalarParameterValue(TEXT("InteriorExposure"), 1.35f);
			GlassMid->SetScalarParameterValue(TEXT("InteriorDepth"), 0.72f);
			SkylineMesh->SetMaterial(1, GlassMid);
			if (SkylineMesh->GetNumSections() > 0)
			{
				SkylineMesh->SetMaterial(0, GlassMid);
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland env: ApplyLookMode midnight=%d skyline_buildings=%d named_towers=%d mids=%d"),
		bMidnightRun ? 1 : 0, LastSkylineBuildingCount, LastNamedTowerCount, LookMIDs.Num());
}

void AClevelandEnvironmentActor::ApplyNightGroundMaterials()
{
	// Dark wet asphalt on runway/taxiway sections; keep Road_Marking readable.
	int32 Applied = 0;
	auto ApplyAsphalt = [&](UProceduralMeshComponent* Mesh, int32 Section)
	{
		if (!Mesh)
		{
			return;
		}
		UMaterialInstanceDynamic* Mid = MakeLookMID(TEXT("Road_Asphalt"), true, false);
		if (!Mid)
		{
			UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr,
				TEXT("/Game/Materials/M_NightAsphalt.M_NightAsphalt"));
			if (Base)
			{
				Mid = UMaterialInstanceDynamic::Create(Base, this);
				if (Mid) { LookMIDs.Add(Mid); }
			}
		}
		if (!Mid)
		{
			return;
		}
		Mid->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.130f, 0.118f, 0.100f));
		Mid->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.130f, 0.118f, 0.100f));
		Mid->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.130f, 0.118f, 0.100f));
		Mid->SetScalarParameterValue(TEXT("Roughness"), 0.22f);
		Mid->SetScalarParameterValue(TEXT("Specular"), 0.78f);
		Mid->SetScalarParameterValue(TEXT("Metallic"), 0.06f);
		Mid->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor::Black);
		Mesh->SetMaterial(Section, Mid);
		++Applied;
	};
	// MarkingMesh sections from BuildRunwayTaxiwayDecals / BuildStartFinish:
	// 0 = Road_Marking, 1 = runway Road_Asphalt, 2 = taxiway Road_Asphalt, 3 = markings
	if (MarkingMesh)
	{
		ApplyAsphalt(MarkingMesh, 1);
		ApplyAsphalt(MarkingMesh, 2);
		// Markings: slightly brighter so they still read on wet asphalt.
		if (UMaterialInstanceDynamic* Mark = MakeLookMID(TEXT("Road_Asphalt"), true, false))
		{
			Mark->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.75f, 0.75f, 0.72f));
			Mark->SetScalarParameterValue(TEXT("Roughness"), 0.45f);
			Mark->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(0.08f, 0.08f, 0.07f));
			MarkingMesh->SetMaterial(0, Mark);
			MarkingMesh->SetMaterial(3, Mark);
			++Applied;
		}
	}
	if (BarrierMesh)
	{
		ApplyAsphalt(BarrierMesh, 1); // tire/asphalt section from BuildBarriers
	}
	if (GrassMesh)
	{
		if (UMaterialInstanceDynamic* Grass = MakeLookMID(TEXT("Building_Concrete"), true, false))
		{
			Grass->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.02f, 0.035f, 0.025f));
			Grass->SetScalarParameterValue(TEXT("Roughness"), 0.9f);
			Grass->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor::Black);
			GrassMesh->SetMaterial(0, Grass);
			++Applied;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland env: V15 night ground materials applied=%d"), Applied);
}
