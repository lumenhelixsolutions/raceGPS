from pathlib import Path
import re
env = Path(r"C:\projects\racegps\apps\unreal-akron-beta\Source\raceGPSAkronBeta\Private\ClevelandEnvironmentActor.cpp")
text = env.read_text(encoding="utf-8")

old = """	const float EmissiveStr = bMidnightRun ? (bGlass ? 2.40f : 1.15f) : 0.f; // V12 real M_NightWindow pins
	const FLinearColor Window = bGlass
		? FLinearColor(1.0f, 0.86f, 0.52f)
		: FLinearColor(1.0f, 0.78f, 0.42f);
	const FLinearColor EmissiveVec = Window * EmissiveStr;
	Mid->SetVectorParameterValue(TEXT("EmissiveColor"), EmissiveVec);
	Mid->SetVectorParameterValue(TEXT("Emissive"), EmissiveVec);
	Mid->SetVectorParameterValue(TEXT("EmissiveColor2"), EmissiveVec);
	Mid->SetScalarParameterValue(TEXT("EmissiveStrength"), EmissiveStr);
	Mid->SetScalarParameterValue(TEXT("EmissiveIntensity"), EmissiveStr);
	Mid->SetScalarParameterValue(TEXT("EmissiveMultiplier"), EmissiveStr);
	Mid->SetScalarParameterValue(TEXT("WindowLight"), EmissiveStr);
}"""
new = """	// V13 City Sample pins: tint separate from strength (no double-multiply sheets).
	const float EmissiveStr = bMidnightRun ? (bGlass ? 2.60f : 1.35f) : 0.f;
	const FLinearColor Window = bGlass
		? FLinearColor(1.0f, 0.86f, 0.52f)
		: FLinearColor(1.0f, 0.78f, 0.42f);
	Mid->SetVectorParameterValue(TEXT("EmissiveColor"), Window);
	Mid->SetVectorParameterValue(TEXT("Emissive"), Window);
	Mid->SetVectorParameterValue(TEXT("EmissiveColor2"), Window);
	Mid->SetVectorParameterValue(TEXT("InteriorTint"), Window);
	Mid->SetScalarParameterValue(TEXT("EmissiveStrength"), EmissiveStr);
	Mid->SetScalarParameterValue(TEXT("EmissiveIntensity"), EmissiveStr);
	Mid->SetScalarParameterValue(TEXT("EmissiveMultiplier"), EmissiveStr);
	Mid->SetScalarParameterValue(TEXT("WindowLight"), EmissiveStr);
	Mid->SetScalarParameterValue(TEXT("WindowTile"), bGlass ? 12.0f : 8.0f);
	Mid->SetScalarParameterValue(TEXT("AmountOff"), bGlass ? 0.28f : 0.45f);
	Mid->SetScalarParameterValue(TEXT("InteriorExposure"), bGlass ? 1.30f : 1.00f);
	Mid->SetScalarParameterValue(TEXT("InteriorDepth"), bGlass ? 0.70f : 0.50f);
	Mid->SetScalarParameterValue(TEXT("LumaVariation"), 0.34f);
}
"""
if old not in text:
    raise SystemExit('ApplyCommonMIDParams emissive block missing')
text = text.replace(old, new)
print('ok ApplyCommonMIDParams')

# Named tower denser/brighter force path
old_nt = """			for (int32 i = 0; i < Num; ++i)
			{
				const bool bGlassSlot = (i % 2) == 1 || bMidnightRun; // V11: prefer lit glass on named towers at night
				if (UMaterialInstanceDynamic* Mid = MakeLookMID(
					bGlassSlot ? TEXT("Building_Glass") : TEXT("Building_Concrete"),
					bMidnightRun, bGlassSlot))
				{
					Mesh->SetMaterial(i, Mid);
				}
			}"""
