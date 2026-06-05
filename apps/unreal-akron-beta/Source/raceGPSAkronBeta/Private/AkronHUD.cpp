#include "AkronHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Kismet/KismetStringLibrary.h"

AAkronHUD::AAkronHUD(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    static ConstructorHelpers::FObjectFinder<UFont> FontObj(TEXT("/Engine/EngineFonts/Roboto.Roboto"));
    if (FontObj.Succeeded())
    {
        MainFont = FontObj.Object;
        LargeFont = FontObj.Object;
    }
}

void AAkronHUD::BeginPlay()
{
    Super::BeginPlay();
}

void AAkronHUD::DrawHUD()
{
    Super::DrawHUD();

    if (bShowCountdown)
    {
        DrawCountdown();
    }
    else if (bShowFinished)
    {
        DrawFinishedScreen();
    }
    else
    {
        DrawRaceInfo();
    }
}

void AAkronHUD::DrawRaceInfo()
{
    if (!Canvas || !MainFont) return;

    float X = 50.0f;
    float Y = 50.0f;
    float Scale = 1.2f;

    // Time
    FString TimeStr = FString::Printf(TEXT("TIME: %s"), *FormatTime(RaceTime));
    FCanvasTextItem TimeItem(FVector2D(X, Y), FText::FromString(TimeStr), MainFont, TextColor);
    TimeItem.Scale = FVector2D(Scale, Scale);
    Canvas->DrawItem(TimeItem);

    // Checkpoints
    Y += 40.0f * Scale;
    FString CpStr = FString::Printf(TEXT("CHECKPOINTS: %d / %d"), CurrentCheckpoint, TotalCheckpoints);
    FCanvasTextItem CpItem(FVector2D(X, Y), FText::FromString(CpStr), MainFont, TextColor);
    CpItem.Scale = FVector2D(Scale, Scale);
    Canvas->DrawItem(CpItem);

    // Speed
    Y += 40.0f * Scale;
    FString SpeedStr = FString::Printf(TEXT("SPEED: %.0f km/h"), SpeedKmh);
    FCanvasTextItem SpeedItem(FVector2D(X, Y), FText::FromString(SpeedStr), MainFont, TextColor);
    SpeedItem.Scale = FVector2D(Scale, Scale);
    Canvas->DrawItem(SpeedItem);
}

void AAkronHUD::DrawCountdown()
{
    if (!Canvas || !LargeFont) return;

    FString CountStr = FString::Printf(TEXT("%d"), CountdownValue);
    if (CountdownValue <= 0)
    {
        CountStr = TEXT("GO!");
    }

    float TextScale = 4.0f;
    FVector2D TextSize = Canvas->ClippedTextSize(LargeFont, TextScale, FText::FromString(CountStr));
    float X = (Canvas->ClipX - TextSize.X) * 0.5f;
    float Y = (Canvas->ClipY - TextSize.Y) * 0.5f;

    FLinearColor Color = (CountdownValue <= 0) ? FLinearColor::Green : FLinearColor::Yellow;
    FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(CountStr), LargeFont, Color);
    Item.Scale = FVector2D(TextScale, TextScale);
    Canvas->DrawItem(Item);
}

