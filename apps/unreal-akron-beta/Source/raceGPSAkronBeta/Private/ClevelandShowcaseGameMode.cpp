#include "ClevelandShowcaseGameMode.h"
#include "ClevelandModuleCompat.h"
#include "RaceGridManager.h"
#include "RacingLineComponent.h"
#include "ClevelandEnvironmentActor.h"
#include "ClevelandLookDirector.h"
#include "ChaosVehiclePawn.h"
#include "CheckpointGate.h"
#include "RaceAIDriverController.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "Misc/DateTime.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformMisc.h"

AClevelandShowcaseGameMode::AClevelandShowcaseGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	ProductTitle = TEXT("raceGPS");
	CircuitTitle = TEXT("CLEVELAND HISTORIC CIRCUIT");
	DefaultPawnClass = nullptr; // grid manager spawns the Chaos pawn
	SessionManager = CreateDefaultSubobject<URaceSessionManager>(TEXT("SessionManager"));
}

FString AClevelandShowcaseGameMode::ResolveCityPackPath(const FString& FileName) const
{
	const TArray<FString> Roots = {
		FPaths::Combine(FPaths::ProjectContentDir(), CityPackRelativeDir),
		FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Dir"), CityPackRelativeDir),
		FPaths::Combine(FPaths::ProjectDir(), CityPackRelativeDir),
		FPaths::Combine(FPaths::ProjectContentDir(), TEXT("citypacks/cleveland/burke_gp_1997")),
		FPaths::ProjectContentDir()
	};
	for (const FString& Root : Roots)
	{
		const FString Path = FPaths::Combine(Root, FileName);
		if (FPaths::FileExists(Path))
		{
			return Path;
		}
	}
	return FPaths::Combine(FPaths::ProjectContentDir(), CityPackRelativeDir, FileName);
}

int32 AClevelandShowcaseGameMode::LoadCheckpointCount() const
{
	const FString Path = ResolveCityPackPath(TEXT("checkpoints.json"));
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *Path))
	{
		UE_LOG(LogTemp, Warning, TEXT("raceGPS Cleveland: checkpoints.json missing at %s"), *Path);
		return 0;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return 0;
	}
	static const TCHAR* Keys[] = { TEXT("checkpoints"), TEXT("gates"), TEXT("points"), TEXT("items") };
	for (const TCHAR* Key : Keys)
	{
		if (Root->HasTypedField<EJson::Array>(Key))
		{
			return Root->GetArrayField(Key).Num();
		}
	}
	if (Root->HasTypedField<EJson::Number>(TEXT("count")))
	{
		return static_cast<int32>(Root->GetNumberField(TEXT("count")));
	}
	return 0;
}

void AClevelandShowcaseGameMode::LoadCityPack()
{
	LoadCheckpointCourse();
	if (GridManager && GridManager->RacingLine)
	{
		const FString LinePath = ResolveCityPackPath(TEXT("racing_line.json"));
		if (!GridManager->RacingLine->LoadFromJsonFile(LinePath))
		{
			GridManager->RacingLine->LoadDefaultClevelandLine();
		}
	}
	UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland: citypack loaded checkpoints=%d title=%s / %s"),
		TotalCheckpoints, *ProductTitle, *CircuitTitle);
}

void AClevelandShowcaseGameMode::BindHud()
{
	// HUD widgets bind to GetHudTitleLine(), GridManager->GetStandings(),
	// GridManager->GetPlayerPlace(), and AI telemetry UPROPERTYs.
	UE_LOG(LogTemp, Log, TEXT("raceGPS HUD: %s"), *GetHudTitleLine());
}

FString AClevelandShowcaseGameMode::GetHudTitleLine() const
{
	return FString::Printf(TEXT("%s  -  %s  (%d/3)"), *ProductTitle, *CircuitTitle,
		GridManager ? GridManager->GetPlayerPlace() : 0);
}

