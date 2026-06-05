#include "LeaderboardWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "LeaderboardSystem.h"
#include "Engine/World.h"

void ULeaderboardWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (BackButton)
    {
        BackButton->OnClicked.AddDynamic(this, &ULeaderboardWidget::OnBackClicked);
    }
}

void ULeaderboardWidget::ShowLeaderboard(const FString& RouteId)
{
    CurrentRouteId = RouteId;

    if (TitleText)
    {
        TitleText->SetText(FText::FromString(FString::Printf(TEXT("LEADERBOARD — %s"), *RouteId)));
    }

    RefreshEntries();
}

void ULeaderboardWidget::RefreshEntries()
{
    if (!EntryList)
        return;

    EntryList->ClearChildren();

    ULeaderboardSystem* LB = NewObject<ULeaderboardSystem>(this);
    LB->LoadLeaderboard(CurrentRouteId);
    TArray<FLeaderboardEntry> Entries = LB->GetTopEntries(CurrentRouteId, MaxDisplayEntries);

    PopulateEntries(Entries);
}

void ULeaderboardWidget::OnBackClicked()
{
    RemoveFromParent();
}

void ULeaderboardWidget::PopulateEntries(const TArray<FLeaderboardEntry>& Entries)
{
    for (int32 i = 0; i < Entries.Num(); ++i)
    {
        const FLeaderboardEntry& Entry = Entries[i];

        UHorizontalBox* Row = NewObject<UHorizontalBox>(this);

        // Rank
        UTextBlock* RankText = NewObject<UTextBlock>(this);
        RankText->SetText(FText::FromString(FString::Printf(TEXT("%d."), i + 1)));
        RankText->SetColorAndOpacity(FLinearColor::White);
        UHorizontalBoxSlot* RankSlot = Row->AddChildToHorizontalBox(RankText);
        RankSlot->SetPadding(FMargin(0.0f, 0.0f, 20.0f, 0.0f));

        // Name
        UTextBlock* NameText = NewObject<UTextBlock>(this);
        NameText->SetText(FText::FromString(Entry.PlayerName));
        NameText->SetColorAndOpacity(Entry.bIsPlayer ? FLinearColor(0.0f, 0.9f, 1.0f) : FLinearColor::White);
        UHorizontalBoxSlot* NameSlot = Row->AddChildToHorizontalBox(NameText);
        NameSlot->SetPadding(FMargin(0.0f, 0.0f, 20.0f, 0.0f));
        NameSlot->SetFillWidth(1.0f);

        // Time
        UTextBlock* TimeText = NewObject<UTextBlock>(this);
        int32 Minutes = FMath::FloorToInt(Entry.TimeSeconds / 60.0f);
        int32 Secs = FMath::FloorToInt(Entry.TimeSeconds) % 60;
        int32 Ms = FMath::FloorToInt((Entry.TimeSeconds - FMath::FloorToInt(Entry.TimeSeconds)) * 1000.0f);
        TimeText->SetText(FText::FromString(FString::Printf(TEXT("%02d:%02d.%03d"), Minutes, Secs, Ms)));
        TimeText->SetColorAndOpacity(FLinearColor::White);
        UHorizontalBoxSlot* TimeSlot = Row->AddChildToHorizontalBox(TimeText);
        TimeSlot->SetPadding(FMargin(0.0f, 0.0f, 20.0f, 0.0f));

        // Medal
        UTextBlock* MedalText = NewObject<UTextBlock>(this);
        MedalText->SetText(FText::FromString(Entry.Medal));
        MedalText->SetColorAndOpacity(GetMedalColor(Entry.Medal));
        Row->AddChildToHorizontalBox(MedalText);

        UVerticalBoxSlot* RowSlot = EntryList->AddChildToVerticalBox(Row);
        RowSlot->SetPadding(FMargin(0.0f, 5.0f, 0.0f, 5.0f));
    }
}

FLinearColor ULeaderboardWidget::GetMedalColor(const FString& Medal) const
{
    if (Medal == TEXT("GOLD"))
        return FLinearColor(1.0f, 0.84f, 0.0f);
    if (Medal == TEXT("SILVER"))
        return FLinearColor(0.75f, 0.75f, 0.75f);
    if (Medal == TEXT("BRONZE"))
        return FLinearColor(0.8f, 0.5f, 0.2f);
    return FLinearColor::Gray;
}
