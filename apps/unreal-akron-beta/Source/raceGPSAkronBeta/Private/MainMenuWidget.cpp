#include "MainMenuWidget.h"
#include "AkronXodrImporter.h"
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
#include "LANBrowserWidget.h"

void UMainMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (PlayButton)
    {
        PlayButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OnPlayClicked);
        PlayButton->SetToolTipText(FText::FromString(BuildLaunchButtonText()));
    }
    if (SettingsButton)
    {
        SettingsButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OnSettingsClicked);
    }
    if (QuitButton)
    {
        QuitButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OnQuitClicked);
    }
    if (HostLANButton)
    {
        HostLANButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OnHostLANClicked);
    }
    if (JoinLANButton)
    {
        JoinLANButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OnJoinLANClicked);
    }

    UraceGPSGameInstance* GI = Cast<UraceGPSGameInstance>(GetGameInstance());

    if (TitleText)
    {
        TitleText->SetText(FText::FromString(TEXT("raceGPS — Midnight Club on real roads")));
    }

    // Populate route selector
    if (RouteSelector)
    {
        RouteSelector->ClearOptions();
        for (const FString& Route : AvailableRoutes)
        {
            RouteSelector->AddOption(Route);
        }

        FString DefaultRoute = (GI && !GI->LastSelectedRoute.IsEmpty())
            ? GI->LastSelectedRoute
            : (AvailableRoutes.Num() > 0 ? AvailableRoutes[0] : TEXT(""));
        RouteSelector->SetSelectedOption(DefaultRoute);
        RouteSelector->OnSelectionChanged.AddDynamic(this, &UMainMenuWidget::OnRouteSelectionChanged);
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

        FString DefaultVehicle = TEXT("");
        if (GI && GI->LastSelectedVehicleTuning)
        {
            DefaultVehicle = GI->LastSelectedVehicleTuning->DisplayName;
        }
        else if (GI && !GI->LastSelectedVehicle.IsEmpty())
        {
            DefaultVehicle = GI->LastSelectedVehicle;
        }
        else if (AvailableVehicles.Num() > 0)
        {
            DefaultVehicle = AvailableVehicles[0]->DisplayName;
        }
        VehicleSelector->SetSelectedOption(DefaultVehicle);
        UpdateVehicleInfo();

        VehicleSelector->OnSelectionChanged.AddDynamic(this, &UMainMenuWidget::OnVehicleSelectionChanged);
    }

    if (HandlingSelector)
    {
        HandlingSelector->ClearOptions();
        for (const FString& HandlingMode : AvailableHandlingModes)
        {
            HandlingSelector->AddOption(HandlingMode);
        }

        FString DefaultHandling = (GI && !GI->LastSelectedHandlingMode.IsEmpty())
            ? GI->LastSelectedHandlingMode
            : (AvailableHandlingModes.Num() > 0 ? AvailableHandlingModes[0] : TEXT("Arcade"));
        HandlingSelector->SetSelectedOption(DefaultHandling);
        HandlingSelector->OnSelectionChanged.AddDynamic(this, &UMainMenuWidget::OnHandlingSelectionChanged);
    }

    if (VersionText)
    {
        VersionText->SetText(FText::FromString(BuildVersionLine()));
    }

    UpdateVehicleInfo();
}

