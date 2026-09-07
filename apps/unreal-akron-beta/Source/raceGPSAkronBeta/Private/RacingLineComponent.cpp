#include "RacingLineComponent.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include <initializer_list>

namespace
{
	float JsonFloat(const TSharedPtr<FJsonObject>& Obj, std::initializer_list<const TCHAR*> Keys, float DefaultValue)
	{
		for (const TCHAR* Key : Keys)
		{
			if (Obj->HasTypedField<EJson::Number>(Key))
			{
				return static_cast<float>(Obj->GetNumberField(Key));
			}
		}
		return DefaultValue;
	}

	double JsonDouble(const TSharedPtr<FJsonObject>& Obj, std::initializer_list<const TCHAR*> Keys, double DefaultValue)
	{
		for (const TCHAR* Key : Keys)
		{
			if (Obj->HasTypedField<EJson::Number>(Key))
			{
				return Obj->GetNumberField(Key);
			}
		}
		return DefaultValue;
	}

	const TArray<TSharedPtr<FJsonValue>>* FindArray(const TSharedPtr<FJsonObject>& Root)
	{
		static const TCHAR* Names[] = {
			TEXT("samples"), TEXT("points"), TEXT("racing_line"), TEXT("line"), TEXT("waypoints")
		};
		for (const TCHAR* Name : Names)
		{
			if (Root->HasTypedField<EJson::Array>(Name))
			{
				return &Root->GetArrayField(Name);
			}
		}
		return nullptr;
	}

	bool ResolveExistingPath(const FString& InPath, FString& OutPath)
	{
		if (FPaths::FileExists(InPath))
		{
			OutPath = InPath;
			return true;
		}

		const FString Content = FPaths::Combine(FPaths::ProjectContentDir(), InPath);
		if (FPaths::FileExists(Content))
		{
			OutPath = Content;
			return true;
		}

		const FString Project = FPaths::Combine(FPaths::ProjectDir(), InPath);
		if (FPaths::FileExists(Project))
		{
			OutPath = Project;
			return true;
		}

		const FString ContentDir = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Dir"), InPath);
		if (FPaths::FileExists(ContentDir))
		{
			OutPath = ContentDir;
			return true;
		}

		return false;
	}
}

URacingLineComponent::URacingLineComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FVector URacingLineComponent::GeoToWorld(double LatDeg, double LonDeg, double RefLatDeg, double RefLonDeg, float HeightCm)
{
	constexpr double MetersPerDegLat = 111320.0;
	const double RefLatRad = FMath::DegreesToRadians(RefLatDeg);
	const double MetersPerDegLon = MetersPerDegLat * FMath::Cos(RefLatRad);
	const double EastM = (LonDeg - RefLonDeg) * MetersPerDegLon;
	const double NorthM = (LatDeg - RefLatDeg) * MetersPerDegLat;
	// 1uu = 1cm on this branch. Z-up. X=east, Y=north.
	return FVector(static_cast<float>(EastM * 100.0), static_cast<float>(NorthM * 100.0), HeightCm);
}

bool URacingLineComponent::LoadDefaultClevelandLine()
{
	return LoadFromJsonFile(JsonRelativePath);
}

