from pathlib import Path
import re
ld = Path(r"C:\projects\racegps\apps\unreal-akron-beta\Source\raceGPSAkronBeta\Private\ClevelandLookDirector.cpp")
text = ld.read_text(encoding="utf-8")

def must_replace(old, new, label):
    global text
    if old not in text:
        raise SystemExit(f"MISSING: {label}")
    text = text.replace(old, new)
    print("ok", label)

must_replace(
    'Post->EpicPreset.BloomThreshold = 0.90f; // V12: facade-only windows, keep sky dark',
    'Post->EpicPreset.BloomThreshold = 0.85f; // V13: warm interior windows, keep sky dark',
    'bloom',
)
must_replace(
    'Post->EpicPreset.SceneColorTintB = 1.00f; // V12: stop navy crush on ground',
    'Post->EpicPreset.SceneColorTintB = 0.96f; // V13: warmer grade, less navy ground',
    'tintB',
)
must_replace(
    'Post->EpicPreset.AutoExposureBias = 1.30f;',
    'Post->EpicPreset.AutoExposureBias = 1.15f;',
    'bias',
)
must_replace(
    'raceGPS Cleveland look: MidnightRun V12 applied (22h moon 2.35, sky 2.20, exposure bias +1.30, fog+lamps)',
    'raceGPS Cleveland look: MidnightRun V13 applied (22h moon 2.35, sky 2.20, exposure bias +1.15, wet ground+lamps)',
    'midnight log',
)
must_replace(
    """            Fog->SetFogDensity(0.0020f);
            Fog->SetFogHeightFalloff(0.20f);
            Fog->SetFogMaxOpacity(0.18f);
            Fog->SetStartDistance(9000.f);
            Fog->SetFogInscatteringColor(FLinearColor(0.07f, 0.08f, 0.12f));""",
    """            // V13: denser fog softens overhead HISM roof sprawl into night sky.
            Fog->SetFogDensity(0.0034f);
            Fog->SetFogHeightFalloff(0.18f);
            Fog->SetFogMaxOpacity(0.40f);
            Fog->SetStartDistance(4200.f);
            Fog->SetFogInscatteringColor(FLinearColor(0.05f, 0.055f, 0.08f));""",
    'fog',
)
must_replace(
    """                    Comp->SetIntensity(120000.f);
                    Comp->SetAttenuationRadius(14000.f);""",
    """                    Comp->SetIntensity(240000.f);
                    Comp->SetAttenuationRadius(18000.f);""",
    'lamps',
)
must_replace(
    """                    Comp->SetIntensity(210000.f);
                    Comp->SetAttenuationRadius(20000.f);""",
    """                    Comp->SetIntensity(380000.f);
                    Comp->SetAttenuationRadius(26000.f);""",
    'key light',
)
must_replace(
    """                    Comp->SetIntensity(48000.f);
                    Comp->SetAttenuationRadius(5200.f);""",
    """                    Comp->SetIntensity(90000.f);
                    Comp->SetAttenuationRadius(7000.f);""",
    'neon',
)
must_replace(
    """        GEngine->Exec(GetWorld(), TEXT("r.VolumetricCloud 0"));
        GEngine->Exec(GetWorld(), TEXT("r.SkyAtmosphere 1"));
        GEngine->Exec(GetWorld(), TEXT("r.BloomQuality 5"));
        GEngine->Exec(GetWorld(), TEXT("r.Tonemapper.Quality 5"));
        GEngine->Exec(GetWorld(), TEXT("r.DefaultFeature.AutoExposure 1"));
        GEngine->Exec(GetWorld(), TEXT("r.EyeAdaptationQuality 2"));
    }
    UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland look: MidnightRun V13 applied (22h moon 2.35, sky 2.20, exposure bias +1.15, wet ground+lamps)"));""",
    """        GEngine->Exec(GetWorld(), TEXT("r.VolumetricCloud 0"));
        GEngine->Exec(GetWorld(), TEXT("r.SkyAtmosphere 1"));
        GEngine->Exec(GetWorld(), TEXT("r.BloomQuality 5"));
        GEngine->Exec(GetWorld(), TEXT("r.Tonemapper.Quality 5"));
        GEngine->Exec(GetWorld(), TEXT("r.DefaultFeature.AutoExposure 1"));
        GEngine->Exec(GetWorld(), TEXT("r.EyeAdaptationQuality 2"));
        // V13: cull small roof sprawl sooner; tall skyline remains.
        GEngine->Exec(GetWorld(), TEXT("r.ViewDistanceScale 0.65"));
    }
    UE_LOG(LogTemp, Log, TEXT("raceGPS Cleveland look: MidnightRun V13 applied (22h moon 2.35, sky 2.20, exposure bias +1.15, wet ground+lamps)"));""",
    'exec vd',
)
must_replace(
    "    const float EmissiveStrGlass = 1.25f; // V12: roofs gated; facades can take more\n    const float WindowTile = 8.0f;",
    "    const float EmissiveStrGlass = 1.65f; // V13: City Sample interiors, no double-multiply\n    const float WindowTile = 9.0f;\n    const float AmountOffGlass = 0.34f;\n    const float AmountOffFacade = 0.50f;",
    'emissive consts',
)

