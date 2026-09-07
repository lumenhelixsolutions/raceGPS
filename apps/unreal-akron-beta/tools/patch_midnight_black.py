from pathlib import Path

ROOT = Path(r"C:\projects\racegps\apps\unreal-akron-beta\Source\raceGPSAkronBeta")

# --- DayNightCycle.h ---
h = (ROOT / "Public" / "DayNightCycle.h").read_text(encoding="utf-8")
old = '''    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Atmosphere")
    TSoftObjectPtr<class UTextureCube> HDRIEnvironmentMap;
'''
new = '''    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Atmosphere")
    TSoftObjectPtr<class UTextureCube> HDRIEnvironmentMap;

    /** Floor directional intensity after sunset so Midnight Run is not a black void. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Atmosphere")
    float NightMoonIntensity = 1.45f;

    /** When true, night keeps a high moon directional (sun-below-horizon would unlit the world). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Atmosphere")
    bool bMoonAtNight = true;
'''
if old not in h:
    raise SystemExit('DayNightCycle.h HDRI block not found')
(ROOT / "Public" / "DayNightCycle.h").write_text(h.replace(old, new, 1), encoding="utf-8")
print('patched DayNightCycle.h')

# --- DayNightCycle.cpp ---
cpp = (ROOT / "Private" / "DayNightCycle.cpp").read_text(encoding="utf-8")

old_sphere = '''        SkySphere->SetStaticMesh(SphereMesh.Object);
        SkySphere->SetRelativeScale3D(FVector(10000.0f, 10000.0f, 10000.0f));
'''
new_sphere = '''        SkySphere->SetStaticMesh(SphereMesh.Object);
        SkySphere->SetRelativeScale3D(FVector(10000.0f, 10000.0f, 10000.0f));
        // Opaque engine sphere at 10km will occlude a camera outside it (intro cam is ~24km).
        // SkyAtmosphere is the actual sky; keep this mesh out of the game view.
        SkySphere->SetVisibility(false);
        SkySphere->SetHiddenInGame(true);
        SkySphere->SetCastShadow(false);
'''
if old_sphere not in cpp:
    raise SystemExit('SkySphere scale block not found')
cpp = cpp.replace(old_sphere, new_sphere, 1)

old_rot = '''void ADayNightCycle::UpdateSunRotation()
{
    DayProgress = CurrentTimeOfDay / 24.0f;
    float SunAngle = (DayProgress - 0.25f) * 360.0f; // Sunrise at 6:00 (0.25)

    FRotator SunRot;
    SunRot.Pitch = -FMath::Sin(FMath::DegreesToRadians(SunAngle)) * 80.0f;
    SunRot.Yaw = SunAngle + 90.0f;
    SunRot.Roll = 0.0f;

    SunLight->SetWorldRotation(SunRot);

    // Adjust intensity based on time
    float DayIntensity = 2.5f;
    float NightIntensity = 0.05f;
    float Intensity = IsDaytime() ? DayIntensity : NightIntensity;
    SunLight->SetIntensity(FMath::Lerp(SunLight->Intensity, Intensity, 0.1f));

    // Update SkyAtmosphere sun disc
    if (SkyAtmosphere && bUseSkyAtmosphere)
    {
        SkyAtmosphere->SetTickGroup(TG_DuringPhysics);
    }
}
'''
new_rot = '''void ADayNightCycle::UpdateSunRotation()
{
    DayProgress = CurrentTimeOfDay / 24.0f;
    float SunAngle = (DayProgress - 0.25f) * 360.0f; // Sunrise at 6:00 (0.25)

    FRotator SunRot;
    SunRot.Pitch = -FMath::Sin(FMath::DegreesToRadians(SunAngle)) * 80.0f;
    SunRot.Yaw = SunAngle + 90.0f;
    SunRot.Roll = 0.0f;

    const bool bNight = !IsDaytime();
    if (bNight && bMoonAtNight)
    {
        // 22:00 solar pitch is ~+69 (sun below horizon, light pointing at the sky).
        // With competing directional lights suppressed that leaves the world unlit = RGB 0,0,0.
        // Keep a high moon directional so SkyAtmosphere and the ground actually receive light.
        SunRot.Pitch = -52.0f;
        SunRot.Yaw = 210.0f;
    }

    SunLight->SetWorldRotation(SunRot);
    SunLight->SetVisibility(true);

    const float DayIntensity = 2.5f;
    const float MoonFloor = FMath::Max(NightMoonIntensity, 1.25f);
    if (bNight)
    {
        SunLight->SetIntensity(MoonFloor);
        SunLight->SetLightColor(FLinearColor(0.62f, 0.76f, 1.0f));
    }
    else
    {
        SunLight->SetIntensity(DayIntensity);
    }

    if (SkyAtmosphere && bUseSkyAtmosphere)
    {
        SkyAtmosphere->SetTickGroup(TG_DuringPhysics);
    }
}
'''
if old_rot not in cpp:
    raise SystemExit('UpdateSunRotation block not found')
cpp = cpp.replace(old_rot, new_rot, 1)

old_sky = '''    return FLinearColor(0.02f, 0.02f, 0.1f);
'''
new_sky = '''    // Visible midnight navy — not near-black. Applied to SkyLight via UpdateSkyColor.
    return FLinearColor(0.10f, 0.14f, 0.28f);
'''
if old_sky not in cpp:
    raise SystemExit('night GetSkyColor not found')
cpp = cpp.replace(old_sky, new_sky, 1)

(ROOT / "Private" / "DayNightCycle.cpp").write_text(cpp, encoding="utf-8")
print('patched DayNightCycle.cpp')
print('ok')
