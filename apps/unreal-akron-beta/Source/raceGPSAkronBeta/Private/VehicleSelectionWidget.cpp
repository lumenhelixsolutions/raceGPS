#include "VehicleSelectionWidget.h"
#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"
#include "VehicleTuningData.h"

void UVehicleSelectionWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (VehicleSelector)
    {
        VehicleSelector->OnSelectionChanged.AddDynamic(this, &UVehicleSelectionWidget::OnVehicleSelected);
    }
}

void UVehicleSelectionWidget::PopulateVehicleList(const TArray<UVehicleTuningData*>& Vehicles)
{
    AvailableVehicles = Vehicles;

    if (!VehicleSelector)
        return;

    VehicleSelector->ClearOptions();
    for (UVehicleTuningData* Vehicle : AvailableVehicles)
    {
        if (Vehicle)
        {
            VehicleSelector->AddOption(Vehicle->DisplayName);
        }
    }

    if (AvailableVehicles.Num() > 0)
    {
        VehicleSelector->SetSelectedOption(AvailableVehicles[0]->DisplayName);
        UpdateVehicleInfo();
    }
}

UVehicleTuningData* UVehicleSelectionWidget::GetSelectedVehicle() const
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

void UVehicleSelectionWidget::OnVehicleSelected(const FString& SelectedItem, ESelectInfo::Type SelectionType)
{
    UpdateVehicleInfo();
}

void UVehicleSelectionWidget::UpdateVehicleInfo()
{
    UVehicleTuningData* Selected = GetSelectedVehicle();
    if (!Selected)
        return;

    if (VehicleNameText)
    {
        VehicleNameText->SetText(FText::FromString(Selected->DisplayName));
    }

    if (VehicleDescriptionText)
    {
        VehicleDescriptionText->SetText(FText::FromString(Selected->Description));
    }

    if (VehicleStatsText)
    {
        FString Stats = FString::Printf(
            TEXT("Mass: %.0f kg\nMax RPM: %.0f\nGears: %d\nDrag: %.2f"),
            Selected->VehicleMass,
            Selected->MaxEngineRPM,
            Selected->Transmission.GearRatios.Num(),
            Selected->DragCoefficient
        );
        VehicleStatsText->SetText(FText::FromString(Stats));
    }
}