old_mid = """        const FLinearColor Win = ((Salt % 3) == 0) ? WindowCool : WindowWarm;
        const FLinearColor EmissiveVec = Win * EmissiveStrGlass;
        Mid->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.03f, 0.035f, 0.045f));
        Mid->SetVectorParameterValue(TEXT("EmissiveColor"), EmissiveVec);
        Mid->SetVectorParameterValue(TEXT("Emissive"), EmissiveVec);
        Mid->SetScalarParameterValue(TEXT("EmissiveStrength"), EmissiveStrGlass);
        Mid->SetScalarParameterValue(TEXT("EmissiveIntensity"), EmissiveStrGlass);
        Mid->SetScalarParameterValue(TEXT("WindowLight"), EmissiveStrGlass);
        Mid->SetScalarParameterValue(TEXT("WindowTile"), WindowTile);
        Mid->SetScalarParameterValue(TEXT("Roughness"), 0.32f);
        Mid->SetScalarParameterValue(TEXT("Metallic"), 0.28f);
        return Mid;"""
new_mid = """        const FLinearColor Win = ((Salt % 3) == 0) ? WindowCool : WindowWarm;
        // V13: do NOT bake strength into EmissiveColor (V12 double-multiply -> sheet look).
        Mid->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.03f, 0.035f, 0.045f));
        Mid->SetVectorParameterValue(TEXT("EmissiveColor"), Win);
        Mid->SetVectorParameterValue(TEXT("Emissive"), Win);
        Mid->SetVectorParameterValue(TEXT("InteriorTint"), Win);
        Mid->SetScalarParameterValue(TEXT("EmissiveStrength"), EmissiveStrGlass);
        Mid->SetScalarParameterValue(TEXT("EmissiveIntensity"), EmissiveStrGlass);
        Mid->SetScalarParameterValue(TEXT("WindowLight"), EmissiveStrGlass);
        Mid->SetScalarParameterValue(TEXT("WindowTile"), WindowTile);
        Mid->SetScalarParameterValue(TEXT("AmountOff"), AmountOffGlass);
        Mid->SetScalarParameterValue(TEXT("InteriorExposure"), 1.15f);
        Mid->SetScalarParameterValue(TEXT("InteriorDepth"), 0.55f);
        Mid->SetScalarParameterValue(TEXT("LumaVariation"), 0.32f);
        Mid->SetScalarParameterValue(TEXT("WindowSeed"), float(Salt % 97) * 0.01f);
        Mid->SetScalarParameterValue(TEXT("Roughness"), 0.28f);
        Mid->SetScalarParameterValue(TEXT("Metallic"), 0.35f);
        return Mid;"""
must_replace(old_mid, new_mid, 'MakeWindowMID')