new_nt = """			for (int32 i = 0; i < Num; ++i)
			{
				// V13: force M_NightWindow path on every named-tower section (denser/brighter).
				const bool bGlassSlot = true;
				if (UMaterialInstanceDynamic* Mid = MakeLookMID(
					TEXT("Building_Glass"),
					bMidnightRun, bGlassSlot))
				{
					if (bMidnightRun)
					{
						Mid->SetScalarParameterValue(TEXT("WindowTile"), 14.0f);
						Mid->SetScalarParameterValue(TEXT("AmountOff"), 0.20f);
						Mid->SetScalarParameterValue(TEXT("EmissiveStrength"), 3.10f);
						Mid->SetScalarParameterValue(TEXT("InteriorExposure"), 1.45f);
						Mid->SetScalarParameterValue(TEXT("InteriorDepth"), 0.80f);
						Mid->SetVectorParameterValue(TEXT("InteriorTint"), FLinearColor(1.0f, 0.84f, 0.48f));
						Mid->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(1.0f, 0.90f, 0.55f));
						Mid->SetScalarParameterValue(TEXT("WindowSeed"), float(i) * 0.17f);
					}
					Mesh->SetMaterial(i, Mid);
				}
			}"""
if old_nt not in text:
    raise SystemExit('named tower loop missing')
text = text.replace(old_nt, new_nt)
print('ok named towers')

# Skyline glass pass also denser
old_sky = """	if (bMidnightRun && SkylineMesh)
	{
		if (UMaterialInstanceDynamic* GlassMid = MakeLookMID(TEXT("Building_Glass"), true, true))
		{
			SkylineMesh->SetMaterial(1, GlassMid);
			if (SkylineMesh->GetNumSections() > 0)
			{
				SkylineMesh->SetMaterial(0, GlassMid);
			}
		}
	}"""
new_sky = """	if (bMidnightRun && SkylineMesh)
	{
		if (UMaterialInstanceDynamic* GlassMid = MakeLookMID(TEXT("Building_Glass"), true, true))
		{
			GlassMid->SetScalarParameterValue(TEXT("WindowTile"), 13.0f);
			GlassMid->SetScalarParameterValue(TEXT("AmountOff"), 0.24f);
			GlassMid->SetScalarParameterValue(TEXT("EmissiveStrength"), 2.85f);
			GlassMid->SetScalarParameterValue(TEXT("InteriorExposure"), 1.35f);
			GlassMid->SetScalarParameterValue(TEXT("InteriorDepth"), 0.72f);
			SkylineMesh->SetMaterial(1, GlassMid);
			if (SkylineMesh->GetNumSections() > 0)
			{
				SkylineMesh->SetMaterial(0, GlassMid);
			}
		}
	}"""
if old_sky not in text:
    raise SystemExit('skyline glass pass missing')
text = text.replace(old_sky, new_sky)
print('ok skyline denser')

# Wet asphalt values
old_asph = """		Mid->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.090f, 0.084f, 0.076f));
		Mid->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.090f, 0.084f, 0.076f));
		Mid->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.090f, 0.084f, 0.076f));
		Mid->SetScalarParameterValue(TEXT("Roughness"), 0.16f);
		Mid->SetScalarParameterValue(TEXT("Specular"), 0.90f);
		Mid->SetScalarParameterValue(TEXT("Metallic"), 0.05f);"""
new_asph = """		Mid->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.125f, 0.115f, 0.098f));
		Mid->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.125f, 0.115f, 0.098f));
		Mid->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.125f, 0.115f, 0.098f));
		Mid->SetScalarParameterValue(TEXT("Roughness"), 0.08f);
		Mid->SetScalarParameterValue(TEXT("Specular"), 0.97f);
		Mid->SetScalarParameterValue(TEXT("Metallic"), 0.10f);"""
if old_asph not in text:
    raise SystemExit('asphalt values missing')
text = text.replace(old_asph, new_asph)
print('ok asphalt')

env.write_text(text, encoding='utf-8')
print('EnvironmentActor written OK')
