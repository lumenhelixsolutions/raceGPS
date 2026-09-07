#pragma once

#include "CoreMinimal.h"

/**
 * Thin include shim for existing module types that live elsewhere in raceGPSAkronBeta.
 * Do not treat this as a replacement for AChaosVehiclePawn / URaceSessionManager.
 */

#if __has_include("ChaosVehiclePawn.h")
#include "ChaosVehiclePawn.h"
#elif __has_include("WheeledVehiclePawn.h")
#include "WheeledVehiclePawn.h"
using AChaosVehiclePawn = AWheeledVehiclePawn;
#else
#include "WheeledVehiclePawn.h"
using AChaosVehiclePawn = AWheeledVehiclePawn;
#endif

#if __has_include("RaceSessionManager.h")
#include "RaceSessionManager.h"
#endif

#ifndef RACEGPS_HAS_SESSION_ENUM
#define RACEGPS_HAS_SESSION_ENUM 1
// Existing URaceSessionManager states: Menu, Countdown, Racing, Paused, Finished.
// If the module enum is named differently, map it in RACESESSIONMANAGER_PATCH.md.
#endif

inline bool RaceGPS_IsSessionRacing(const URaceSessionManager* Session)
{
	if (!Session)
	{
		return false;
	}
	// Requires the tiny additive getter described in RACESESSIONMANAGER_PATCH.md.
	return Session->GetCurrentState() == ERaceSessionState::Racing;
}

inline bool RaceGPS_IsSessionCountdownOrMenu(const URaceSessionManager* Session)
{
	if (!Session)
	{
		return true;
	}
	const ERaceSessionState St = Session->GetCurrentState();
	return St == ERaceSessionState::Menu || St == ERaceSessionState::Countdown;
}