void AClevelandShowcaseGameMode::StartSkylineIntro(APlayerController* PC)
{
	UWorld* World = GetWorld();
	AChaosVehiclePawn* Pawn = GridManager ? GridManager->GetPlayerPawn() : nullptr;
	if (!World || !PC || !Pawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("raceGPS Cleveland: skip skyline intro (missing world/pc/pawn)"));
		return;
	}

	/*
	 * Intended framing (V1):
	 *   Geo: X=east, Y=north. Burke start heading ~247 deg WSW.
	 *   T10 HISM downtown (~120k buildings) + Karla silhouette sit SOUTH (-Y).
	 *   Lake Erie sheet sits NORTH (+Y). A north-facing chase along the runway
	 *   therefore puts the city behind the camera.
	 *   Intro camera: EAST of the grid, elevated, looking WSW along the cars so
	 *   downtown is LEFT of frame and Erie is RIGHT, with the 3-car grid in the
	 *   foreground. Then blend to the raised 3/4 pawn chase (left/south bias).
	 */
	const FVector PawnLoc = Pawn->GetActorLocation();
	// V15: magazine horizon lock - downtown sits ON the horizon, not under a roof cloud.
	const FVector CamLoc = PawnLoc + FVector(3800.f, 2600.f, 78.f);
	const FVector LookAt = PawnLoc + FVector(-11000.f, -22000.f, 18.f);
	FRotator CamRot = (LookAt - CamLoc).Rotation();
	CamRot.Pitch = FMath::Clamp(CamRot.Pitch, -4.2f, 0.15f);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	IntroCamera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), CamLoc, CamRot, Params);
	if (!IntroCamera)
	{
		UE_LOG(LogTemp, Warning, TEXT("raceGPS Cleveland: failed to spawn intro camera"));
		return;
	}
	if (UCameraComponent* Cam = IntroCamera->GetCameraComponent())
	{
		Cam->SetFieldOfView(66.f);
		Cam->bConstrainAspectRatio = false;
	}

	Pawn->ApplyClevelandShowcaseChaseFraming();
	Pawn->SetActorHiddenInGame(false);
	PC->SetViewTarget(IntroCamera);
	UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland: pawnYaw=%.1f loc=%s (chase is world-south locked, downtown -Y)"),
		Pawn->GetActorRotation().Yaw, *PawnLoc.ToCompactString());
	UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland: SetViewTarget intro=%s actualVT=%s pawnHidden=%d"),
		*IntroCamera->GetName(),
		PC->GetViewTarget() ? *PC->GetViewTarget()->GetName() : TEXT("null"),
		Pawn->IsHidden() ? 1 : 0);
	bIntroActive = true;
	IntroElapsed = 0.f;
	UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland: skyline intro camera at %s looking %s (downtown south / lake north)"),
		*CamLoc.ToCompactString(), *CamRot.ToCompactString());
}

void AClevelandShowcaseGameMode::FinishSkylineIntro()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	AChaosVehiclePawn* Pawn = GridManager ? GridManager->GetPlayerPawn() : nullptr;
	if (PC && Pawn)
	{
		Pawn->SetActorHiddenInGame(false);
		PC->SetViewTargetWithBlend(Pawn, IntroBlendSeconds, VTBlend_Cubic);
		UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland: blend ViewTarget -> pawn %s (pc=%s)"),
			*Pawn->GetName(), *PC->GetName());
	}
	bIntroActive = false;
	bChaseCapturePending = true;
	HeroCaptureElapsed = 0.f;
	UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland: blending intro camera to pawn chase (%.1fs); chase capture armed"), IntroBlendSeconds);
}

void AClevelandShowcaseGameMode::BeginPlay()
{
	Super::BeginPlay();
	ApplyPlaytestFlags();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	GridManager = World->SpawnActor<ARaceGridManager>(ARaceGridManager::StaticClass());
	if (!GridManager)
	{
		UE_LOG(LogTemp, Error, TEXT("raceGPS Cleveland: failed to spawn RaceGridManager"));
		return;
	}

	GridManager->BindSession(SessionManager);
	LoadCityPack();
	BindHud();

	// M5 environment dressing (lake / skyline / barriers). Additive; does not replace
	// the T10 baked HISM city (~120k buildings) already in Cleveland5_0KmWorld.
	EnvironmentActor = World->SpawnActor<AClevelandEnvironmentActor>(AClevelandEnvironmentActor::StaticClass());
	if (!EnvironmentActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("raceGPS Cleveland: failed to spawn ClevelandEnvironmentActor"));
	}

	LookDirector = World->SpawnActor<AClevelandLookDirector>(AClevelandLookDirector::StaticClass());
	if (LookDirector)
	{
		LookDirector->ApplyVisualMode(EClevelandVisualMode::MidnightRun);
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (GridManager)
	{
		GridManager->bAutoDrivePlayer = bAutoDrivePlayer;
	}
	GridManager->SpawnGrid(PC);
	SpawnShowcaseCheckpoints();

	if (AChaosVehiclePawn* PlayerPawn = GridManager->GetPlayerPawn())
	{
		PlayerPawn->ApplyClevelandShowcaseChaseFraming();
	}

	if (bSkipIntro)
	{
		if (PC && GridManager && GridManager->GetPlayerPawn())
		{
			PC->SetViewTarget(GridManager->GetPlayerPawn());
		}
		UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland: skip intro (playtest)"));
	}
	else
	{
		StartSkylineIntro(PC);
	}

	if (SessionManager)
	{
		SessionManager->StartSession(TEXT("cleveland_burke_gp_1997"), TEXT("hellcat"));
	}
	ConfigureAndStartRace();
}

