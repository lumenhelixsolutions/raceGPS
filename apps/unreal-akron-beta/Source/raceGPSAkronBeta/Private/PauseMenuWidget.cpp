#include "PauseMenuWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "CruiseSprintGameMode.h"

void UPauseMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (ResumeButton)
    {
        ResumeButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnResumeClicked);
    }
    if (RestartButton)
    {
        RestartButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnRestartClicked);
    }
    if (SettingsButton)
    {
        SettingsButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnSettingsClicked);
    }
    if (QuitToMenuButton)
    {
        QuitToMenuButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnQuitToMenuClicked);
    }

    // Update stats
    if (RaceTimeText)
    {
        ACruiseSprintGameMode* GM = Cast<ACruiseSprintGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
        if (GM)
        {
            int32 Minutes = FMath::FloorToInt(GM->GetElapsedTime() / 60.0f);
            int32 Secs = FMath::FloorToInt(GM->GetElapsedTime()) % 60;
            int32 Ms = FMath::FloorToInt((GM->GetElapsedTime() - FMath::FloorToInt(GM->GetElapsedTime())) * 1000.0f);
            RaceTimeText->SetText(FText::FromString(FString::Printf(TEXT("%02d:%02d.%03d"), Minutes, Secs, Ms)));
        }
    }

    if (CheckpointText)
    {
        ACruiseSprintGameMode* GM = Cast<ACruiseSprintGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
        if (GM)
        {
            CheckpointText->SetText(FText::FromString(FString::Printf(TEXT("Checkpoint %d / %d"),
                GM->GetCurrentCheckpoint(), GM->GetTotalCheckpoints())));
        }
    }
}

void UPauseMenuWidget::OnResumeClicked()
{
    ACruiseSprintGameMode* GM = Cast<ACruiseSprintGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
    if (GM)
    {
        GM->ResumeRace();
    }
    RemoveFromParent();
    GetOwningPlayer()->SetInputMode(FInputModeGameOnly());
    GetOwningPlayer()->bShowMouseCursor = false;
}

void UPauseMenuWidget::OnRestartClicked()
{
    ACruiseSprintGameMode* GM = Cast<ACruiseSprintGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
    if (GM)
    {
        GM->RestartRace();
    }
    RemoveFromParent();
    GetOwningPlayer()->SetInputMode(FInputModeGameOnly());
    GetOwningPlayer()->bShowMouseCursor = false;
}

void UPauseMenuWidget::OnSettingsClicked()
{
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Settings from pause menu"));
}

void UPauseMenuWidget::OnQuitToMenuClicked()
{
    UGameplayStatics::SetGamePaused(GetWorld(), false);
    UGameplayStatics::OpenLevel(GetWorld(), FName(*MenuLevelName));
}
