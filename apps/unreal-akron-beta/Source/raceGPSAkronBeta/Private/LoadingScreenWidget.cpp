#include "LoadingScreenWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Kismet/KismetMathLibrary.h"

void ULoadingScreenWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (CityNameText)
    {
        CityNameText->SetText(FText::FromString(TEXT("AKRON, OHIO")));
    }

    if (LoadingTips.Num() == 0)
    {
        LoadingTips.Add(TEXT("Tip: Clean driving earns time bonuses"));
        LoadingTips.Add(TEXT("Tip: Missed checkpoints add penalties"));
        LoadingTips.Add(TEXT("Tip: Use the handbrake for tight corners"));
        LoadingTips.Add(TEXT("Tip: Follow the ghost for the best line"));
        LoadingTips.Add(TEXT("Tip: Collisions slow you down"));
        LoadingTips.Add(TEXT("Tip: Time of day affects visibility"));
    }

    UpdateTip(0.0f);
}

void ULoadingScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    DisplayProgress = FMath::FInterpTo(DisplayProgress, TargetProgress, InDeltaTime, 5.0f);

    if (ProgressBar)
    {
        ProgressBar->SetPercent(DisplayProgress);
    }

    UpdateTip(InDeltaTime);
}

void ULoadingScreenWidget::SetProgress(float Percent)
{
    TargetProgress = FMath::Clamp(Percent, 0.0f, 1.0f);
}

void ULoadingScreenWidget::SetStatusText(const FString& Text)
{
    if (StatusText)
    {
        StatusText->SetText(FText::FromString(Text));
    }
}

void ULoadingScreenWidget::StartLoading()
{
    TargetProgress = 0.0f;
    DisplayProgress = 0.0f;
    SetVisibility(ESlateVisibility::Visible);
    SetProgress(0.0f);
    SetStatusText(TEXT("Loading city data..."));
}

void ULoadingScreenWidget::FinishLoading()
{
    SetProgress(1.0f);
    FTimerHandle HideTimer;
    GetWorld()->GetTimerManager().SetTimer(HideTimer, [this]()
    {
        SetVisibility(ESlateVisibility::Collapsed);
    }, 0.5f, false);
}

void ULoadingScreenWidget::UpdateTip(float DeltaTime)
{
    if (LoadingTips.Num() == 0 || !TipText)
        return;

    TipTimer += DeltaTime;
    if (TipTimer >= 4.0f)
    {
        TipTimer = 0.0f;
        CurrentTipIndex = (CurrentTipIndex + 1) % LoadingTips.Num();
        TipText->SetText(FText::FromString(LoadingTips[CurrentTipIndex]));
    }
}