void AClevelandShowcaseGameMode::BeginCountdownAndRace()
{
	if (SessionManager && SessionManager->GetCurrentState() == ERaceSessionState::Menu)
	{
		SessionManager->StartSession(TEXT("cleveland_burke_gp_1997"), TEXT("hellcat"));
	}
	ConfigureAndStartRace();
}

void AClevelandShowcaseGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (SessionManager)
	{
		SessionManager->TickSession(DeltaSeconds);
	}
	TickPlayerCheckpoints(DeltaSeconds);
	if (bPlaytestLap)
	{
		PlaytestLogElapsed += DeltaSeconds;
		AChaosVehiclePawn* Pawn = GridManager ? GridManager->GetPlayerPawn() : nullptr;
		const float Speed = Pawn ? Pawn->GetSpeedKmh() : 0.f;
		const bool bRacingNow = SessionManager && SessionManager->GetCurrentState() == ERaceSessionState::Racing;
		if (bRacingNow)
		{
			if (Speed >= 5.0f)
			{
				bSawPositiveSpeed = true;
				RacingZeroSpeedSeconds = 0.f;
				if (!bPlaytestStillCaptured && Speed > 8.0f)
				{
					CaptureStill(TEXT("playtest"));
				}
			}
			else
			{
				// crawl<5 or parked: quit after 90s
				RacingZeroSpeedSeconds += DeltaSeconds;
				if (!bDumpedDriveDiag && RacingZeroSpeedSeconds >= 8.f)
				{
					bDumpedDriveDiag = true;
					DumpDriveWhyNotMoving(TEXT("speed0_8s"));
				}
				if (!bStuckQuitIssued && RacingZeroSpeedSeconds >= 90.f)
				{
					bStuckQuitIssued = true;
					DumpDriveWhyNotMoving(TEXT("speed0_90s"));
					WritePlaytestReport(TEXT("FAILED_STUCK_SPEED0"));
					CaptureStill(TEXT("playtest"));
					UE_LOG(LogTemp, Error, TEXT("raceGPS Cleveland: playtest stuck speed=0 for 90s after Racing - quitting"));
					FGenericPlatformMisc::RequestExit(false);
				}
			}
		}
		if (PlaytestLogElapsed >= 5.f)
		{
			PlaytestLogElapsed = 0.f;
			const float S = (GridManager && GridManager->RacingLine && Pawn) ? GridManager->RacingLine->GetNearestS(Pawn->GetActorLocation()) : -1.f;
			UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland playtest: state=%s s=%.1f nextCP=%d/%d speed=%.1f possessed=%s finished=%d thr=%.2f hb=%d zeroT=%.1f"),
				SessionManager ? *UEnum::GetValueAsString(SessionManager->GetCurrentState()) : TEXT("none"),
				S, NextCheckpointIndex, TotalCheckpoints,
				Speed,
				(Pawn && Pawn->GetController()) ? *Pawn->GetController()->GetName() : TEXT("none"),
				GridManager && GridManager->HasPlayerFinished() ? 1 : 0,
				Pawn ? Pawn->GetThrottleCommand() : 0.f,
				Pawn && Pawn->IsHandbrakeOn() ? 1 : 0,
				RacingZeroSpeedSeconds);
		}
	}
	if (bIntroActive)
	{
		IntroElapsed += DeltaSeconds;
		// V11: intro HighResShot while 3-car grid framing still owns the view.
		if (!bIntroCaptureDone && IntroElapsed >= FMath::Max(1.2f, IntroHoldSeconds - 0.35f))
		{
			CaptureStill(TEXT("intro"));
		}
		if (IntroElapsed >= IntroHoldSeconds)
		{
			FinishSkylineIntro();
		}
	}
	if (bChaseCapturePending && !bChaseCaptureDone)
	{
		HeroCaptureElapsed += DeltaSeconds;
		const float ReadyAt = IntroBlendSeconds + HeroCaptureDelaySeconds;
		if (HeroCaptureElapsed >= ReadyAt)
		{
			CaptureStill(TEXT("chase"));
		}
	}
	if (!bShowcaseEnded && GridManager && (GridManager->HasPlayerFinished() || GridManager->HaveAllFinished()))
	{
		EndRace();
	}
}

