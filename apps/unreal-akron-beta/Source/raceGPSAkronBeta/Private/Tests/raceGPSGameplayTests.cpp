// Copyright LumenHelix Solutions. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "RaceScoringSystem.h"
#include "AchievementSystem.h"
#include "LeaderboardSystem.h"
#include "VehicleTuningData.h"

// ---------------------------------------------------------------------------
// Race Scoring Tests
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FraceGPSScoringPerfectRun, "raceGPS.Gameplay.Scoring.PerfectRun",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FraceGPSScoringPerfectRun::RunTest(const FString& Parameters)
{
    URaceScoringSystem* Scoring = NewObject<URaceScoringSystem>();
    Scoring->Reset();

    // Simulate a 110s race with 0 collisions, 0 missed checkpoints
    Scoring->BaseTime = 110.0f;
    Scoring->Collisions = 0;
    Scoring->MissedCheckpoints = 0;

    FRaceScore Score = Scoring->CalculateFinalScore(110.0f);

    TestEqual(TEXT("Base time should be 110"), Score.BaseTime, 110.0f);
    TestEqual(TEXT("Collision penalty should be 0"), Score.CollisionPenalty, 0.0f);
    TestEqual(TEXT("Clean bonus should be -1.0"), Score.CleanDrivingBonus, -1.0f);
    TestEqual(TEXT("Final time should be 109.0"), Score.FinalTime, 109.0f);
    TestEqual(TEXT("Medal should be GOLD"), Score.Medal, FString(TEXT("GOLD")));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FraceGPSScoringMessyRun, "raceGPS.Gameplay.Scoring.MessyRun",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FraceGPSScoringMessyRun::RunTest(const FString& Parameters)
{
    URaceScoringSystem* Scoring = NewObject<URaceScoringSystem>();
    Scoring->Reset();

    Scoring->BaseTime = 100.0f;
    Scoring->Collisions = 3;
    Scoring->MissedCheckpoints = 1;

    FRaceScore Score = Scoring->CalculateFinalScore(100.0f);

    // 100 + (3 * 2) + (1 * 5) - 0 = 111
    TestEqual(TEXT("Final time should be 111.0"), Score.FinalTime, 111.0f);
    TestEqual(TEXT("Collision penalty should be 6.0"), Score.CollisionPenalty, 6.0f);
    TestEqual(TEXT("Missed CP penalty should be 5.0"), Score.MissedCheckpointPenalty, 5.0f);
    TestEqual(TEXT("Clean bonus should be 0"), Score.CleanDrivingBonus, 0.0f);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FraceGPSScoringMedalBoundaries, "raceGPS.Gameplay.Scoring.MedalBoundaries",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FraceGPSScoringMedalBoundaries::RunTest(const FString& Parameters)
{
    URaceScoringSystem* Scoring = NewObject<URaceScoringSystem>();
    Scoring->Reset();

    // Gold boundary
    Scoring->Collisions = 0;
    Scoring->MissedCheckpoints = 0;
    FRaceScore Score = Scoring->CalculateFinalScore(120.0f);
    TestEqual(TEXT("120s clean should be GOLD"), Score.Medal, FString(TEXT("GOLD")));

    // Silver boundary
    Scoring->Reset();
    Scoring->Collisions = 0;
    Scoring->MissedCheckpoints = 0;
    Score = Scoring->CalculateFinalScore(150.0f);
    TestEqual(TEXT("150s clean should be SILVER"), Score.Medal, FString(TEXT("SILVER")));

    // Bronze boundary
    Scoring->Reset();
    Scoring->Collisions = 0;
    Scoring->MissedCheckpoints = 0;
    Score = Scoring->CalculateFinalScore(200.0f);
    TestEqual(TEXT("200s clean should be BRONZE"), Score.Medal, FString(TEXT("BRONZE")));

    // No medal
    Scoring->Reset();
    Scoring->Collisions = 0;
    Scoring->MissedCheckpoints = 0;
    Score = Scoring->CalculateFinalScore(201.0f);
    TestEqual(TEXT("201s clean should be NONE"), Score.Medal, FString(TEXT("NONE")));

    return true;
}

// ---------------------------------------------------------------------------
// Achievement Tests
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FraceGPSAchievementUnlock, "raceGPS.Gameplay.Achievements.Unlock",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FraceGPSAchievementUnlock::RunTest(const FString& Parameters)
{
    UAchievementSystem* Achievements = NewObject<UAchievementSystem>();
    Achievements->InitializeAchievements();

    TestFalse(TEXT("first_race should start locked"), Achievements->IsUnlocked(TEXT("first_race")));

    Achievements->UnlockAchievement(TEXT("first_race"));
    TestTrue(TEXT("first_race should be unlocked after UnlockAchievement"), Achievements->IsUnlocked(TEXT("first_race")));

    // Double-unlock should be safe
    Achievements->UnlockAchievement(TEXT("first_race"));
    TestTrue(TEXT("first_race should still be unlocked"), Achievements->IsUnlocked(TEXT("first_race")));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FraceGPSAchievementProgress, "raceGPS.Gameplay.Achievements.Progress",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FraceGPSAchievementProgress::RunTest(const FString& Parameters)
{
    UAchievementSystem* Achievements = NewObject<UAchievementSystem>();
    Achievements->InitializeAchievements();

    // gold_medalist requires 3 progress
    Achievements->AddProgress(TEXT("gold_medalist"), 1);
    TestFalse(TEXT("gold_medalist should be locked at 1/3"), Achievements->IsUnlocked(TEXT("gold_medalist")));

    Achievements->AddProgress(TEXT("gold_medalist"), 1);
    TestFalse(TEXT("gold_medalist should be locked at 2/3"), Achievements->IsUnlocked(TEXT("gold_medalist")));

    Achievements->AddProgress(TEXT("gold_medalist"), 1);
    TestTrue(TEXT("gold_medalist should unlock at 3/3"), Achievements->IsUnlocked(TEXT("gold_medalist")));

    return true;
}

// ---------------------------------------------------------------------------
// Leaderboard Tests
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FraceGPSLeaderboardAddAndSort, "raceGPS.Gameplay.Leaderboard.AddAndSort",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FraceGPSLeaderboardAddAndSort::RunTest(const FString& Parameters)
{
    ULeaderboardSystem* Leaderboards = NewObject<ULeaderboardSystem>();
    FString RouteId = TEXT("test_route");

    Leaderboards->SeedDefaultEntries(RouteId, 120.0f, 150.0f, 200.0f);

    // Add a player entry
    FLeaderboardEntry Entry;
    Entry.PlayerName = TEXT("Player");
    Entry.TimeSeconds = 130.0f;
    Entry.Medal = TEXT("SILVER");
    Entry.Date = TEXT("2026-06-04");
    Entry.VehicleUsed = TEXT("Sedan");
    Entry.Collisions = 1;
    Entry.bIsPlayer = true;
    Leaderboards->AddEntry(RouteId, Entry);

    TArray<FLeaderboardEntry> Entries = Leaderboards->GetEntries(RouteId);
    TestTrue(TEXT("Leaderboard should have entries"), Entries.Num() > 0);

    // Player at 130s should rank below Gold (120s) but above Silver AI
    int32 Rank = Leaderboards->GetPlayerRank(RouteId, 130.0f);
    TestEqual(TEXT("Player rank at 130s should be 2 (below Gold)"), Rank, 2);

    return true;
}

// ---------------------------------------------------------------------------
// Vehicle Tuning Tests
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FraceGPSVehicleTuningPreset, "raceGPS.Gameplay.Vehicle.TuningPreset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FraceGPSVehicleTuningPreset::RunTest(const FString& Parameters)
{
    UVehicleTuningData* Tuning = NewObject<UVehicleTuningData>();
    Tuning->DisplayName = TEXT("Test Vehicle");
    Tuning->Description = TEXT("A test vehicle for validation.");
    Tuning->VehicleClass = EVehicleClass::Sports;
    Tuning->VehicleMass = 1200.0f;
    Tuning->MaxEngineRPM = 8000.0f;

    TestEqual(TEXT("Display name should match"), Tuning->DisplayName, FString(TEXT("Test Vehicle")));
    TestEqual(TEXT("Vehicle class should be Sports"), static_cast<uint8>(Tuning->VehicleClass), static_cast<uint8>(EVehicleClass::Sports));
    TestEqual(TEXT("Mass should be 1200"), Tuning->VehicleMass, 1200.0f);

    return true;
}
