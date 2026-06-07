// Copyright raceGPS. All Rights Reserved.

#include "DriftScoreSystem.h"
#include "Math/UnrealMathUtility.h"

void UDriftScoreSystem::StartDrift()
{
    if (bDrifting)
        return;

    bDrifting = true;
    DriftStartTime = FPlatformTime::Seconds();
    CurrentDriftScore = 0.0f;
    CurrentMaxAngle = 0.0f;
    CurrentSpeedSum = 0.0f;
    CurrentSampleCount = 0;
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Drift started"));
}

void UDriftScoreSystem::UpdateDrift(float Angle, float Speed)
{
    if (!bDrifting)
        return;

    float AbsAngle = FMath::Abs(Angle);
    if (AbsAngle < AngleThreshold || Speed < SpeedThreshold)
    {
        // Drift interrupted
        EndDrift();
        return;
    }

    CurrentMaxAngle = FMath::Max(CurrentMaxAngle, AbsAngle);
    CurrentSpeedSum += Speed;
    CurrentSampleCount++;

    float Multiplier = CalculateMultiplier(AbsAngle, Speed);
    float Points = (AbsAngle * 0.1f + Speed * 0.01f) * Multiplier;
    CurrentDriftScore += Points;
}

FDriftSession UDriftScoreSystem::EndDrift()
{
    FDriftSession Session;
    if (!bDrifting)
        return Session;

    bDrifting = false;
    Session.StartTime = DriftStartTime;
    Session.EndTime = FPlatformTime::Seconds();
    Session.TotalAngle = CurrentMaxAngle;
    Session.PeakAngle = CurrentMaxAngle;
    Session.AverageSpeed = CurrentSampleCount > 0 ? CurrentSpeedSum / CurrentSampleCount : 0.0f;
    Session.Score = CurrentDriftScore;

    TotalScore += CurrentDriftScore;
    DriftHistory.Add(Session);

    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Drift ended. Score: %.0f (multiplier: %.1fx)"),
        Session.Score, GetMultiplier());
    return Session;
}

float UDriftScoreSystem::GetCurrentScore() const
{
    return bDrifting ? CurrentDriftScore : 0.0f;
}

float UDriftScoreSystem::GetMultiplier() const
{
    if (!bDrifting)
        return 1.0f;

    float AvgSpeed = CurrentSampleCount > 0 ? CurrentSpeedSum / CurrentSampleCount : 0.0f;
    return CalculateMultiplier(CurrentMaxAngle, AvgSpeed);
}

void UDriftScoreSystem::ResetScore()
{
    TotalScore = 0.0f;
    DriftHistory.Empty();
    bDrifting = false;
    CurrentDriftScore = 0.0f;
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Drift score reset"));
}

float UDriftScoreSystem::CalculateMultiplier(float Angle, float Speed) const
{
    float AngleBonus = FMath::Clamp((Angle - AngleThreshold) / 30.0f, 0.0f, 3.0f);
    float SpeedBonus = FMath::Clamp((Speed - SpeedThreshold) / 50.0f, 0.0f, 2.0f);
    return ScoreMultiplierBase + AngleBonus + SpeedBonus;
}