bool URacingLineComponent::LoadFromJsonFile(const FString& AbsoluteOrRelativePath)
{
	Samples.Reset();
	TrackLength = 0.f;

	FString Path;
	if (!ResolveExistingPath(AbsoluteOrRelativePath, Path))
	{
		UE_LOG(LogTemp, Warning, TEXT("raceGPS Cleveland: racing_line.json not found: %s"), *AbsoluteOrRelativePath);
		return false;
	}

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *Path))
	{
		UE_LOG(LogTemp, Warning, TEXT("raceGPS Cleveland: failed to read %s"), *Path);
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("raceGPS Cleveland: invalid JSON %s"), *Path);
		return false;
	}

	if (Root->HasTypedField<EJson::Boolean>(TEXT("closed")))
	{
		bClosedLoop = Root->GetBoolField(TEXT("closed"));
	}

	const TSharedPtr<FJsonObject>* OriginObj = nullptr;
	if (Root->TryGetObjectField(TEXT("origin"), OriginObj) && OriginObj && (*OriginObj).IsValid())
	{
		OriginLat = JsonDouble(*OriginObj, {TEXT("lat"), TEXT("latitude")}, 0.0);
		OriginLon = JsonDouble(*OriginObj, {TEXT("lon"), TEXT("lng"), TEXT("longitude")}, 0.0);
	}

	const TArray<TSharedPtr<FJsonValue>>* Arr = FindArray(Root);
	if (!Arr || Arr->Num() < 2)
	{
		UE_LOG(LogTemp, Warning, TEXT("raceGPS Cleveland: racing line needs >= 2 samples"));
		return false;
	}

	Samples.Reserve(Arr->Num());
	for (const TSharedPtr<FJsonValue>& Value : *Arr)
	{
		const TSharedPtr<FJsonObject> Obj = Value->AsObject();
		if (!Obj.IsValid())
		{
			continue;
		}
		FRacingLineSample Sample;
		Sample.Lat = JsonDouble(Obj, {TEXT("lat"), TEXT("latitude")}, 0.0);
		Sample.Lon = JsonDouble(Obj, {TEXT("lon"), TEXT("lng"), TEXT("longitude")}, 0.0);
		Sample.S = JsonFloat(Obj, {TEXT("s"), TEXT("s_cm"), TEXT("s_m"), TEXT("distance")}, 0.f);
		Sample.Curvature = JsonFloat(Obj, {TEXT("curvature"), TEXT("k"), TEXT("kappa")}, 0.f);
		if (Obj->HasTypedField<EJson::Number>(TEXT("s_m")) && !Obj->HasTypedField<EJson::Number>(TEXT("s_cm"))
			&& !Obj->HasTypedField<EJson::Number>(TEXT("s")))
		{
			Sample.S *= 100.f;
		}
		Samples.Add(Sample);
	}

	if (Samples.Num() < 2)
	{
		return false;
	}

	if (OriginLat == 0.0 && OriginLon == 0.0)
	{
		OriginLat = Samples[0].Lat;
		OriginLon = Samples[0].Lon;
	}

	float MaxS = 0.f;
	float MaxAbsK = 0.f;
	for (const FRacingLineSample& S : Samples)
	{
		MaxS = FMath::Max(MaxS, S.S);
		MaxAbsK = FMath::Max(MaxAbsK, FMath::Abs(S.Curvature));
	}
	// Heuristic: s stored in meters for a ~2–4 km street circuit.
	if (MaxS > KINDA_SMALL_NUMBER && MaxS < 50000.f)
	{
		for (FRacingLineSample& S : Samples)
		{
			S.S *= 100.f;
		}
	}
	// Curvature in 1/cm would be huge; convert to 1/m.
	if (MaxAbsK > 2.f)
	{
		for (FRacingLineSample& S : Samples)
		{
			S.Curvature *= 100.f;
		}
	}

	RebuildWorldFromGeo();
	EnsureArcLength();
	UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland: loaded %d samples, track=%.1f cm from %s"),
		Samples.Num(), TrackLength, *Path);
	return IsValidLine();
}

void URacingLineComponent::RebuildWorldFromGeo()
{
	for (FRacingLineSample& S : Samples)
	{
		S.WorldPos = GeoToWorld(S.Lat, S.Lon, OriginLat, OriginLon, 0.f);
	}
}

void URacingLineComponent::EnsureArcLength()
{
	if (Samples.Num() < 2)
	{
		TrackLength = 0.f;
		return;
	}

	bool bNeedS = Samples[Samples.Num() - 1].S <= Samples[0].S + KINDA_SMALL_NUMBER;
	if (bNeedS)
	{
		Samples[0].S = 0.f;
		for (int32 i = 1; i < Samples.Num(); ++i)
		{
			Samples[i].S = Samples[i - 1].S + FVector::Dist(Samples[i - 1].WorldPos, Samples[i].WorldPos);
		}
	}

	TrackLength = Samples.Last().S;
	if (bClosedLoop)
	{
		const float Close = FVector::Dist(Samples.Last().WorldPos, Samples[0].WorldPos);
		TrackLength = Samples.Last().S + Close;
	}
}

void URacingLineComponent::FindSegment(float SWrapped, int32& OutI0, int32& OutI1, float& OutAlpha) const
{
	OutI0 = 0;
	OutI1 = 1;
	OutAlpha = 0.f;
	if (Samples.Num() < 2)
	{
		return;
	}

	auto SegEndS = [this](int32 Index) -> float
	{
		if (Index < Samples.Num() - 1)
		{
			return Samples[Index + 1].S;
		}
		return TrackLength;
	};

	for (int32 i = 0; i < Samples.Num(); ++i)
	{
		const float S0 = Samples[i].S;
		const float S1 = SegEndS(i);
		if (SWrapped + KINDA_SMALL_NUMBER >= S0 && SWrapped <= S1 + KINDA_SMALL_NUMBER)
		{
			OutI0 = i;
			OutI1 = (i + 1 < Samples.Num()) ? (i + 1) : 0;
			const float Den = FMath::Max(S1 - S0, KINDA_SMALL_NUMBER);
			OutAlpha = FMath::Clamp((SWrapped - S0) / Den, 0.f, 1.f);
			return;
		}
	}

	OutI0 = Samples.Num() - 1;
	OutI1 = bClosedLoop ? 0 : OutI0;
	OutAlpha = 1.f;
}