start = text.find('const float Str = bGlass ? 1.45f : 0.80f;')
if start < 0:
    raise SystemExit('slot start missing')
marker = 'Mid->SetVectorParameterValue(TEXT("BaseColor"),'
mpos = text.find(marker, start)
close = text.find(');', mpos)
close = text.find('\n', close)
new_slot = """const float Str = bGlass ? 1.75f : 0.90f;
                    const FLinearColor Win = (((Mi + HismComps) % 3) == 0)
                        ? FLinearColor(0.55f, 0.75f, 1.0f) : FLinearColor(1.0f, 0.82f, 0.48f);
                    Mid->SetVectorParameterValue(TEXT("EmissiveColor"), Win);
                    Mid->SetVectorParameterValue(TEXT("InteriorTint"), Win);
                    Mid->SetScalarParameterValue(TEXT("EmissiveStrength"), Str);
                    Mid->SetScalarParameterValue(TEXT("WindowTile"), bGlass ? 11.0f : 7.0f);
                    Mid->SetScalarParameterValue(TEXT("AmountOff"), bGlass ? AmountOffGlass : AmountOffFacade);
                    Mid->SetScalarParameterValue(TEXT("InteriorExposure"), bGlass ? 1.25f : 0.95f);
                    Mid->SetScalarParameterValue(TEXT("InteriorDepth"), 0.55f);
                    Mid->SetScalarParameterValue(TEXT("WindowSeed"), float((Mi + HismComps) % 97) * 0.01f);
                    Mid->SetVectorParameterValue(TEXT("BaseColor"),
                        bGlass ? FLinearColor(0.030f, 0.034f, 0.042f) : FLinearColor(0.055f, 0.052f, 0.048f));
"""
text = text[:start] + new_slot + text[close+1:]
print('ok slot override')

marker = 'if (bGlass) { ++GlassSlots; } else { ++FacadeSlots; }'
pos = text.find(marker)
old_tail_start = text.find('H->MarkRenderStateDirty();', pos)
if 'H->SetCullDistances' not in text[old_tail_start:old_tail_start+220]:
    text = text[:old_tail_start] + 'H->SetCullDistances(22000, 150000);\n            ' + text[old_tail_start:]
    print('ok cull distances')

text, n = re.subn(
    r'TEXT\("raceGPS Cleveland look: V12 T10 HISM facade-only windows .{1,3} comps=%d instances=%d glass=%d facade=%d skippedActors=%d"\)',
    'TEXT("raceGPS Cleveland look: V13 T10 HISM CitySample windows - comps=%d instances=%d glass=%d facade=%d skippedActors=%d")',
    text,
    count=1,
)
if n != 1:
    raise SystemExit(f'HISM log replace count={n}')
print('ok HISM log')

must_replace(
    """                    Mid->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.090f, 0.084f, 0.076f));
                    Mid->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.090f, 0.084f, 0.076f));
                    Mid->SetScalarParameterValue(TEXT("Roughness"), 0.16f);
                    Mid->SetScalarParameterValue(TEXT("Specular"), 0.90f);
                    Mid->SetScalarParameterValue(TEXT("Metallic"), 0.05f);""",
    """                    Mid->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.120f, 0.110f, 0.095f));
                    Mid->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.120f, 0.110f, 0.095f));
                    Mid->SetScalarParameterValue(TEXT("Roughness"), 0.09f);
                    Mid->SetScalarParameterValue(TEXT("Specular"), 0.96f);
                    Mid->SetScalarParameterValue(TEXT("Metallic"), 0.08f);""",
    'terrain wet',
)

text, n = re.subn(
    r'raceGPS Cleveland look: V12 night ground wetness .{1,3} terrainSections=%d',
    'raceGPS Cleveland look: V13 night ground wetness - terrainSections=%d',
    text,
    count=1,
)
if n != 1:
    raise SystemExit(f'ground log replace count={n}')
print('ok ground log')

ld.write_text(text, encoding='utf-8')
print('LookDirector written OK')
