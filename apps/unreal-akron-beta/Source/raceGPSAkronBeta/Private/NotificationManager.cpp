// Copyright raceGPS. All Rights Reserved.

#include "NotificationManager.h"

UNotificationManager::UNotificationManager()
    : DefaultDisplayDuration(3.0f)
    , MaxQueueSize(32)
    , CurrentDisplayTime(0.0f)
{
}

void UNotificationManager::ShowNotification(const FString& Title, const FString& Body, const FString& Icon)
{
    if (NotificationQueue.Num() >= MaxQueueSize)
    {
        UE_LOG(LogTemp, Warning, TEXT("[raceGPS] Notification queue full; dropping oldest"));
        NotificationQueue.RemoveAt(0);
    }

    bool bWasEmpty = NotificationQueue.IsEmpty();

    FNotificationData Data;
    Data.Title = Title;
    Data.Body = Body;
    Data.IconPath = Icon;
    Data.DisplayDuration = DefaultDisplayDuration;
    NotificationQueue.Add(Data);

    if (bWasEmpty)
    {
        CurrentDisplayTime = 0.0f;
        OnNotificationShown.Broadcast(Data);
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Notification: %s"), *Title);
    }
}

void UNotificationManager::ClearQueue()
{
    NotificationQueue.Empty();
    CurrentDisplayTime = 0.0f;
    OnQueueEmpty.Broadcast();
}

FNotificationData UNotificationManager::PeekNextNotification() const
{
    if (NotificationQueue.IsEmpty())
        return FNotificationData();
    return NotificationQueue[0];
}

void UNotificationManager::TickManager(float DeltaTime)
{
    if (NotificationQueue.IsEmpty())
        return;

    CurrentDisplayTime += DeltaTime;
    if (CurrentDisplayTime >= NotificationQueue[0].DisplayDuration)
    {
        NotificationQueue.RemoveAt(0);
        CurrentDisplayTime = 0.0f;

        if (!NotificationQueue.IsEmpty())
        {
            OnNotificationShown.Broadcast(NotificationQueue[0]);
            UE_LOG(LogTemp, Log, TEXT("[raceGPS] Notification: %s"), *NotificationQueue[0].Title);
        }
        else
        {
            OnQueueEmpty.Broadcast();
        }
    }
}
