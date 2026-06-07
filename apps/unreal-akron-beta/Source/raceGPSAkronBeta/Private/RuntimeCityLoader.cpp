// Copyright raceGPS. All Rights Reserved.

#include "RuntimeCityLoader.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "JsonObjectConverter.h"
#include "Components/SplineComponent.h"
#include "Components/SphereComponent.h"

ARuntimeCityLoader::ARuntimeCityLoader()
{
    PrimaryActorTick.bCanEverTick = false;
}

void ARuntimeCityLoader::BeginPlay()
{
    Super::BeginPlay();
    if (bLoadOnBeginPlay && !CitypackPath.IsEmpty())
    {
        LoadCitypack(CitypackPath);
    }
}

void ARuntimeCityLoader::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UnloadCitypack();
    Super::EndPlay(EndPlayReason);
}

bool ARuntimeCityLoader::LoadCitypack(const FString& Path)
{
    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *Path))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load citypack: %s"), *Path);
        return false;
    }

    TSharedPtr<FJsonObject> RootObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, RootObject))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to parse citypack JSON"));
        return false;
    }

    LoadedCityId = RootObject->GetStringField(TEXT("city_id"));
    UE_LOG(LogTemp, Log, TEXT("Loading citypack: %s"), *LoadedCityId);

    // Load routes
    const TArray<TSharedPtr<FJsonValue>>* RoutesArray;
    if (RootObject->TryGetArrayField(TEXT("routes"), RoutesArray))
    {
        for (const auto& RouteVal : *RoutesArray)
        {
            TSharedPtr<FJsonObject> RouteObj = RouteVal->AsObject();
            if (!RouteObj.IsValid()) continue;

            FString RouteId = RouteObj->GetStringField(TEXT("route_id"));
            const TArray<TSharedPtr<FJsonValue>>* PointsArray;
            if (RouteObj->TryGetArrayField(TEXT("spline_points"), PointsArray))
            {
                TArray<FVector> Points;
                for (const auto& Pt : *PointsArray)
                {
                    TSharedPtr<FJsonObject> PtObj = Pt->AsObject();
                    if (PtObj.IsValid())
                    {
                        Points.Add(FVector(
                            PtObj->GetNumberField(TEXT("x")),
                            PtObj->GetNumberField(TEXT("y")),
                            PtObj->GetNumberField(TEXT("z"))
                        ));
                    }
                }
                SpawnRoadSpline(Points, RouteId);
            }
        }
    }

    // Load spawn points
    const TArray<TSharedPtr<FJsonValue>>* SpawnsArray;
    if (RootObject->TryGetArrayField(TEXT("spawn_points"), SpawnsArray))
    {
        for (const auto& Sp : *SpawnsArray)
        {
            TSharedPtr<FJsonObject> SpObj = Sp->AsObject();
            if (!SpObj.IsValid()) continue;
            TSharedPtr<FJsonObject> LocObj = SpObj->GetObjectField(TEXT("location"));
            if (LocObj.IsValid())
            {
                FVector Loc(LocObj->GetNumberField(TEXT("x")), LocObj->GetNumberField(TEXT("y")), LocObj->GetNumberField(TEXT("z")));
                SpawnCheckpoint(Loc, 50.0f, SpObj->GetStringField(TEXT("id")));
            }
        }
    }

    bLoaded = true;
    UE_LOG(LogTemp, Log, TEXT("Citypack loaded: %s (spawned %d actors)"), *LoadedCityId, SpawnedActors.Num());
    return true;
}

bool ARuntimeCityLoader::UnloadCitypack()
{
    for (AActor* Actor : SpawnedActors)
    {
        if (IsValid(Actor))
        {
            Actor->Destroy();
        }
    }
    SpawnedActors.Empty();
    bLoaded = false;
    LoadedCityId.Empty();
    return true;
}

