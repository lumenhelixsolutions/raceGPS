#include "MainMenuWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/ComboBoxString.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "raceGPSGameInstance.h"
#include "VehicleTuningData.h"
#include "Version.h"

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

    // Populate route selector
    if (RouteSelector)
    {
        RouteSelector->ClearOptions();
        for (const FString& Route : AvailableRoutes)
        {
            RouteSelector->AddOption(Route);
        }

        UraceGPSGameInstance* GI = Cast<UraceGPSGameInstance>(GetGameInstance());
        FString DefaultRoute = (GI && !GI->LastSelectedRoute.IsEmpty())
            ? GI->LastSelectedRoute
            : (AvailableRoutes.Num() > 0 ? AvailableRoutes[0] : TEXT(""));
        RouteSelector->SetSelectedOption(DefaultRoute);
    }

    // Populate vehicle selector
    if (VehicleSelector)
    {
        VehicleSelector->ClearOptions();
        for (UVehicleTuningData* Vehicle : AvailableVehicles)
        {
            if (Vehicle)
            {
                VehicleSelector->AddOption(Vehicle->DisplayName);
            }
        }

        UraceGPSGameInstance* GI = Cast<UraceGPSGameInstance>(GetGameInstance());
        FString DefaultVehicle = TEXT("");
        if (GI && GI->LastSelectedVehicleTuning)
        {
            DefaultVehicle = GI->LastSelectedVehicleTuning->DisplayName;
        }
        else if (AvailableVehicles.Num() > 0)
        {
            DefaultVehicle = AvailableVehicles[0]->DisplayName;
        }
        VehicleSelector->SetSelectedOption(DefaultVehicle);
        UpdateVehicleInfo();

        VehicleSelector->OnSelectionChanged.AddDynamic(this, &UMainMenuWidget::OnVehicleSelectionChanged);
    }

    if (VersionText)
    {
        FString GameVer = FString(RACEGPS_VERSION_STRING);
        FString CityVer = TEXT("unknown");
        FString CityName = TEXT("Akron");

        FString ManifestPath = FPaths::ProjectDir() / TEXT("citypacks/akron-oh-beta-001/akron_semantic_manifest.json");
        FString Content;
        if (FFileHelper::LoadFileToString(Content, *ManifestPath))
        {
            TSharedPtr<FJsonObject> Root;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
            if (FJsonSerializer::Deserialize(Reader, Root))
            {
                Root->TryGetStringField(TEXT("version"), CityVer);
                Root->TryGetStringField(TEXT("display_name"), CityName);
            }
        }

        FString VersionStr = FString::Printf(TEXT("Game %s | %s %s"), *GameVer, *CityName, *CityVer);
        VersionText->SetText(FText::FromString(VersionStr));
    }
}

void UMainMenuWidget::OnPlayClicked()
{
    UraceGPSGameInstance* GI = Cast<UraceGPSGameInstance>(GetGameInstance());
    if (GI)
    {
        if (RouteSelector)
        {
            GI->LastSelectedRoute = RouteSelector->GetSelectedOption();
        }

        UVehicleTuningData* SelectedVehicle = GetSelectedVehicle();
        if (SelectedVehicle)
        {
            GI->LastSelectedVehicleTuning = SelectedVehicle;
        }

        GI->SaveSettings();
    }

    UGameplayStatics::OpenLevel(GetWorld(), FName(*GameLevelName));
}

void UMainMenuWidget::OnSettingsClicked()
{
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

UVehicleTuningData* UMainMenuWidget::GetSelectedVehicle() const
{
    if (!VehicleSelector)
        return nullptr;

    FString SelectedName = VehicleSelector->GetSelectedOption();
    for (UVehicleTuningData* Vehicle : AvailableVehicles)
    {
        if (Vehicle && Vehicle->DisplayName == SelectedName)
        {
            return Vehicle;
        }
    }
    return nullptr;
}

void UMainMenuWidget::UpdateVehicleInfo()
{
    UVehicleTuningData* Selected = GetSelectedVehicle();
    if (!Selected || !VehicleInfoText)
        return;

    FString Info = FString::Printf(
        TEXT("%s\n%s\nMass: %.0f kg | Top Speed: %.0f km/h | Gears: %d"),
        *Selected->DisplayName,
        *Selected->Description,
        Selected->VehicleMass,
        Selected->MaxEngineRPM * 0.04f, // Rough km/h estimate
        Selected->Transmission.GearRatios.Num()
    );
    VehicleInfoText->SetText(FText::FromString(Info));
}

void UMainMenuWidget::OnVehicleSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    UpdateVehicleInfo();
}