FVector URacingLineComponent::GetPointAtS(float S) const
{
	if (Samples.Num() == 0)
	{
		return FVector::ZeroVector;
	}
	if (Samples.Num() == 1)
	{
		return Samples[0].WorldPos;
	}
	int32 I0, I1;
	float A;
	FindSegment(RaceAIControlMath::WrapS(S, TrackLength), I0, I1, A);
	return FMath::Lerp(Samples[I0].WorldPos, Samples[I1].WorldPos, A);
}

FVector URacingLineComponent::GetTangentAtS(float S) const
{
	if (Samples.Num() < 2 || TrackLength <= KINDA_SMALL_NUMBER)
	{
		return FVector::ForwardVector;
	}
	int32 I0, I1;
	float A;
	FindSegment(RaceAIControlMath::WrapS(S, TrackLength), I0, I1, A);
	FVector T = Samples[I1].WorldPos - Samples[I0].WorldPos;
	T.Z = 0.f;
	if (T.IsNearlyZero())
	{
		return FVector::ForwardVector;
	}
	return T.GetSafeNormal();
}

float URacingLineComponent::GetCurvatureAtS(float S) const
{
	if (Samples.Num() == 0)
	{
		return 0.f;
	}
	if (Samples.Num() == 1)
	{
		return Samples[0].Curvature;
	}
	int32 I0, I1;
	float A;
	FindSegment(RaceAIControlMath::WrapS(S, TrackLength), I0, I1, A);
	return FMath::Lerp(Samples[I0].Curvature, Samples[I1].Curvature, A);
}

float URacingLineComponent::GetNearestS(const FVector& WorldLocation) const
{
	float Dummy = 0.f;
	return GetNearestSWithCte(WorldLocation, Dummy);
}

float URacingLineComponent::GetNearestSWithCte(const FVector& WorldLocation, float& OutSignedCteCm) const
{
	OutSignedCteCm = 0.f;
	if (Samples.Num() < 2)
	{
		return 0.f;
	}

	float BestDistSq = TNumericLimits<float>::Max();
	float BestS = 0.f;
	float BestCte = 0.f;

	const int32 SegCount = bClosedLoop ? Samples.Num() : (Samples.Num() - 1);
	for (int32 i = 0; i < SegCount; ++i)
	{
		const int32 J = (i + 1 < Samples.Num()) ? (i + 1) : 0;
		const FVector A = Samples[i].WorldPos;
		const FVector B = Samples[J].WorldPos;
		FVector AB = B - A;
		AB.Z = 0.f;
		const float ABLenSq = AB.SizeSquared();
		FVector AP = WorldLocation - A;
		AP.Z = 0.f;
		float T = 0.f;
		if (ABLenSq > KINDA_SMALL_NUMBER)
		{
			T = FMath::Clamp(FVector::DotProduct(AP, AB) / ABLenSq, 0.f, 1.f);
		}
		const FVector Closest = A + AB * T;
		FVector Delta = WorldLocation - Closest;
		Delta.Z = 0.f;
		const float DistSq = Delta.SizeSquared();
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			const float S0 = Samples[i].S;
			const float S1 = (J == 0) ? TrackLength : Samples[J].S;
			BestS = FMath::Lerp(S0, S1, T);
			if (BestS >= TrackLength)
			{
				BestS = 0.f;
			}
			const FVector Tangent = AB.GetSafeNormal();
			BestCte = Tangent.X * Delta.Y - Tangent.Y * Delta.X; // left positive
		}
	}

	OutSignedCteCm = BestCte;
	return BestS;
}

FTransform URacingLineComponent::GetPoseAtS(float S, float LateralOffsetCm) const
{
	const FVector Tangent = GetTangentAtS(S);
	const FVector Normal(-Tangent.Y, Tangent.X, 0.f); // left
	const FVector Pos = GetPointAtS(S) + Normal * LateralOffsetCm;
	const FRotator Rot = Tangent.Rotation();
	return FTransform(Rot, Pos);
}
