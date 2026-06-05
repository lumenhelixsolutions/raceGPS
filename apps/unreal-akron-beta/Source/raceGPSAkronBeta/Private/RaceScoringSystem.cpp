#include "RaceScoringSystem.h"

void URaceScoringSystem::Reset()
{
    CollisionCount = 0;
    MissedCheckpointCount = 0;
    ReachedCheckpointCount = 0;
    TotalCollisionPenalty = 0.0f;
}

void URaceScoringSystem::OnCollision(float ImpactSpeedKmh)
{
    CollisionCount++;
    float Penalty = CollisionPenaltyPerHit + (ImpactSpeedKmh * CollisionSpeedMultiplier);
    TotalCollisionPenalty += Penalty;
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Collision #%d at %.0f km/h, penalty +%.1fs"),
        CollisionCount, ImpactSpeedKmh, Penalty);
}

void URaceScoringSystem::OnMissedCheckpoint()
{
    MissedCheckpointCount++;
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Missed checkpoint #%d, penalty +%.1fs"),
        MissedCheckpointCount, MissedCheckpointPenalty);
}

void URaceScoringSystem::OnCheckpointReached()
{
    ReachedCheckpointCount++;
}

FRaceScore URaceScoringSystem::CalculateFinalScore(float ElapsedTime) const
{
    FRaceScore Score;
    Score.BaseTime = ElapsedTime;
    Score.Collisions = CollisionCount;
    Score.MissedCheckpoints = MissedCheckpointCount;
    Score.CollisionPenalty = TotalCollisionPenalty;
    Score.MissedCheckpointPenalty = MissedCheckpointCount * MissedCheckpointPenalty;

    float Bonus = 0.0f;
    if (CollisionCount == 0 && MissedCheckpointCount == 0 && ElapsedTime <= CleanDrivingThresholdSeconds)
    {
        Bonus = CleanDrivingBonus;
        Score.CleanDrivingBonus = Bonus;
    }

    Score.FinalTime = ElapsedTime + Score.CollisionPenalty + Score.MissedCheckpointPenalty - Bonus;

    if (Score.FinalTime <= 120.0f)
        Score.Medal = TEXT("GOLD");
    else if (Score.FinalTime <= 150.0f)
        Score.Medal = TEXT("SILVER");
    else if (Score.FinalTime <= 200.0f)
        Score.Medal = TEXT("BRONZE");
    else
        Score.Medal = TEXT("NONE");

    return Score;
}

float URaceScoringSystem::GetCurrentPenaltyTotal() const
{
    return TotalCollisionPenalty + (MissedCheckpointCount * MissedCheckpointPenalty);
}