void UMainMenuWidget::OnPlayClicked()
{
    UraceGPSGameInstance* GI = Cast<UraceGPSGameInstance>(GetGameInstance());
    if (!RouteSelector)
    {
        UE_LOG(LogTemp, Warning, TEXT("[raceGPS] Cannot start play: route selector widget is missing"));
        return;
    }

    const FString SelectedRoute = RouteSelector->GetSelectedOption();
    if (RouteSelector->GetSelectedOption().IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[raceGPS] Cannot start play: no route selected"));
        return;
    }

    UVehicleTuningData* SelectedVehicle = GetSelectedVehicle();
    if (!SelectedVehicle)
    {
        UE_LOG(LogTemp, Warning, TEXT("[raceGPS] Cannot start play: no vehicle preset selected"));
        return;
    }

    if (!HandlingSelector)
    {
        UE_LOG(LogTemp, Warning, TEXT("[raceGPS] Cannot start play: handling selector widget is missing"));
        return;
    }

    const FString SelectedHandlingMode = HandlingSelector->GetSelectedOption();
    if (SelectedHandlingMode.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[raceGPS] Cannot start play: no handling mode selected"));
        return;
    }

    if (GI)
    {
        GI->LastSelectedRoute = SelectedRoute;
        GI->LastSelectedVehicleTuning = SelectedVehicle;
        GI->LastSelectedVehicle = SelectedVehicle->DisplayName;
        GI->LastSelectedHandlingMode = GetSelectedHandlingMode();

        GI->SaveSettings();
    }

    FString LevelToOpen = GameLevelName;
    FRaceGPSCityLayout Layout;
    if (UAkronXodrImporter::ResolveCityLayout(Layout) && !Layout.LevelName.IsEmpty())
    {
        LevelToOpen = Layout.LevelName;
    }
    UGameplayStatics::OpenLevel(GetWorld(), FName(*LevelToOpen));
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

void UMainMenuWidget::OnHostLANClicked()
{
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Host LAN clicked"));
    // Open LAN browser with host tab active
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (PC && LANBrowserClass)
    {
        ULANBrowserWidget* Browser = CreateWidget<ULANBrowserWidget>(PC, LANBrowserClass);
        if (Browser)
        {
            Browser->AddToViewport(100);
            // Trigger host immediately
            Browser->OnHostClicked();
        }
    }
}

void UMainMenuWidget::OnJoinLANClicked()
{
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Join LAN clicked"));
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (PC && LANBrowserClass)
    {
        ULANBrowserWidget* Browser = CreateWidget<ULANBrowserWidget>(PC, LANBrowserClass);
        if (Browser)
        {
            Browser->AddToViewport(100);
            Browser->OnRefreshClicked();
        }
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

FString UMainMenuWidget::GetSelectedHandlingMode() const
{
    if (!HandlingSelector)
        return TEXT("Arcade");

    const FString SelectedMode = HandlingSelector->GetSelectedOption();
    return SelectedMode.IsEmpty() ? TEXT("Arcade") : SelectedMode;
}

FString UMainMenuWidget::BuildVehicleClassLabel(const UVehicleTuningData* Vehicle) const
{
    if (!Vehicle)
    {
        return TEXT("Street Machine");
    }

    switch (Vehicle->VehicleClass)
    {
    case EVehicleClass::Sedan: return TEXT("Sedan");
    case EVehicleClass::Sports: return TEXT("Sports");
    case EVehicleClass::Truck: return TEXT("Truck");
    case EVehicleClass::Hatchback: return TEXT("Hatchback");
    case EVehicleClass::SUV: return TEXT("SUV");
    default: return TEXT("Street Machine");
    }
}

FString UMainMenuWidget::BuildDriveSummaryText() const
{
    const UVehicleTuningData* Vehicle = GetSelectedVehicle();
    const FString VehicleName = Vehicle ? Vehicle->DisplayName : TEXT("Sedan");
    const FString VehicleClass = BuildVehicleClassLabel(Vehicle);
    const FString HandlingMode = GetSelectedHandlingMode();
    const FString RouteName = RouteSelector ? RouteSelector->GetSelectedOption() : TEXT("Akron Sprint");

    return FString::Printf(
        TEXT("Tonight's run: %s on %s | %s setup | Route: %s"),
        *VehicleName,
        *VehicleClass,
        *HandlingMode,
        RouteName.IsEmpty() ? TEXT("Akron Sprint") : *RouteName
    );
}

FString UMainMenuWidget::BuildLaunchButtonText() const
{
    FString CityName = TEXT("Akron");
    FRaceGPSCityLayout Layout;
    if (UAkronXodrImporter::ResolveCityLayout(Layout) && !Layout.DisplayName.IsEmpty())
    {
        CityName = Layout.DisplayName;
    }

    const FString RouteName = RouteSelector ? RouteSelector->GetSelectedOption() : TEXT("");
    if (RouteName.IsEmpty())
    {
        return FString::Printf(TEXT("Choose a route to Drive %s"), *CityName);
    }
    return FString::Printf(TEXT("Drive %s — %s"), *CityName, *RouteName);
}

FString UMainMenuWidget::BuildVersionLine() const
{
    FString GameVer = FString(RACEGPS_VERSION_STRING);
    FString CityVer = TEXT("unknown");
    FString CityName = TEXT("Akron");

    // Read version/display name from the active city's manifest (either dialect).
    FRaceGPSCityLayout Layout;
    if (UAkronXodrImporter::ResolveCityLayout(Layout) && !Layout.ManifestPath.IsEmpty())
    {
        FString ManifestPath = FPaths::ProjectDir() / Layout.ManifestPath;
        FString Content;
        if (FFileHelper::LoadFileToString(Content, *ManifestPath))
        {
            TSharedPtr<FJsonObject> Root;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
            if (FJsonSerializer::Deserialize(Reader, Root))
            {
                Root->TryGetStringField(TEXT("version"), CityVer);
            }
        }
        if (!Layout.DisplayName.IsEmpty())
        {
            CityName = Layout.DisplayName;
        }
    }

    return FString::Printf(TEXT("Build %s • Citypack %s %s • Smooth arcade GPS racing"), *GameVer, *CityName, *CityVer);
}

void UMainMenuWidget::UpdateVehicleInfo()
{
    UVehicleTuningData* Selected = GetSelectedVehicle();
    if (!Selected || !VehicleInfoText)
        return;

    FString Info = FString::Printf(
        TEXT("%s • %s\n%s\nMass: %.0f kg | Speed Bias: %.0f km/h | Gears: %d\n%s"),
        *Selected->DisplayName,
        *BuildVehicleClassLabel(Selected),
        *Selected->Description,
        Selected->VehicleMass,
        Selected->MaxEngineRPM * 0.04f, // Rough km/h estimate
        Selected->Transmission.GearRatios.Num(),
        *BuildDriveSummaryText()
    );
    VehicleInfoText->SetText(FText::FromString(Info));

    if (HandlingInfoText)
    {
        const FString HandlingMode = GetSelectedHandlingMode();
        FString HandlingDescription = TEXT("Balanced all-round assist tuning.");
        if (HandlingMode == TEXT("Drift"))
        {
            HandlingDescription = TEXT("Aggressive oversteer, high handbrake bite, low assists.");
        }
        else if (HandlingMode == TEXT("Simulation"))
        {
            HandlingDescription = TEXT("Realistic transfer, restrained slip, stronger assists.");
        }
        HandlingInfoText->SetText(FText::FromString(FString::Printf(TEXT("Handling: %s\n%s\n%s"), *HandlingMode, *HandlingDescription, *BuildLaunchButtonText())));
    }
}

void UMainMenuWidget::OnVehicleSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    UpdateVehicleInfo();

    if (PlayButton)
    {
        PlayButton->SetToolTipText(FText::FromString(BuildLaunchButtonText()));
    }
}

void UMainMenuWidget::OnRouteSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    UpdateVehicleInfo();

    if (PlayButton)
    {
        PlayButton->SetToolTipText(FText::FromString(BuildLaunchButtonText()));
    }
}

void UMainMenuWidget::OnHandlingSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    UpdateVehicleInfo();

    if (PlayButton)
    {
        PlayButton->SetToolTipText(FText::FromString(BuildLaunchButtonText()));
    }
}