void AClevelandShowcaseGameMode::CaptureStill(const TCHAR* Phase)
{
	const bool bIntro = FCString::Stricmp(Phase, TEXT("intro")) == 0;
	const bool bPlaytest = FCString::Stricmp(Phase, TEXT("playtest")) == 0;
	if (bPlaytest)
	{
		if (bPlaytestStillCaptured) { return; }
		bPlaytestStillCaptured = true;
	}
	else if (bIntro)
	{
		if (bIntroCaptureDone) { return; }
		bIntroCaptureDone = true;
	}
	else
	{
		if (bChaseCaptureDone) { return; }
		bChaseCaptureDone = true;
		bChaseCapturePending = false;
	}

	UWorld* World = GetWorld();
	if (!World || !GEngine)
	{
		return;
	}
	if (!bIntro)
	{
		if (AChaosVehiclePawn* Pawn = GridManager ? GridManager->GetPlayerPawn() : nullptr)
		{
			Pawn->ApplyClevelandShowcaseChaseFraming();
		}
	}
	const FString ShotDir = FPaths::ScreenShotDir();
	IFileManager::Get().MakeDirectory(*ShotDir, true);
	const FString PhaseStr(Phase);
	UE_LOG(LogTemp, Warning, TEXT("raceGPS Cleveland: V15 %s capture HighResShot 1280x800 -> %s"), *PhaseStr, *ShotDir);
	if (UGameViewportClient* VC = World->GetGameViewport())
	{
		VC->Exec(World, TEXT("HighResShot 1280x800"), *GLog);
		// Also PrintWindow for a second chase still when available.
		if (!bIntro)
		{
			VC->Exec(World, TEXT("Shot"), *GLog);
		}
	}
	else if (GEngine)
	{
		GEngine->Exec(World, TEXT("HighResShot 1280x800"));
	}

	const FString DestName = bPlaytest
		? FString(TEXT("cleveland_playtest_lap.png"))
		: (bIntro ? FString(TEXT("cleveland_v15_hero.png")) : FString(TEXT("cleveland_v15_chase.png")));
	FTimerHandle Ignored;
	World->GetTimerManager().SetTimer(Ignored, FTimerDelegate::CreateLambda([ShotDir, DestName, PhaseStr]()
	{
		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files, *(ShotDir / TEXT("*.png")), true, false);
		FString Best;
		FDateTime BestTime = FDateTime::MinValue();
		for (const FString& F : Files)
		{
			const FString Full = ShotDir / F;
			const FDateTime T = IFileManager::Get().GetTimeStamp(*Full);
			if (T > BestTime)
			{
				BestTime = T;
				Best = Full;
			}
		}
		if (!Best.IsEmpty())
		{
			const FString TempDir = FPlatformProcess::UserTempDir();
			const FString Dest = TempDir / DestName;
			IFileManager::Get().Copy(*Dest, *Best, true, true);
			const FString ProjectTemp = FPaths::Combine(FPaths::ProjectDir(), TEXT("Temp"));
			IFileManager::Get().MakeDirectory(*ProjectTemp, true);
			IFileManager::Get().Copy(*(ProjectTemp / DestName), *Best, true, true);
			UE_LOG(LogTemp, Warning, TEXT("raceGPS Cleveland: staged V15 %s still %s -> %s and %s"), *PhaseStr, *Best, *Dest, *(ProjectTemp / DestName));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("raceGPS Cleveland: V15 %s PNG not found in %s"), *PhaseStr, *ShotDir);
		}
	}), 2.5f, false);
}

void AClevelandShowcaseGameMode::CaptureHeroStill()
{
	CaptureStill(TEXT("intro"));
}