void ARuntimeCityLoader::StreamChunk(const FVector& PlayerLocation, float RadiusMeters)
{
    // Distance-based culling for spawned actors
    for (AActor* Actor : SpawnedActors)
    {
        if (!IsValid(Actor)) continue;
        float Dist = FVector::Dist(PlayerLocation, Actor->GetActorLocation());
        Actor->SetActorHiddenInGame(Dist > RadiusMeters);
    }
}

void ARuntimeCityLoader::SpawnRoadSpline(const TArray<FVector>& Points, const FString& RouteId)
{
    if (Points.Num() < 2) return;

    FActorSpawnParameters Params;
    Params.Name = FName(*FString::Printf(TEXT("Route_%s"), *RouteId));
    AActor* SplineActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), Points[0], FRotator::ZeroRotator, Params);
    if (!SplineActor) return;

    USplineComponent* Spline = NewObject<USplineComponent>(SplineActor);
    Spline->RegisterComponent();
    SplineActor->AddInstanceComponent(Spline);
    Spline->ClearSplinePoints();

    for (const FVector& Pt : Points)
    {
        Spline->AddSplinePoint(Pt, ESplineCoordinateSpace::World);
    }

    SplineActor->SetActorLabel(FString::Printf(TEXT("Route_%s"), *RouteId));
    SpawnedActors.Add(SplineActor);
}

void ARuntimeCityLoader::SpawnCheckpoint(const FVector& Location, float Radius, const FString& Id)
{
    FActorSpawnParameters Params;
    Params.Name = FName(*FString::Printf(TEXT("Checkpoint_%s"), *Id));
    AActor* Actor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), Location, FRotator::ZeroRotator, Params);
    if (!Actor) return;

    USphereComponent* Sphere = NewObject<USphereComponent>(Actor);
    Sphere->RegisterComponent();
    Sphere->SetSphereRadius(Radius);
    Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Actor->AddInstanceComponent(Sphere);
    Actor->SetActorLabel(FString::Printf(TEXT("Checkpoint_%s"), *Id));
    SpawnedActors.Add(Actor);
}

void ARuntimeCityLoader::SpawnPOI(const FVector& Location, const FString& Type, const FString& Name)
{
    FActorSpawnParameters Params;
    AActor* Actor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), Location, FRotator::ZeroRotator, Params);
    if (!Actor) return;
    Actor->Tags.Add(FName(*Type));
    Actor->SetActorLabel(FString::Printf(TEXT("POI_%s_%s"), *Type, *Name));
    SpawnedActors.Add(Actor);
}

void ARuntimeCityLoader::SpawnWaterBody(const TArray<FVector>& Points, const FString& Type)
{
    // Placeholder: spawn empty actor tagged as water
    if (Points.Num() == 0) return;
    FVector Center = Points[Points.Num() / 2];
    FActorSpawnParameters Params;
    AActor* Actor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), Center, FRotator::ZeroRotator, Params);
    if (!Actor) return;
    Actor->Tags.Add(FName(TEXT("water")));
    Actor->Tags.Add(FName(*Type));
    Actor->SetActorLabel(FString::Printf(TEXT("Water_%s"), *Type));
    SpawnedActors.Add(Actor);
}

void ARuntimeCityLoader::SpawnVegetationZone(const FVector& Center, float Radius, const FString& Type, float Density)
{
    FActorSpawnParameters Params;
    AActor* Actor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), Center, FRotator::ZeroRotator, Params);
    if (!Actor) return;
    Actor->Tags.Add(FName(TEXT("vegetation")));
    Actor->Tags.Add(FName(*Type));
    Actor->SetActorLabel(FString::Printf(TEXT("Veg_%s"), *Type));
    SpawnedActors.Add(Actor);
}

void ARuntimeCityLoader::OnStreamTick()
{
    APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
    if (PlayerPawn)
    {
        StreamChunk(PlayerPawn->GetActorLocation(), 5000.0f);
    }
}
