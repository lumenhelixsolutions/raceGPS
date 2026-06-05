#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LeaderboardEntry.h"
#include "LeaderboardWidget.generated.h"

UCLASS()
class RACEGPSAKRONBETA_API ULeaderboardWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Leaderboard")
    void ShowLeaderboard(const FString& RouteId);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Leaderboard")
    void RefreshEntries();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Leaderboard")
    void OnBackClicked();

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* TitleText;

    UPROPERTY(meta = (BindWidget))
    class UVerticalBox* EntryList;

    UPROPERTY(meta = (BindWidget))
    class UButton* BackButton;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Leaderboard")
    int32 MaxDisplayEntries = 10;

protected:
    UPROPERTY()
    FString CurrentRouteId;

    void PopulateEntries(const TArray<FLeaderboardEntry>& Entries);
    FLinearColor GetMedalColor(const FString& Medal) const;
};