void AClevelandShowcaseGameMode::EndRace()
{
	if (bShowcaseEnded)
	{
		return;
	}
	bShowcaseEnded = true;
	if (SessionManager
		&& SessionManager->GetCurrentState() != ERaceSessionState::Finished
		&& (SessionManager->GetCurrentState() == ERaceSessionState::Racing
			|| SessionManager->GetCurrentState() == ERaceSessionState::Countdown))
	{
		SessionManager->EndRace();
	}
	UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland: EndRace place=%d/3 nextCP=%d"),
		GridManager ? GridManager->GetPlayerPlace() : 0, NextCheckpointIndex);
	WritePlaytestReport(bSawPositiveSpeed ? TEXT("finished") : TEXT("finished_speed0"));
	if (bPlaytestLap)
	{
		CaptureStill(TEXT("playtest"));
	}
}

void AClevelandShowcaseGameMode::RestartShowcase()
{
	bShowcaseEnded = false;
	NextCheckpointIndex = 1;
	bHaveCheckpointPrevS = false;
	PlaytestCheckpointLines.Reset();
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (GridManager)
	{
		GridManager->bAutoDrivePlayer = bAutoDrivePlayer;
		GridManager->RespawnGridAndRestart(PC);
		if (AChaosVehiclePawn* PlayerPawn = GridManager->GetPlayerPawn())
		{
			PlayerPawn->ApplyClevelandShowcaseChaseFraming();
		}
		SpawnShowcaseCheckpoints();
		if (!bSkipIntro) { StartSkylineIntro(PC); }
		else if (PC && GridManager->GetPlayerPawn()) { PC->SetViewTarget(GridManager->GetPlayerPawn()); }
		ConfigureAndStartRace();
	}
	else if (SessionManager)
	{
		SessionManager->StartSession(TEXT("cleveland_burke_gp_1997"), TEXT("hellcat"));
		ConfigureAndStartRace();
	}
}

void AClevelandShowcaseGameMode::LoadCheckpointCourse()
{
	TotalCheckpoints = LoadCheckpointCount();
	CheckpointSCm.Reset();
	CheckpointNames.Reset();
	const FString Path = ResolveCityPackPath(TEXT("checkpoints.json"));
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *Path))
	{
		return;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return;
	}
	static const TCHAR* Keys[] = { TEXT("checkpoints"), TEXT("gates"), TEXT("points"), TEXT("items") };
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	for (const TCHAR* Key : Keys)
	{
		if (Root->TryGetArrayField(Key, Arr) && Arr)
		{
			break;
		}
	}
	if (!Arr)
	{
		return;
	}
	float Acc = 0.f;
	for (int32 i = 0; i < Arr->Num(); ++i)
	{
		const TSharedPtr<FJsonObject> Obj = (*Arr)[i]->AsObject();
		float S = Acc;
		FString Name = FString::Printf(TEXT("CP%d"), i + 1);
		if (Obj.IsValid())
		{
			if (Obj->HasTypedField<EJson::Number>(TEXT("s_cm"))) { S = static_cast<float>(Obj->GetNumberField(TEXT("s_cm"))); }
			else if (Obj->HasTypedField<EJson::Number>(TEXT("s"))) { S = static_cast<float>(Obj->GetNumberField(TEXT("s"))); }
			else if (Obj->HasTypedField<EJson::Number>(TEXT("distance"))) { S = static_cast<float>(Obj->GetNumberField(TEXT("distance"))); }
			FString N;
			if (Obj->TryGetStringField(TEXT("name"), N) && !N.IsEmpty()) { Name = N; }
		}
		CheckpointSCm.Add(S);
		CheckpointNames.Add(Name);
		Acc = S + 25000.f;
	}
	float MaxS = 0.f;
	for (float S : CheckpointSCm) { MaxS = FMath::Max(MaxS, S); }
	if (MaxS > KINDA_SMALL_NUMBER && MaxS < 50000.f)
	{
		for (float& S : CheckpointSCm) { S *= 100.f; }
	}
	if (CheckpointSCm.Num() > 0)
	{
		TotalCheckpoints = CheckpointSCm.Num();
	}
	UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland: LoadCheckpointCourse count=%d firstS=%.1f lastS=%.1f"),
		TotalCheckpoints, CheckpointSCm.Num() ? CheckpointSCm[0] : 0.f, CheckpointSCm.Num() ? CheckpointSCm.Last() : 0.f);
}

