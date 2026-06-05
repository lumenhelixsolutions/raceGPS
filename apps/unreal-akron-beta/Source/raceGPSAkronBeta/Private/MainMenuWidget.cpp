#include "MainMenuWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/ComboBoxString.h"
#include "Kismet/GameplayStatics.h"
#include "raceGPSGameInstance.h"

void UMainMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (PlayButton)
    {
        PlayButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OnPlayClicked);
    }
    if (SettingsButton)
    {
        SettingsButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OnSettingsClicked);
    }
    if (QuitButton)
    {
        QuitButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OnQuitClicked);
    }

    if (RouteSelector)
    {
        RouteSelector->ClearOptions();
        for (const FString& Route : AvailableRoutes)
        {
            RouteSelector->AddOption(Route);
        }
        RouteSelector->SetSelectedOption(AvailableRoutes.Num() > 0 ? AvailableRoutes[0] : TEXT(""));
    }

    if (VersionText)
    {
        VersionText->SetText(FText::FromString(TEXT("v0.1.0 Beta")));
    }
}

void UMainMenuWidget::OnPlayClicked()
{
    UraceGPSGameInstance* GI = Cast<UraceGPSGameInstance>(GetGameInstance());
    if (GI && RouteSelector)
    {
        GI->LastSelectedRoute = RouteSelector->GetSelectedOption();
        GI->SaveSettings();
    }

    UGameplayStatics::OpenLevel(GetWorld(), FName(*GameLevelName));
}

void UMainMenuWidget::OnSettingsClicked()
{
    // TODO: Open settings sub-menu
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Settings clicked"));
}

void UMainMenuWidget::OnQuitClicked()
{
    UKismetSystemLibrary::QuitGame(GetWorld(), GetOwningPlayer(), EQuitPreference::Quit, true);
}

void UMainMenuWidget::OnSelectRouteClicked(const FString& RouteId)
{
    UraceGPSGameInstance* GI = Cast<UraceGPSGameInstance>(GetGameInstance());
    if (GI)
    {
        GI->LastSelectedRoute = RouteId;
    }
}
