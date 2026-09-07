from pathlib import Path

gm = Path(r"C:\projects\racegps\apps\unreal-akron-beta\Source\raceGPSAkronBeta\Private\ClevelandShowcaseGameMode.cpp")
t = gm.read_text(encoding="utf-8")
old = """\t// V13: low + flat so skyline sits on horizon (kill overhead HISM roof cloud).
\tconst FVector CamLoc = PawnLoc + FVector(4800.f, 2800.f, 160.f);
\tconst FVector LookAt = PawnLoc + FVector(-9000.f, -16000.f, 260.f);
\tFRotator CamRot = (LookAt - CamLoc).Rotation();
\tCamRot.Pitch = FMath::Clamp(CamRot.Pitch, -3.2f, -0.4f);"""
new = """\t// V13.1: allow slight UP pitch so upper frustum is sky, not T10 roof tops.
\tconst FVector CamLoc = PawnLoc + FVector(4200.f, 2400.f, 140.f);
\tconst FVector LookAt = PawnLoc + FVector(-8500.f, -15000.f, 520.f);
\tFRotator CamRot = (LookAt - CamLoc).Rotation();
\tCamRot.Pitch = FMath::Clamp(CamRot.Pitch, -1.5f, 3.0f);"""
if old not in t:
    raise SystemExit("cam block missing")
t = t.replace(old, new)
t = t.replace("Cam->SetFieldOfView(62.f);", "Cam->SetFieldOfView(70.f);")
gm.write_text(t, encoding="utf-8")
print("GameMode ok")

pawn = Path(r"C:\projects\racegps\apps\unreal-akron-beta\Source\raceGPSAkronBeta\Private\ChaosVehiclePawn.cpp")
p = pawn.read_text(encoding="utf-8")
oldp = """    // V13: flatter look so downtown facades fill horizon, not roof tops.
    const FVector LookAt = Pivot + FVector(-7000.0f, -11000.0f, 220.0f);
    FRotator WorldRot = (LookAt - Pivot).Rotation();
    WorldRot.Roll = 0.0f;
    WorldRot.Pitch = FMath::Clamp(WorldRot.Pitch, -3.5f, -0.8f);"""
newp = """    // V13.1: slight UP look — upper frustum is sky, not T10 roof cloud.
    const FVector LookAt = Pivot + FVector(-7000.0f, -11000.0f, 480.0f);
    FRotator WorldRot = (LookAt - Pivot).Rotation();
    WorldRot.Roll = 0.0f;
    WorldRot.Pitch = FMath::Clamp(WorldRot.Pitch, -1.8f, 2.5f);"""
if oldp not in p:
    raise SystemExit("pawn update missing")
p = p.replace(oldp, newp)
p = p.replace("ChaseCamera->SetFieldOfView(68.0f);", "ChaseCamera->SetFieldOfView(72.0f);")
p = p.replace("arm=1650 FOV=68", "arm=1650 FOV=72")
pawn.write_text(p, encoding="utf-8")
print("Pawn ok")

# More aggressive HISM roof cloud: dim sprawl emissive + tighter cull
ld = Path(r"C:\projects\racegps\apps\unreal-akron-beta\Source\raceGPSAkronBeta\Private\ClevelandLookDirector.cpp")
l = ld.read_text(encoding="utf-8")
l = l.replace(
    "    const float EmissiveStrGlass = 1.65f; // V13: City Sample interiors, no double-multiply\n    const float WindowTile = 9.0f;\n    const float AmountOffGlass = 0.34f;\n    const float AmountOffFacade = 0.50f;",
    "    const float EmissiveStrGlass = 1.55f; // V13.1: keep warmth but avoid overhead lit rims\n    const float WindowTile = 9.0f;\n    const float AmountOffGlass = 0.38f;\n    const float AmountOffFacade = 0.62f;",
)
l = l.replace("H->SetCullDistances(22000, 150000);", "H->SetCullDistances(8000, 70000);")
l = l.replace(
    "        GEngine->Exec(GetWorld(), TEXT(\"r.ViewDistanceScale 0.65\"));",
    "        GEngine->Exec(GetWorld(), TEXT(\"r.ViewDistanceScale 0.45\"));",
)
# Stronger fog
l = l.replace(
    """            Fog->SetFogDensity(0.0034f);
            Fog->SetFogHeightFalloff(0.18f);
            Fog->SetFogMaxOpacity(0.40f);
            Fog->SetStartDistance(4200.f);""",
    """            Fog->SetFogDensity(0.0048f);
            Fog->SetFogHeightFalloff(0.14f);
            Fog->SetFogMaxOpacity(0.55f);
            Fog->SetStartDistance(2800.f);""",
)
# Dim facade HISM slots (sprawl roofs) vs glass
l = l.replace(
    "                    const float Str = bGlass ? 1.75f : 0.90f;",
    "                    const float Str = bGlass ? 1.60f : 0.35f; // V13.1: dim non-glass sprawl rims",
)
ld.write_text(l, encoding="utf-8")
print("LookDirector ok")
print("done")