void AClevelandShowcaseGameMode::SpawnShowcaseCheckpoints()
{
	UWorld* World = GetWorld();
	URacingLineComponent* Line = GridManager ? GridManager->RacingLine.Get() : nullptr;
	for (ACheckpointGate* Old : ShowcaseGates)
	{
		if (Old) { Old->Destroy(); }
	}
	ShowcaseGates.Reset();
	if (!World || !Line || !Line->IsValidLine() || CheckpointSCm.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("raceGPS Cleveland: skip gate spawn (world/line/cp missing)"));
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 i = 0; i < CheckpointSCm.Num(); ++i)
	{
		const FTransform Pose = Line->GetPoseAtS(CheckpointSCm[i]);
		FVector Loc = Pose.GetLocation();
		Loc.Z += 80.f;
		ACheckpointGate* Gate = World->SpawnActor<ACheckpointGate>(ACheckpointGate::StaticClass(), Loc, Pose.Rotator(), Params);
		if (!Gate) { continue; }
		Gate->CheckpointIndex = i;
		Gate->OnCheckpointReached.AddDynamic(this, &AClevelandShowcaseGameMode::OnShowcaseCheckpointReached);
		if (i == NextCheckpointIndex) { Gate->ActivateGate(); }
		else { Gate->DeactivateGate(); }
		ShowcaseGates.Add(Gate);
	}
	UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland: spawned %d checkpoint gates (next=%d)"), ShowcaseGates.Num(), NextCheckpointIndex);
}

void AClevelandShowcaseGameMode::ConfigureAndStartRace()
{
	if (!SessionManager)
	{
		UE_LOG(LogTemp, Error, TEXT("raceGPS Cleveland: no SessionManager"));
		return;
	}
	if (SessionManager->GetCurrentState() == ERaceSessionState::Menu
		|| SessionManager->GetCurrentState() == ERaceSessionState::Finished)
	{
		if (SessionManager->CurrentTrackId.IsEmpty())
		{
			SessionManager->StartSession(TEXT("cleveland_burke_gp_1997"), TEXT("hellcat"));
		}
		SessionManager->StartRace();
	}
	SessionManager->TotalCheckpoints = TotalCheckpoints;
	SessionManager->CurrentCheckpoint = 1;
	NextCheckpointIndex = 1;
	bHaveCheckpointPrevS = false;
	UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland: race armed state=%s countdown=%.1f checkpoints=%d autodive=%d"),
		*UEnum::GetValueAsString(SessionManager->GetCurrentState()),
		SessionManager->CountdownTimer, TotalCheckpoints, bAutoDrivePlayer ? 1 : 0);
}

void AClevelandShowcaseGameMode::ApplyPlaytestFlags()
{
	const TCHAR* Cmd = FCommandLine::Get();
	bPlaytestLap = FParse::Param(Cmd, TEXT("ClevelandAutoLap")) || FParse::Param(Cmd, TEXT("ClevelandPlaytest"));
	bSkipIntro = bPlaytestLap || FParse::Param(Cmd, TEXT("ClevelandSkipIntro"));
	bAutoDrivePlayer = bPlaytestLap || FParse::Param(Cmd, TEXT("ClevelandAutoDrive"));
	if (bSkipIntro)
	{
		IntroHoldSeconds = 0.35f;
		IntroBlendSeconds = 0.20f;
		HeroCaptureDelaySeconds = 0.10f;
	}
	UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland: flags playtest=%d autodive=%d skipIntro=%d"),
		bPlaytestLap ? 1 : 0, bAutoDrivePlayer ? 1 : 0, bSkipIntro ? 1 : 0);
}

float AClevelandShowcaseGameMode::CanonicalizeSplineS(float S, float Length) const
{
	if (Length <= KINDA_SMALL_NUMBER)
	{
		return S;
	}
	// Skip-start-line still in effect (NextCheckpointIndex starts at 1). Until later CPs
	// are actually crossed, a start==finish nearest-S of ~TrackLength is the GRID, not a lap.
	const bool bWaitingOnFinish = (NextCheckpointIndex >= CheckpointSCm.Num() - 1) && NextCheckpointIndex > 1;
	if (!bWaitingOnFinish && S > 0.85f * Length)
	{
		return 0.f;
	}
	if (S >= Length)
	{
		return 0.f;
	}
	return S;
}

