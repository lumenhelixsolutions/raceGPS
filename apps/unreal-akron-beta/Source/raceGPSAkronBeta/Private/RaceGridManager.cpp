#include "RaceGridManager.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "ClevelandModuleCompat.h"
#include "RacingLineComponent.h"
#include "RaceAIDriverController.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "CollisionQueryParams.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectGlobals.h"

ARaceGridManager::ARaceGridManager()
{
	PrimaryActorTick.bCanEverTick = true;
	RacingLine = CreateDefaultSubobject<URacingLineComponent>(TEXT("RacingLine"));
	AIControllerClass = ARaceAIDriverController::StaticClass();
	SlotRoles = { ERaceGridSlotRole::Player, ERaceGridSlotRole::AI, ERaceGridSlotRole::AI };
	SlotLooks = { EVehicleLook::Hellcat, EVehicleLook::ChargerAsphalt, EVehicleLook::ChargerSilver };

	// Cleveland showcase visuals + real Chaos WheelSetups live on the CARLA Charger BP.
	static ConstructorHelpers::FClassFinder<AChaosVehiclePawn> ChargerBP(
		TEXT("/Game/Vehicles/DodgeCharger2024/BP_DodgeCharger2024"));
	if (ChargerBP.Succeeded())
	{
		VehicleClass = ChargerBP.Class;
	}
}

void ARaceGridManager::BeginPlay()
{
	Super::BeginPlay();
}

void ARaceGridManager::BindSession(URaceSessionManager* InSession)
{
	SessionManager = InSession;
}

FTransform ARaceGridManager::ComputeGridPose(int32 SlotIndex) const
{
	if (!RacingLine || !RacingLine->IsValidLine())
	{
		return FTransform(FRotator::ZeroRotator, FVector(0.f, static_cast<float>(SlotIndex) * SlotSpacingCm, 50.f));
	}
	const float S = RaceAIControlMath::WrapS(-static_cast<float>(SlotIndex) * SlotSpacingCm, RacingLine->TrackLength);
	const float Lateral = (SlotIndex % 2 == 1) ? StaggerLateralCm : 0.f;
	FTransform Pose = RacingLine->GetPoseAtS(S, Lateral);
	FVector Loc = Pose.GetLocation();
	Loc.Z += 40.f;
	Pose.SetLocation(Loc);
	return Pose;
}

void ARaceGridManager::PossessPlayer(APlayerController* PlayerPC, AChaosVehiclePawn* Pawn)
{
	if (!PlayerPC || !Pawn)
	{
		UE_LOG(LogTemp, Error, TEXT("raceGPS Cleveland: possess failed (pc=%s pawn=%s)"),
			*GetNameSafe(PlayerPC), *GetNameSafe(Pawn));
		return;
	}
	PlayerPC->Possess(Pawn);
	Pawn->EnableInput(PlayerPC);
	PlayerPC->SetIgnoreMoveInput(false);
	PlayerPC->SetIgnoreLookInput(false);
	UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland: possessed player pawn=%s pcPawn=%s inputEnabled=%d"),
		*GetNameSafe(Pawn), *GetNameSafe(PlayerPC->GetPawn()), Pawn->InputEnabled() ? 1 : 0);
}

void ARaceGridManager::SnapPawnToGround(AChaosVehiclePawn* Pawn)
{
	UWorld* World = GetWorld();
	if (!World || !Pawn)
	{
		return;
	}
	const FVector Loc = Pawn->GetActorLocation();
	const FVector Start = Loc + FVector(0.f, 0.f, 8000.f);
	const FVector End = Loc - FVector(0.f, 0.f, 20000.f);
	FCollisionQueryParams Params(NAME_None, true, Pawn);
	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params)
		|| World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params);
	if (bHit && Hit.bBlockingHit)
	{
		FVector NewLoc = Loc;
		NewLoc.Z = Hit.ImpactPoint.Z + 32.f;
		Pawn->SetActorLocation(NewLoc, false, nullptr, ETeleportType::TeleportPhysics);
		UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland: snap slot pawn=%s z %.1f -> %.1f"),
			*GetNameSafe(Pawn), Loc.Z, NewLoc.Z);
	}
}

