#include "NeonHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Kismet/KismetStringLibrary.h"

ANeonHUD::ANeonHUD(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    static ConstructorHelpers::FObjectFinder<UFont> FontObj(TEXT("/Engine/EngineFonts/Roboto.Roboto"));
    if (FontObj.Succeeded())
    {
        MainFont = FontObj.Object;
        LargeFont = FontObj.Object;
    }
}

void ANeonHUD::BeginPlay()
{
    Super::BeginPlay();
}

void ANeonHUD::DrawHUD()
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

void ANeonHUD::DrawNeonPanel(float X, float Y, float Width, float Height, const FLinearColor& BorderColor)
{
    // Background
    FCanvasTileItem Background(FVector2D(X, Y), FVector2D(Width, Height), DarkBackground);
    Background.BlendMode = SE_BLEND_Translucent;
    Canvas->DrawItem(Background);

    // Top border
    FCanvasTileItem TopBorder(FVector2D(X, Y), FVector2D(Width, 2.0f), BorderColor);
    Canvas->DrawItem(TopBorder);

    // Bottom border
    FCanvasTileItem BottomBorder(FVector2D(X, Y + Height - 2.0f), FVector2D(Width, 2.0f), BorderColor);
    Canvas->DrawItem(BottomBorder);

    // Left border
    FCanvasTileItem LeftBorder(FVector2D(X, Y), FVector2D(2.0f, Height), BorderColor);
    Canvas->DrawItem(LeftBorder);

    // Right border
    FCanvasTileItem RightBorder(FVector2D(X + Width - 2.0f, Y), FVector2D(2.0f, Height), BorderColor);
    Canvas->DrawItem(RightBorder);
}

void ANeonHUD::DrawRaceInfo()
{
    if (!Canvas || !MainFont) return;

    float PanelX = 30.0f;
    float PanelY = 30.0f;
    float PanelW = 320.0f;
    float PanelH = 220.0f;

    DrawNeonPanel(PanelX, PanelY, PanelW, PanelH, NeonCyan);

    float X = PanelX + 20.0f;
    float Y = PanelY + 20.0f;
    float Scale = 1.1f;

    // Time
    FString TimeStr = FString::Printf(TEXT("TIME  %s"), *FormatTime(RaceTime));
    FCanvasTextItem TimeItem(FVector2D(X, Y), FText::FromString(TimeStr), MainFont, NeonCyan);
    TimeItem.Scale = FVector2D(Scale, Scale);
    Canvas->DrawItem(TimeItem);

    // Checkpoints
    Y += 40.0f * Scale;
    FString CpStr = FString::Printf(TEXT("CHECKPOINTS  %d / %d"), CurrentCheckpoint, TotalCheckpoints);
    FCanvasTextItem CpItem(FVector2D(X, Y), FText::FromString(CpStr), MainFont, NeonMagenta);
    CpItem.Scale = FVector2D(Scale, Scale);
    Canvas->DrawItem(CpItem);

    // Speed
    Y += 40.0f * Scale;
    FString SpeedStr = FString::Printf(TEXT("SPEED  %.0f km/h"), SpeedKmh);
    FCanvasTextItem SpeedItem(FVector2D(X, Y), FText::FromString(SpeedStr), MainFont, NeonGreen);
    SpeedItem.Scale = FVector2D(Scale, Scale);
    Canvas->DrawItem(SpeedItem);

    // RPM / Gear
    Y += 40.0f * Scale;
    FString TelemetryStr = FString::Printf(TEXT("RPM  %.0f  GEAR  %d"), EngineRPM, CurrentGear);
    FCanvasTextItem TelemetryItem(FVector2D(X, Y), FText::FromString(TelemetryStr), MainFont, NeonYellow);
    TelemetryItem.Scale = FVector2D(Scale, Scale);
    Canvas->DrawItem(TelemetryItem);
}

void ANeonHUD::DrawCountdown()
{
    if (!Canvas || !LargeFont) return;

    FString CountStr = FString::Printf(TEXT("%d"), CountdownValue);
    if (CountdownValue <= 0)
    {
        CountStr = TEXT("GO!");
    }

    float TextScale = 5.0f;
    float XL = 0.0f, YL = 0.0f;
    Canvas->TextSize(LargeFont, CountStr, XL, YL, TextScale, TextScale);
    FVector2D TextSize(XL, YL);
    float X = (Canvas->ClipX - TextSize.X) * 0.5f;
    float Y = (Canvas->ClipY - TextSize.Y) * 0.5f;

    FLinearColor Color = (CountdownValue <= 0) ? NeonGreen : NeonCyan;

    // Glow effect (draw multiple times with offset)
    for (int32 i = 3; i >= 0; --i)
    {
        FLinearColor GlowColor = Color;
        GlowColor.A = 0.3f - (i * 0.1f);
        float Offset = i * 4.0f;
        FCanvasTextItem GlowItem(FVector2D(X + Offset, Y + Offset), FText::FromString(CountStr), LargeFont, GlowColor);
        GlowItem.Scale = FVector2D(TextScale, TextScale);
        Canvas->DrawItem(GlowItem);
    }

    FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(CountStr), LargeFont, Color);
    Item.Scale = FVector2D(TextScale, TextScale);
    Canvas->DrawItem(Item);
}

