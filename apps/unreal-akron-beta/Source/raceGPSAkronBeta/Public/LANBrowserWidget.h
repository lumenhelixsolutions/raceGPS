#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LANBrowserWidget.generated.h"

UCLASS()
class RACEGPSAKRONBETA_API ULANBrowserWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

    UFUNCTION(BlueprintCallable, Category = "raceGPS|LAN")
    void OnHostClicked();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|LAN")
    void OnJoinClicked();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|LAN")
    void OnRefreshClicked();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|LAN")
    void OnSessionSelected(int32 Index);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|LAN")
    void OnBackClicked();

    UFUNCTION()
    void OnSessionsFound(const TArray<FBlueprintSessionResult>& Results);

    UFUNCTION()
    void OnJoinComplete(bool bSuccess);

    UFUNCTION()
    void OnHostComplete(bool bSuccess);

    UPROPERTY(meta = (BindWidget))
    class UButton* HostButton;

    UPROPERTY(meta = (BindWidget))
    class UButton* JoinButton;

    UPROPERTY(meta = (BindWidget))
    class UButton* RefreshButton;

    UPROPERTY(meta = (BindWidget))
    class UButton* BackButton;

    UPROPERTY(meta = (BindWidget))
    class UListView* SessionList;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* StatusText;

    UPROPERTY(meta = (BindWidget))
    class USlider* MaxPlayersSlider;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* MaxPlayersText;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|LAN")
    TSubclassOf<class ULANSessionManager> LANManagerClass;

    /** When true, Host/Refresh use Node backend (8787) instead of UE OnlineSubsystem LAN. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Backend")
    bool bPreferNodeBackend = true;

protected:
    UPROPERTY()
    TObjectPtr<class ULANSessionManager> LANManager;

    UPROPERTY()
    TObjectPtr<class URaceGPSBackendClient> BackendClient;

    UFUNCTION()
    void OnBackendSessionReady(bool bSuccess);

    UFUNCTION()
    void OnBackendRoomsListed(const FString& RoomsJson);

    UFUNCTION()
    void OnBackendConnected(bool bSuccess);

    UPROPERTY()
    TArray<FBlueprintSessionResult> FoundSessions;

    int32 SelectedSessionIndex = -1;

    UFUNCTION()
    void UpdateMaxPlayersDisplay();
};