void ARaceGridManager::HoldPlayerForCountdown()
{
	if (!RaceGPS_IsSessionRacing(SessionManager))
	{
		for (AChaosVehiclePawn* Pawn : Pawns)
		{
			if (!Pawn)
			{
				continue;
			}
			if (!Pawn->IsDriveOverrideActive())
			{
				Pawn->SetThrottleInput(0.f);
				Pawn->SetSteeringInput(0.f);
				Pawn->SetBrakeInput(1.f);
				Pawn->SetHandbrakeInput(true);
			}
			// Force forward gear while held so green flag cannot launch in reverse (gear=-1).
			if (auto* Move = Pawn->GetVehicleMovementComponent())
			{
				if (auto* W = Cast<UChaosWheeledVehicleMovementComponent>(Move))
				{
					W->SetTargetGear(1, true);
				}
			}
		}
		bReleasedPlayerHandbrake = false;
		RaceLiveElapsed = 0.f;
		return;
	}
	if (!bReleasedPlayerHandbrake)
	{
		ReleaseAllForRacing();
	}
}

void ARaceGridManager::ReleaseAllForRacing()
{
	bReleasedPlayerHandbrake = true;
	for (AChaosVehiclePawn* Pawn : Pawns)
	{
		if (!Pawn)
		{
			continue;
		}
		Pawn->CloseVehicleDoors();
		// AI Hold uses SetDriveOverride(0,0,1,true); SetBrakeInput is ignored while override is on.
		Pawn->ReleaseForRace();
	}
	UE_LOG(LogTemp, Warning, TEXT("raceGPS Cleveland: race live - released handbrake/wake on %d pawns autodive=%d"),
		Pawns.Num(), bAutoDrivePlayer ? 1 : 0);
	DumpGridDriveState(TEXT("release"));
}

void ARaceGridManager::DumpGridDriveState(const TCHAR* Tag)
{
	for (int32 i = 0; i < Pawns.Num(); ++i)
	{
		AChaosVehiclePawn* Pawn = Pawns[i];
		if (!Pawn)
		{
			continue;
		}
		Pawn->DumpDriveState(*FString::Printf(TEXT("%s slot=%d"), Tag ? Tag : TEXT("?"), i));
	}
	if (PlayerAutoDriver)
	{
		UE_LOG(LogTemp, Warning, TEXT("raceGPS Cleveland drive-dump %s autoDriver thr=%.2f brake=%.2f steer=%.2f rec=%s live=%d"),
			Tag ? Tag : TEXT("?"),
			PlayerAutoDriver->ThrottleCommand,
			PlayerAutoDriver->BrakeCommand,
			PlayerAutoDriver->SteeringCommand,
			*UEnum::GetValueAsString(PlayerAutoDriver->RecoveryState),
			RaceGPS_IsSessionRacing(SessionManager) ? 1 : 0);
	}
}

void ARaceGridManager::DestroyGrid()
{
	for (ARaceAIDriverController* AI : AIControllers)
	{
		if (AI)
		{
			AI->UnPossess();
			AI->Destroy();
		}
	}
	AIControllers.Reset();

	for (AChaosVehiclePawn* Pawn : Pawns)
	{
		if (Pawn)
		{
			Pawn->Destroy();
		}
	}
	if (PlayerAutoDriver)
	{
		PlayerAutoDriver->Destroy();
		PlayerAutoDriver = nullptr;
	}
	Pawns.Reset();
	PlayerPawn = nullptr;
	bReleasedPlayerHandbrake = false;
	FinishTimes.Reset();
	SlotLaps.Reset();
	bPlayerFinished = false;
	bRaceEnded = false;
	FinishedCount = 0;
}

