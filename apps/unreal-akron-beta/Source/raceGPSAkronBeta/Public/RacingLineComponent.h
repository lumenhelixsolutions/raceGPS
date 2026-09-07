#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ClevelandShowcaseTypes.h"
#include "RacingLineComponent.generated.h"

/**
 * Closed-loop racing line loaded from racing_line.json (lat/lon + s + curvature).
 * World positions are UE centimeters, Z-up (1uu = 1cm) via GeoToWorld on this branch.
 */
UCLASS(ClassGroup = (raceGPS), meta = (BlueprintSpawnableComponent))
class RACEGPSAKRONBETA_API URacingLineComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URacingLineComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|Line")
	FString JsonRelativePath = TEXT("citypacks/cleveland/burke_gp_1997/racing_line.json");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|Line")
	bool bClosedLoop = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Cleveland|Line")
	float TrackLength = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Cleveland|Line")
	double OriginLat = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Cleveland|Line")
	double OriginLon = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Cleveland|Line")
	TArray<FRacingLineSample> Samples;

	UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland|Line")
	bool LoadFromJsonFile(const FString& AbsoluteOrRelativePath);

	UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland|Line")
	bool LoadDefaultClevelandLine();

	UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland|Line")
	FVector GetPointAtS(float S) const;

	UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland|Line")
	FVector GetTangentAtS(float S) const;

	UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland|Line")
	float GetCurvatureAtS(float S) const;

	UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland|Line")
	float GetNearestS(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland|Line")
	float GetNearestSWithCte(const FVector& WorldLocation, float& OutSignedCteCm) const;

	UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland|Line")
	FTransform GetPoseAtS(float S, float LateralOffsetCm = 0.f) const;

	UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland|Line")
	int32 NumSamples() const { return Samples.Num(); }

	UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland|Line")
	bool IsValidLine() const { return Samples.Num() >= 2 && TrackLength > KINDA_SMALL_NUMBER; }

	/** Equirectangular GeoToWorld, Z-up, 1uu=1cm. Matches this branch's GeoToWorld. */
	UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland|Line")
	static FVector GeoToWorld(double LatDeg, double LonDeg, double RefLatDeg, double RefLonDeg, float HeightCm = 0.f);

protected:
	void RebuildWorldFromGeo();
	void EnsureArcLength();
	void FindSegment(float SWrapped, int32& OutI0, int32& OutI1, float& OutAlpha) const;
};