void AClevelandShowcaseGameMode::TickPlayerCheckpoints(float DeltaSeconds)
{
	if (bShowcaseEnded || !GridManager || !GridManager->GetPlayerPawn() || !GridManager->RacingLine || !GridManager->RacingLine->IsValidLine())
	{
		return;
	}
	if (!SessionManager || SessionManager->GetCurrentState() != ERaceSessionState::Racing)
	{
		return;
	}
	if (NextCheckpointIndex >= CheckpointSCm.Num())
	{
		return;
	}
	const float Length = GridManager->RacingLine->TrackLength;
	const float RawS = GridManager->RacingLine->GetNearestS(GridManager->GetPlayerPawn()->GetActorLocation());
	const float S = CanonicalizeSplineS(RawS, Length);
	if (!bHaveCheckpointPrevS)
	{
		PlayerCheckpointPrevS = S;
		bHaveCheckpointPrevS = true;
		return;
	}

	const float Prev = PlayerCheckpointPrevS;
	// ~80 m/s (~288 km/h) plus a 20m floor so a single tick cannot wrap the 3.2km loop.
	const float MaxDs = FMath::Max(2000.f, 8000.f * FMath::Max(DeltaSeconds, 0.001f));
	const bool bNearFinish = Prev > 0.80f * Length;
	const bool bNearStart = S < 0.20f * Length;
	const bool bWaitingOnFinish = (NextCheckpointIndex >= CheckpointSCm.Num() - 1) && NextCheckpointIndex > 1;
	const bool bLegitimateWrap = bNearFinish && bNearStart && bWaitingOnFinish;

	float ForwardDs = 0.f;
	if (S >= Prev)
	{
		ForwardDs = S - Prev;
	}
	else
	{
		const float Backward = Prev - S;
		const float WrapForward = FMath::Max(0.f, Length - Prev) + S;
		// GetNearestS noise / brief reverse must NOT count as a full-lap wrap (dS~TrackLength).
		if (Backward <= MaxDs && !bLegitimateWrap)
		{
			ForwardDs = 0.f;
		}
		else
		{
			ForwardDs = WrapForward;
		}
	}

	if (ForwardDs > MaxDs && !bLegitimateWrap)
	{
		UE_LOG(LogTemp, Warning, TEXT("raceGPS Cleveland: reject S jump prev=%.1f raw=%.1f canon=%.1f dS=%.1f max=%.1f nextCP=%d"),
			Prev, RawS, S, ForwardDs, MaxDs, NextCheckpointIndex);
		PlayerCheckpointPrevS = S;
		return;
	}

	// Sequential: at most one CP per tick, and never CP i until i-1 fired on a prior tick.
	if (NextCheckpointIndex < CheckpointSCm.Num())
	{
		const float GateS = CheckpointSCm[NextCheckpointIndex];
		bool bCrossed = false;
		if (bLegitimateWrap)
		{
			bCrossed = true;
		}
		else
		{
			const bool bLast = (NextCheckpointIndex == CheckpointSCm.Num() - 1);
			const float Slack = bLast ? 400.f : 0.f;
			// One CP per tick. S past gate counts even if Prev was advanced during reject-S noise
			// (otherwise nextCP stays 1 after mid-track with GateS already behind).
			bCrossed = (S + Slack >= GateS);
		}
		if (bCrossed)
		{
			OnShowcaseCheckpointReached(NextCheckpointIndex);
		}
	}
	PlayerCheckpointPrevS = S;
}

void AClevelandShowcaseGameMode::DumpDriveWhyNotMoving(const TCHAR* Reason)
{
	AChaosVehiclePawn* Pawn = GridManager ? GridManager->GetPlayerPawn() : nullptr;
	UE_LOG(LogTemp, Error, TEXT("raceGPS Cleveland: cars not moving (%s) nextCP=%d/%d autodive=%d"),
		Reason ? Reason : TEXT("?"), NextCheckpointIndex, TotalCheckpoints, bAutoDrivePlayer ? 1 : 0);
	if (GridManager)
	{
		GridManager->DumpGridDriveState(Reason ? Reason : TEXT("stuck"));
	}
	else if (Pawn)
	{
		Pawn->DumpDriveState(Reason);
	}
}

