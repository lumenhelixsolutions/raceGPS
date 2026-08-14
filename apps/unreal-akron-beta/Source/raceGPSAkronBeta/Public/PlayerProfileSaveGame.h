// Copyright raceGPS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "PlayerProfileSaveGame.generated.h"

/**
 * Single saved car build entry with raw garageGPS JSON.
 */
USTRUCT(BlueprintType)
struct FCarBuildEntry
{
    GENERATED_BODY()

    /** Unique identifier for this build. */
    UPROPERTY()
    FString BuildId;

    /** Display name shown in garage UI. */
    UPROPERTY()
    FString DisplayName;

    /** Raw garageGPS JSON string representing the full build manifest. */
    UPROPERTY()
    FString GarageGPSJson;
};

/**
 * Persistent player profile using UE's SaveGame system.
 * Stores progression, stats, garage builds, lap records, and unlocks.
 */
UCLASS()
class RACEGPSAKRONBETA_API UPlayerProfileSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPlayerProfileSaveGame();

    /** Player display name. */
    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Profile")
    FString PlayerName;

    /** Current player level (increments every 1000 XP). */
    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Profile")
    int32 PlayerLevel;

    /** Total accumulated experience points. */
    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Profile")
    int32 TotalXP;

    /** Total distance driven across all sessions (meters). */
    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Profile")
    float TotalDistanceDriven;

    /** Number of races started. */
    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Profile")
    int32 TotalRaces;

    /** Number of races finished in 1st place. */
    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Profile")
    int32 TotalWins;

    /** Saved garage builds as raw JSON strings. */
    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Profile")
    TArray<FCarBuildEntry> SavedCarBuilds;

    /** Best lap times per track (TrackId -> TimeSeconds). */
    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Profile")
    TMap<FString, float> BestLapTimes;

    /** Unlocked part IDs. */
    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Profile")
    TArray<FString> UnlockedParts;

    /** Unlocked material IDs. */
    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Profile")
    TArray<FString> UnlockedMaterials;

    /** Unlocked decal IDs. */
    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Profile")
    TArray<FString> UnlockedDecals;

    /** Save the profile to the default slot. Returns true on success. */
    UFUNCTION(BlueprintCallable, Category = "raceGPS|Profile")
    bool SaveProfile();

    /**
     * Load the profile from the default slot.
     * Returns a valid profile (existing or freshly created default) on success,
     * or nullptr if the load/create operation critically failed.
     */
    UFUNCTION(BlueprintCallable, Category = "raceGPS|Profile")
    static UPlayerProfileSaveGame* LoadProfile();

    /** Add XP and auto-level up. */
    UFUNCTION(BlueprintCallable, Category = "raceGPS|Profile")
    void AddXP(int32 Amount);

    /** Unlock a part by ID. */
    UFUNCTION(BlueprintCallable, Category = "raceGPS|Profile")
    void UnlockPart(const FString& PartId);

    /** Unlock a material by ID. */
    UFUNCTION(BlueprintCallable, Category = "raceGPS|Profile")
    void UnlockMaterial(const FString& MaterialId);

    /** Unlock a decal by ID. */
    UFUNCTION(BlueprintCallable, Category = "raceGPS|Profile")
    void UnlockDecal(const FString& DecalId);

    /** Add distance driven (meters). */
    UFUNCTION(BlueprintCallable, Category = "raceGPS|Profile")
    void AddDistance(float Meters);

    /**
     * Record a completed race.
     * @param bWon Whether the player won.
     * @param LapTime Best lap time for this track.
     * @param TrackId Track identifier.
     */
    UFUNCTION(BlueprintCallable, Category = "raceGPS|Profile")
    void RecordRaceFinished(bool bWon, float LapTime, const FString& TrackId);

    /** Check if a part is unlocked. */
    UFUNCTION(BlueprintPure, Category = "raceGPS|Profile")
    bool HasUnlockedPart(const FString& PartId) const;

    /** Check if a material is unlocked. */
    UFUNCTION(BlueprintPure, Category = "raceGPS|Profile")
    bool HasUnlockedMaterial(const FString& MaterialId) const;

    /** Check if a decal is unlocked. */
    UFUNCTION(BlueprintPure, Category = "raceGPS|Profile")
    bool HasUnlockedDecal(const FString& DecalId) const;

    /** Get best lap time for a track (-1.0f if none). */
    UFUNCTION(BlueprintPure, Category = "raceGPS|Profile")
    float GetBestLapTime(const FString& TrackId) const;

    /** Default save slot name. */
    static const FString SaveSlotName;

    /** Default save user index. */
    static const int32 SaveUserIndex;

protected:
    /** Check for level-up after XP changes. */
    void CheckLevelUp();

    /** XP required per level. */
    static const int32 XPPerLevel;
};
