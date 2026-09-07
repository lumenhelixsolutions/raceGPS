#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ClevelandEnvironmentActor.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInstanceDynamic;
class ABuildingMeshGenerator;

/**
 * Camera-needed Cleveland Historic Circuit dressing (M5).
 * Loads citypack JSON (environment / water / skyline / track_dressing) and
 * builds procedural meshes. V14 Cesium OSM Buildings skyline spike; no CARLA interiors.
 *
 * Search paths match URacingLineComponent:
 *   ProjectDir()/citypacks/cleveland/burke_gp_1997/<file>
 *   ProjectContentDir() variants (see ResolveCityPackFile).
 *
 * Geo: Z-up, 1uu=1cm, X=east, Y=north (URacingLineComponent::GeoToWorld).
 *
 * Additive near-track silhouette + Lake Erie sheet. The T10 baked HISM city
 * (~120k buildings, ~7k water) already lives in Cleveland5_0KmWorld.
 */
UCLASS()
class RACEGPSAKRONBETA_API AClevelandEnvironmentActor : public AActor
{
	GENERATED_BODY()

public:
	AClevelandEnvironmentActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|Env")
	FString CityPackRelativeDir = TEXT("citypacks/cleveland/burke_gp_1997");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|Env")
	double OriginLat = 41.51722;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|Env")
	double OriginLon = -81.68306;

	/** Burke field ~583 ft AMSL. Cesium OriginHeight is meters above WGS84 ellipsoid. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|Env")
	double OriginHeightMeters = 177.0;

	/** Cesium ion OSM Buildings (96188). Google Photorealistic 2275207 needs user ion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|Env")
	int64 CesiumOsmBuildingsAssetId = 96188;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|Env")
	bool bEnableCesiumSkyline = false; // V15: ion Connect paused; do not stream tiles

	/** 14:00 day-demo lighting if ADayNightCycle is missing from the world. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|Env")
	float DayNightHours = 14.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Cleveland|Env")
	TObjectPtr<UProceduralMeshComponent> LakeMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Cleveland|Env")
	TObjectPtr<UProceduralMeshComponent> GrassMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Cleveland|Env")
	TObjectPtr<UProceduralMeshComponent> BarrierMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Cleveland|Env")
	TObjectPtr<UProceduralMeshComponent> ConeMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Cleveland|Env")
	TObjectPtr<UProceduralMeshComponent> MarkingMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Cleveland|Env")
	TObjectPtr<UProceduralMeshComponent> SkylineMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Cleveland|Env")
	TObjectPtr<UProceduralMeshComponent> HangarMesh;

	/** Offline CARLA hangar static mesh instances when the uasset exists; else unused. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Cleveland|Env")
	TObjectPtr<UInstancedStaticMeshComponent> HangarPropISM;

	/** Procedural hangar box cap (airport polish; complements baked HISM city). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|Env")
	int32 HangarBoxCap = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|Env")
	int32 TaxiwayMarkingCap = 240;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Cleveland|Env")
	int32 LastSkylineBuildingCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Cleveland|Env")
	int32 LastNamedTowerCount = 0;

	UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland|Env")
	bool LoadAndBuild();

	UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland|Env")
	void ApplyLookMode(bool bMidnightRun);

	/** V11: wet night asphalt on runway/taxiway MarkingMesh + grass. */
	UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland|Env")
	void ApplyNightGroundMaterials();

	UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland|Env")
	static FVector GeoToWorld(double LatDeg, double LonDeg, double RefLatDeg, double RefLonDeg, float HeightCm = 0.f);

protected:
	virtual void BeginPlay() override;

	FString ResolveCityPackFile(const FString& FileName) const;
	bool LoadJsonFile(const FString& FileName, TSharedPtr<class FJsonObject>& OutRoot) const;
	UMaterialInterface* ResolveMaterial(const TCHAR* SlotName) const;
	UMaterialInstanceDynamic* MakeLookMID(const TCHAR* SlotName, bool bMidnightRun, bool bGlass);
	void ApplyCommonMIDParams(UMaterialInstanceDynamic* Mid, bool bWater, bool bMidnightRun, bool bGlass) const;
	void EnsureDayNightCycle();
	void EnsureCesiumSpike();

	void BuildLake(const TSharedPtr<FJsonObject>& Water);
	void BuildGrass(const TSharedPtr<FJsonObject>& Dressing);
	void BuildBarriers(const TSharedPtr<FJsonObject>& Dressing);
	void BuildCones(const TSharedPtr<FJsonObject>& Dressing);
	void BuildStartFinish(const TSharedPtr<FJsonObject>& Dressing);
	void BuildSkyline(const TSharedPtr<FJsonObject>& Skyline);
	void BuildHangars(const TSharedPtr<FJsonObject>& Dressing);
	void BuildRunwayTaxiwayDecals(const TSharedPtr<FJsonObject>& Dressing);

	void AppendBox(TArray<FVector>& Verts, TArray<int32>& Tris, TArray<FVector>& Normals, TArray<FVector2D>& UV,
		const FVector& Center, const FVector& Extent, float YawDeg);
	void AppendCone(TArray<FVector>& Verts, TArray<int32>& Tris, TArray<FVector>& Normals, TArray<FVector2D>& UV,
		const FVector& Base, float RadiusCm, float HeightCm, int32 Sides);
	void AppendPolygonXZ(TArray<FVector>& Verts, TArray<int32>& Tris, TArray<FVector>& Normals, TArray<FVector2D>& UV,
		const TArray<FVector>& Ring, float ZCm);

	void CommitSection(UProceduralMeshComponent* Mesh, int32 Section, const TArray<FVector>& Verts,
		const TArray<int32>& Tris, const TArray<FVector>& Normals, const TArray<FVector2D>& UV,
		const TCHAR* MaterialSlot);

	UPROPERTY()
	TObjectPtr<ABuildingMeshGenerator> NamedTowerGenerator;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> LookMIDs;

	bool bLastMidnightLook = false;
};