void AClevelandShowcaseGameMode::WritePlaytestReport(const TCHAR* Outcome)
{
	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("outcome=%s"), Outcome ? Outcome : TEXT("?")));
	Lines.Add(FString::Printf(TEXT("state=%s"), SessionManager ? *UEnum::GetValueAsString(SessionManager->GetCurrentState()) : TEXT("none")));
	Lines.Add(FString::Printf(TEXT("place=%d"), GridManager ? GridManager->GetPlayerPlace() : 0));
	AChaosVehiclePawn* Pawn = GridManager ? GridManager->GetPlayerPawn() : nullptr;
	Lines.Add(FString::Printf(TEXT("pawnClass=%s"), Pawn ? *GetNameSafe(Pawn->GetClass()) : TEXT("none")));
	Lines.Add(FString::Printf(TEXT("nextCP=%d total=%d playerFinished=%d"),
		NextCheckpointIndex, TotalCheckpoints, GridManager && GridManager->HasPlayerFinished() ? 1 : 0));
	Lines.Add(FString::Printf(TEXT("elapsed=%.2f speed=%.2f sawSpeed=%d zeroT=%.1f"),
		SessionManager ? SessionManager->ElapsedTime : 0.f,
		Pawn ? Pawn->GetSpeedKmh() : 0.f,
		bSawPositiveSpeed ? 1 : 0,
		RacingZeroSpeedSeconds));
	Lines.Add(FString::Printf(TEXT("throttle=%.2f brake=%.2f hb=%d override=%d"),
		Pawn ? Pawn->GetThrottleCommand() : 0.f,
		Pawn ? Pawn->GetBrakeCommand() : 0.f,
		Pawn && Pawn->IsHandbrakeOn() ? 1 : 0,
		Pawn && Pawn->IsDriveOverrideActive() ? 1 : 0));
	Lines.Add(TEXT("checkpoints:"));
	Lines.Append(PlaytestCheckpointLines);
	const FString Dest = FPaths::Combine(FPaths::ProjectDir(), TEXT("Temp/cleveland_playtest_lap.txt"));
	FFileHelper::SaveStringArrayToFile(Lines, *Dest);
	UE_LOG(LogTemp, Warning, TEXT("raceGPS Cleveland: playtest report %s -> %s"), Outcome ? Outcome : TEXT("?"), *Dest);
}

void AClevelandShowcaseGameMode::OnShowcaseCheckpointReached(int32 CheckpointIndex)
{
	if (CheckpointIndex != NextCheckpointIndex)
	{
		return;
	}
	const float Elapsed = SessionManager ? SessionManager->ElapsedTime : 0.f;
	const FString Name = CheckpointNames.IsValidIndex(CheckpointIndex) ? CheckpointNames[CheckpointIndex] : TEXT("?");
	UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland: CHECKPOINT %d/%d %s t=%.2f"),
		CheckpointIndex, TotalCheckpoints, *Name, Elapsed);
	PlaytestCheckpointLines.Add(FString::Printf(TEXT("%d	%s	%.2f"), CheckpointIndex, *Name, Elapsed));
	if (SessionManager && SessionManager->GetCurrentState() == ERaceSessionState::Racing)
	{
		SessionManager->OnCheckpointReached(CheckpointIndex);
	}
	NextCheckpointIndex = CheckpointIndex + 1;
	for (ACheckpointGate* Gate : ShowcaseGates)
	{
		if (!Gate) { continue; }
		if (Gate->CheckpointIndex == NextCheckpointIndex) { Gate->ActivateGate(); }
		else { Gate->DeactivateGate(); }
	}
	if (CheckpointIndex >= TotalCheckpoints - 1 && GridManager)
	{
		GridManager->NotifyVehicleLapComplete(0, Elapsed);
	}
}

void AClevelandShowcaseGameMode::ClevelandAutoLap()
{
	bAutoDrivePlayer = true;
	bPlaytestLap = true;
	bSkipIntro = true;
	if (GridManager) { GridManager->bAutoDrivePlayer = true; }
	ConfigureAndStartRace();
	UE_LOG(LogTemp, Warning, TEXT("raceGPS Cleveland: ClevelandAutoLap armed"));
}

void AClevelandShowcaseGameMode::ClevelandSkipCountdown()
{
	if (SessionManager)
	{
		if (SessionManager->GetCurrentState() == ERaceSessionState::Menu
			|| SessionManager->GetCurrentState() == ERaceSessionState::Finished)
		{
			ConfigureAndStartRace();
		}
		SessionManager->CountdownTimer = 0.01f;
	}
	UE_LOG(LogTemp, Warning, TEXT("raceGPS Cleveland: skip countdown"));
}

void AClevelandShowcaseGameMode::ClevelandForceFinish()
{
	EndRace();
	WritePlaytestReport(TEXT("force_finish"));
}