void AAkronHUD::DrawFinishedScreen()
{
    if (!Canvas || !LargeFont || !MainFont) return;

    // Background overlay
    FCanvasTileItem Background(FVector2D(0.0f, 0.0f), FVector2D(Canvas->ClipX, Canvas->ClipY), FLinearColor(0.0f, 0.0f, 0.0f, 0.75f));
    Background.BlendMode = SE_BLEND_Translucent;
    Canvas->DrawItem(Background);

    // Title
    float TitleScale = 2.5f;
    FString TitleStr = TEXT("RACE COMPLETE");
    FVector2D TitleSize = Canvas->ClippedTextSize(LargeFont, TitleScale, FText::FromString(TitleStr));
    float TitleX = (Canvas->ClipX - TitleSize.X) * 0.5f;
    float TitleY = Canvas->ClipY * 0.3f;

    FCanvasTextItem TitleItem(FVector2D(TitleX, TitleY), FText::FromString(TitleStr), LargeFont, TextColor);
    TitleItem.Scale = FVector2D(TitleScale, TitleScale);
    Canvas->DrawItem(TitleItem);

    // Medal
    FLinearColor MedalColor = TextColor;
    if (FinishedMedal == TEXT("GOLD")) MedalColor = GoldColor;
    else if (FinishedMedal == TEXT("SILVER")) MedalColor = SilverColor;
    else if (FinishedMedal == TEXT("BRONZE")) MedalColor = BronzeColor;

    float MedalScale = 3.0f;
    FString MedalStr = FinishedMedal;
    FVector2D MedalSize = Canvas->ClippedTextSize(LargeFont, MedalScale, FText::FromString(MedalStr));
    float MedalX = (Canvas->ClipX - MedalSize.X) * 0.5f;
    float MedalY = TitleY + 80.0f;

    FCanvasTextItem MedalItem(FVector2D(MedalX, MedalY), FText::FromString(MedalStr), LargeFont, MedalColor);
    MedalItem.Scale = FVector2D(MedalScale, MedalScale);
    Canvas->DrawItem(MedalItem);

    // Time
    float TimeScale = 1.5f;
    FString TimeStr = FString::Printf(TEXT("Time: %s"), *FormatTime(FinishedTime));
    FVector2D TimeSize = Canvas->ClippedTextSize(MainFont, TimeScale, FText::FromString(TimeStr));
    float TimeX = (Canvas->ClipX - TimeSize.X) * 0.5f;
    float TimeY = MedalY + 100.0f;

    FCanvasTextItem TimeItem(FVector2D(TimeX, TimeY), FText::FromString(TimeStr), MainFont, TextColor);
    TimeItem.Scale = FVector2D(TimeScale, TimeScale);
    Canvas->DrawItem(TimeItem);

    // Prompt
    float PromptScale = 1.0f;
    FString PromptStr = TEXT("Press R to Restart  |  Press Esc to Menu");
    FVector2D PromptSize = Canvas->ClippedTextSize(MainFont, PromptScale, FText::FromString(PromptStr));
    float PromptX = (Canvas->ClipX - PromptSize.X) * 0.5f;
    float PromptY = TimeY + 60.0f;

    FCanvasTextItem PromptItem(FVector2D(PromptX, PromptY), FText::FromString(PromptStr), MainFont, FLinearColor::Gray);
    PromptItem.Scale = FVector2D(PromptScale, PromptScale);
    Canvas->DrawItem(PromptItem);
}

FString AAkronHUD::FormatTime(float Seconds) const
{
    int32 Minutes = FMath::FloorToInt(Seconds / 60.0f);
    int32 Secs = FMath::FloorToInt(Seconds) % 60;
    int32 Ms = FMath::FloorToInt((Seconds - FMath::FloorToInt(Seconds)) * 1000.0f);
    return FString::Printf(TEXT("%02d:%02d.%03d"), Minutes, Secs, Ms);
}

void AAkronHUD::SetRaceTime(float Seconds)
{
    RaceTime = Seconds;
}

void AAkronHUD::SetCheckpointProgress(int32 Current, int32 Total)
{
    CurrentCheckpoint = Current;
    TotalCheckpoints = Total;
}

void AAkronHUD::SetSpeedKmh(float Speed)
{
    SpeedKmh = Speed;
}

void AAkronHUD::SetCountdownValue(int32 Value)
{
    CountdownValue = Value;
}

void AAkronHUD::ShowCountdown(bool bVisible)
{
    bShowCountdown = bVisible;
    if (!bVisible)
    {
        CountdownValue = 0;
    }
}

void AAkronHUD::ShowRaceFinished(float FinalTime, const FString& Medal)
{
    bShowFinished = true;
    FinishedTime = FinalTime;
    FinishedMedal = Medal;
}