void ANeonHUD::DrawFinishedScreen()
{
    if (!Canvas || !LargeFont || !MainFont) return;

    // Background overlay
    FCanvasTileItem Background(FVector2D(0.0f, 0.0f), FVector2D(Canvas->ClipX, Canvas->ClipY), DarkBackground);
    Background.BlendMode = SE_BLEND_Translucent;
    Canvas->DrawItem(Background);

    // Title
    float TitleScale = 2.5f;
    FString TitleStr = TEXT("RACE COMPLETE");
    float TitleXL = 0.0f, TitleYL = 0.0f;
    Canvas->TextSize(LargeFont, TitleStr, TitleXL, TitleYL, TitleScale, TitleScale);
    FVector2D TitleSize(TitleXL, TitleYL);
    float TitleX = (Canvas->ClipX - TitleSize.X) * 0.5f;
    float TitleY = Canvas->ClipY * 0.3f;

    FCanvasTextItem TitleItem(FVector2D(TitleX, TitleY), FText::FromString(TitleStr), LargeFont, NeonCyan);
    TitleItem.Scale = FVector2D(TitleScale, TitleScale);
    Canvas->DrawItem(TitleItem);

    // Medal
    FLinearColor MedalColor = NeonCyan;
    if (FinishedMedal == TEXT("GOLD")) MedalColor = NeonYellow;
    else if (FinishedMedal == TEXT("SILVER")) MedalColor = FLinearColor(0.75f, 0.75f, 0.75f);
    else if (FinishedMedal == TEXT("BRONZE")) MedalColor = FLinearColor(0.8f, 0.5f, 0.2f);

    float MedalScale = 3.0f;
    FString MedalStr = FinishedMedal;
    float MedalXL = 0.0f, MedalYL = 0.0f;
    Canvas->TextSize(LargeFont, MedalStr, MedalXL, MedalYL, MedalScale, MedalScale);
    FVector2D MedalSize(MedalXL, MedalYL);
    float MedalX = (Canvas->ClipX - MedalSize.X) * 0.5f;
    float MedalY = TitleY + 80.0f;

    FCanvasTextItem MedalItem(FVector2D(MedalX, MedalY), FText::FromString(MedalStr), LargeFont, MedalColor);
    MedalItem.Scale = FVector2D(MedalScale, MedalScale);
    Canvas->DrawItem(MedalItem);

    // Time
    float TimeScale = 1.5f;
    FString TimeStr = FString::Printf(TEXT("Time: %s"), *FormatTime(FinishedTime));
    float TimeXL = 0.0f, TimeYL = 0.0f;
    Canvas->TextSize(MainFont, TimeStr, TimeXL, TimeYL, TimeScale, TimeScale);
    FVector2D TimeSize(TimeXL, TimeYL);
    float TimeX = (Canvas->ClipX - TimeSize.X) * 0.5f;
    float TimeY = MedalY + 100.0f;

    FCanvasTextItem TimeItem(FVector2D(TimeX, TimeY), FText::FromString(TimeStr), MainFont, FLinearColor::White);
    TimeItem.Scale = FVector2D(TimeScale, TimeScale);
    Canvas->DrawItem(TimeItem);

    // Prompt
    float PromptScale = 1.0f;
    FString PromptStr = TEXT("Press R to Restart  |  Press Esc to Menu");
    float PromptXL = 0.0f, PromptYL = 0.0f;
    Canvas->TextSize(MainFont, PromptStr, PromptXL, PromptYL, PromptScale, PromptScale);
    FVector2D PromptSize(PromptXL, PromptYL);
    float PromptX = (Canvas->ClipX - PromptSize.X) * 0.5f;
    float PromptY = TimeY + 60.0f;

    FCanvasTextItem PromptItem(FVector2D(PromptX, PromptY), FText::FromString(PromptStr), MainFont, FLinearColor(0.5f, 0.5f, 0.5f));
    PromptItem.Scale = FVector2D(PromptScale, PromptScale);
    Canvas->DrawItem(PromptItem);
}

FString ANeonHUD::FormatTime(float Seconds) const
{
    int32 Minutes = FMath::FloorToInt(Seconds / 60.0f);
    int32 Secs = FMath::FloorToInt(Seconds) % 60;
    int32 Ms = FMath::FloorToInt((Seconds - FMath::FloorToInt(Seconds)) * 1000.0f);
    return FString::Printf(TEXT("%02d:%02d.%03d"), Minutes, Secs, Ms);
}

void ANeonHUD::SetRaceTime(float Seconds)
{
    RaceTime = Seconds;
}

void ANeonHUD::SetCheckpointProgress(int32 Current, int32 Total)
{
    CurrentCheckpoint = Current;
    TotalCheckpoints = Total;
}

void ANeonHUD::SetSpeedKmh(float Speed)
{
    SpeedKmh = Speed;
}

void ANeonHUD::SetTelemetry(float RPM, int32 Gear)
{
    EngineRPM = RPM;
    CurrentGear = Gear;
}

void ANeonHUD::SetCountdownValue(int32 Value)
{
    CountdownValue = Value;
}

void ANeonHUD::ShowCountdown(bool bVisible)
{
    bShowCountdown = bVisible;
    if (!bVisible)
    {
        CountdownValue = 0;
    }
}

void ANeonHUD::ShowRaceFinished(float FinalTime, const FString& Medal)
{
    bShowFinished = true;
    FinishedTime = FinalTime;
    FinishedMedal = Medal;
}