bool ARaceGridManager::SpawnGrid(APlayerController* PlayerPC)
{
	DestroyGrid();

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	if (!VehicleClass)
	{
		VehicleClass = LoadClass<AChaosVehiclePawn>(nullptr,
			TEXT("/Game/Vehicles/DodgeCharger2024/BP_DodgeCharger2024.BP_DodgeCharger2024_C"));
	}
	if (!VehicleClass)
	{
		UE_LOG(LogTemp, Error, TEXT("raceGPS Cleveland: BP_DodgeCharger2024 missing - falling back to C++ AChaosVehiclePawn (synthesized wheels)"));
		VehicleClass = AChaosVehiclePawn::StaticClass();
	}
	UE_LOG(LogTemp, Warning, TEXT("raceGPS Cleveland: grid VehicleClass=%s native=%s"),
		*GetNameSafe(VehicleClass), *GetNameSafe(AChaosVehiclePawn::StaticClass()));
	if (!AIControllerClass)
	{
		AIControllerClass = ARaceAIDriverController::StaticClass();
	}
	if (SlotRoles.Num() < NumSlots)
	{
		SlotRoles = { ERaceGridSlotRole::Player, ERaceGridSlotRole::AI, ERaceGridSlotRole::AI };
	}

	FinishTimes.SetNumZeroed(NumSlots);
	SlotLaps.SetNumZeroed(NumSlots);
	PlayerPrevS = 0.f;
	bHavePlayerPrevS = false;
	bPlayerLeftStartZone = false;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	for (int32 i = 0; i < NumSlots; ++i)
	{
		const FTransform Pose = ComputeGridPose(i);
		AChaosVehiclePawn* Pawn = World->SpawnActor<AChaosVehiclePawn>(VehicleClass, Pose, Params);
		if (!Pawn)
		{
			UE_LOG(LogTemp, Error, TEXT("raceGPS Cleveland: failed to spawn Chaos vehicle slot %d class=%s"), i, *GetNameSafe(VehicleClass));
			continue;
		}
		UE_LOG(LogTemp, Warning, TEXT("raceGPS Cleveland: spawned slot=%d class=%s name=%s"),
			i, *GetNameSafe(Pawn->GetClass()), *GetNameSafe(Pawn));
		SnapPawnToGround(Pawn);
		Pawn->CloseVehicleDoors();
		Pawn->WakeForDrive();
		const EVehicleLook Look = SlotLooks.IsValidIndex(i) ? SlotLooks[i] : EVehicleLook::ChargerAsphalt;
		Pawn->ApplyVehicleLook(Look);
		Pawns.Add(Pawn);

		if (SlotRoles[i] == ERaceGridSlotRole::Player)
		{
			PlayerPawn = Pawn;
			PossessPlayer(PlayerPC, Pawn);
			if (bAutoDrivePlayer)
			{
				PlayerAutoDriver = World->SpawnActor<ARaceAIDriverController>(AIControllerClass, Pose, Params);
				if (PlayerAutoDriver)
				{
					PlayerAutoDriver->ConfigureDriver(RacingLine, SessionManager, i, FRaceAIPersonality::ConservativeAI01());
					PlayerAutoDriver->SetGridManager(this);
					UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland: player auto-drive armed (AI-as-player, PC still possesses)"));
				}
			}
		}
		else
		{
			ARaceAIDriverController* AI = World->SpawnActor<ARaceAIDriverController>(AIControllerClass, Pose, Params);
			if (!AI)
			{
				UE_LOG(LogTemp, Error, TEXT("raceGPS Cleveland: failed to spawn AI controller slot %d"), i);
				continue;
			}
			const FRaceAIPersonality Persona = (i == 1)
				? FRaceAIPersonality::ConservativeAI01()
				: FRaceAIPersonality::AggressiveAI02();
			AI->ConfigureDriver(RacingLine, SessionManager, i, Persona);
			AI->SetGridManager(this);
			AI->Possess(Pawn);
			AIControllers.Add(AI);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland: grid spawned (%d pawns) before countdown"), Pawns.Num());
	return Pawns.Num() == NumSlots;
}

void ARaceGridManager::RespawnGridAndRestart(APlayerController* PlayerPC)
{
	SpawnGrid(PlayerPC);
	if (SessionManager)
	{
		SessionManager->StartSession(TEXT("cleveland_burke_gp_1997"), TEXT("hellcat"));
	}
}

void ARaceGridManager::NotifyVehicleLapComplete(int32 InSlotIndex, float LapTimeSec)
{
	if (!SlotLaps.IsValidIndex(InSlotIndex))
	{
		return;
	}
	if (SlotLaps[InSlotIndex] >= TargetLaps)
	{
		return;
	}
	SlotLaps[InSlotIndex] += 1;
	UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland: lap complete slot=%d role=%s time=%.2f laps=%d"),
		InSlotIndex,
		(SlotRoles.IsValidIndex(InSlotIndex) && SlotRoles[InSlotIndex] == ERaceGridSlotRole::Player) ? TEXT("PLAYER") : TEXT("AI"),
		LapTimeSec, SlotLaps[InSlotIndex]);
	if (FinishTimes.IsValidIndex(InSlotIndex) && FinishTimes[InSlotIndex] <= 0.f)
	{
		FinishTimes[InSlotIndex] = LapTimeSec;
	}
	if (SlotRoles.IsValidIndex(InSlotIndex) && SlotRoles[InSlotIndex] == ERaceGridSlotRole::Player)
	{
		bPlayerFinished = true;
	}
	FinishedCount = 0;
	for (int32 Lap : SlotLaps)
	{
		if (Lap >= TargetLaps)
		{
			++FinishedCount;
		}
	}
}

bool ARaceGridManager::HaveAllFinished() const
{
	return FinishedCount >= NumSlots;
}

int32 ARaceGridManager::GetPlayerPlace() const
{
	const TArray<FRaceStandingEntry> S = GetStandings();
	for (const FRaceStandingEntry& E : S)
	{
		if (E.Role == ERaceGridSlotRole::Player)
		{
			return E.Place;
		}
	}
	return 0;
}

TArray<FRaceStandingEntry> ARaceGridManager::GetStandings() const
{
	TArray<FRaceStandingEntry> Entries;
	const float Length = (RacingLine && RacingLine->IsValidLine()) ? RacingLine->TrackLength : 0.f;

	auto ProgressForPawn = [&](int32 Index) -> TPair<int32, float>
	{
		int32 Lap = SlotLaps.IsValidIndex(Index) ? SlotLaps[Index] : 0;
		float S = 0.f;
		if (SlotRoles.IsValidIndex(Index) && SlotRoles[Index] == ERaceGridSlotRole::AI)
		{
			for (ARaceAIDriverController* AI : AIControllers)
			{
				if (AI && AI->SlotIndex == Index)
				{
					Lap = AI->LapIndex;
					S = AI->CurrentSplineDistance;
					break;
				}
			}
		}
		else if (Pawns.IsValidIndex(Index) && Pawns[Index] && RacingLine)
		{
			S = RacingLine->GetNearestS(Pawns[Index]->GetActorLocation());
		}
		return TPair<int32, float>(Lap, S);
	};

	for (int32 i = 0; i < Pawns.Num(); ++i)
	{
		FRaceStandingEntry E;
		E.SlotIndex = i;
		E.Role = SlotRoles.IsValidIndex(i) ? SlotRoles[i] : ERaceGridSlotRole::AI;
		const TPair<int32, float> PS = ProgressForPawn(i);
		E.LapIndex = PS.Key;
		E.CurrentSplineDistance = PS.Value;
		E.RaceProgress = RaceAIControlMath::RaceProgress(E.LapIndex, Length, E.CurrentSplineDistance);
		E.LastLapTimeSec = FinishTimes.IsValidIndex(i) ? FinishTimes[i] : 0.f;
		E.bFinished = E.LapIndex >= TargetLaps;
		Entries.Add(E);
	}

	Entries.Sort([](const FRaceStandingEntry& A, const FRaceStandingEntry& B)
	{
		return A.RaceProgress > B.RaceProgress;
	});

	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		Entries[i].Place = i + 1;
	}
	return Entries;
}

void ARaceGridManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	HoldPlayerForCountdown();
	if (RaceGPS_IsSessionRacing(SessionManager))
	{
		RaceLiveElapsed += DeltaSeconds;
		if (PlayerPawn && PlayerPawn->GetSpeedKmh() < 0.5f && RaceLiveElapsed < 8.f)
		{
			PlayerPawn->WakeForDrive();
		}
	}
	if (bAutoDrivePlayer && PlayerAutoDriver && PlayerPawn)
	{
		PlayerAutoDriver->TickDriveForPawn(PlayerPawn, DeltaSeconds);
	}
	TickStandings(GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f);
}

void ARaceGridManager::TickStandings(float WorldTimeSec)
{
	if (!PlayerPawn || !RacingLine || !RacingLine->IsValidLine())
	{
		return;
	}

	const float S = RacingLine->GetNearestS(PlayerPawn->GetActorLocation());
	const float Length = RacingLine->TrackLength;
	if (!bHavePlayerPrevS)
	{
		PlayerPrevS = S;
		bHavePlayerPrevS = true;
		return;
	}
	// Closed-loop start==finish: ignore wrap until the car has actually been racing long enough.
	if (S > 0.15f * Length && S < 0.90f * Length)
	{
		bPlayerLeftStartZone = true;
	}
	const bool bLegitimateWrap =
		bPlayerLeftStartZone &&
		(PlayerPrevS > 0.80f * Length) &&
		(S < 0.20f * Length);
	if (bLegitimateWrap)
	{
		int32 PlayerSlot = INDEX_NONE;
		for (int32 i = 0; i < SlotRoles.Num(); ++i)
		{
			if (SlotRoles[i] == ERaceGridSlotRole::Player)
			{
				PlayerSlot = i;
				break;
			}
		}
		if (PlayerSlot != INDEX_NONE && SlotLaps.IsValidIndex(PlayerSlot) && SlotLaps[PlayerSlot] < TargetLaps)
		{
			const float LapTime = FinishTimes.IsValidIndex(PlayerSlot) ? FMath::Max(0.f, WorldTimeSec) : WorldTimeSec;
			NotifyVehicleLapComplete(PlayerSlot, LapTime);
		}
	}
	PlayerPrevS = S;
}
