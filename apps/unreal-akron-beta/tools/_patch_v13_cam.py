from pathlib import Path

gm = Path(r"C:\projects\racegps\apps\unreal-akron-beta\Source\raceGPSAkronBeta\Private\ClevelandShowcaseGameMode.cpp")
text = gm.read_text(encoding="utf-8")
old_cam = """\tconst FVector CamLoc = PawnLoc + FVector(3600.f, 2100.f, 420.f);   // V12 lower pitch, 3-car grid\n\tconst FVector LookAt = PawnLoc + FVector(-7000.f, -12000.f, 80.f); // V12 skyline on horizon (south)\n\tFRotator CamRot = (LookAt - CamLoc).Rotation();\n\tCamRot.Pitch = FMath::Clamp(CamRot.Pitch, -8.0f, -2.5f);"""
new_cam = """\t// V13: low + flat so skyline sits on horizon (kill overhead HISM roof cloud).\n\tconst FVector CamLoc = PawnLoc + FVector(4800.f, 2800.f, 160.f);\n\tconst FVector LookAt = PawnLoc + FVector(-9000.f, -16000.f, 260.f);\n\tFRotator CamRot = (LookAt - CamLoc).Rotation();\n\tCamRot.Pitch = FMath::Clamp(CamRot.Pitch, -3.2f, -0.4f);"""
if old_cam not in text:
    raise SystemExit("GameMode camera block not found")
text = text.replace(old_cam, new_cam)
text = text.replace("\t\tCam->SetFieldOfView(78.f);", "\t\tCam->SetFieldOfView(62.f);")
text = text.replace("V12 %s capture", "V13 %s capture")
text = text.replace("cleveland_v12_hero.png", "cleveland_v13_hero.png")
text = text.replace("cleveland_v12_chase.png", "cleveland_v13_chase.png")
text = text.replace("staged V12 %s still", "staged V13 %s still")
text = text.replace("V12 %s PNG not found", "V13 %s PNG not found")
old_alias = """\t\t\tif (DestName.Contains(TEXT(\"hero\")))\n\t\t\t{\n\t\t\t\tIFileManager::Get().Copy(*(TempDir / TEXT(\"cleveland_v11_hero.png\")), *Best, true, true);\n\t\t\t}"""
new_alias = """\t\t\tif (DestName.Contains(TEXT(\"hero\")))\n\t\t\t{\n\t\t\t\tIFileManager::Get().Copy(*(TempDir / TEXT(\"cleveland_v11_hero.png\")), *Best, true, true);\n\t\t\t\tIFileManager::Get().Copy(*(TempDir / TEXT(\"cleveland_v12_hero.png\")), *Best, true, true);\n\t\t\t}"""
if old_alias not in text:
    raise SystemExit("alias block not found")
text = text.replace(old_alias, new_alias)
gm.write_text(text, encoding="utf-8")
print("GameMode patched")

pawn = Path(r"C:\projects\racegps\apps\unreal-akron-beta\Source\raceGPSAkronBeta\Private\ChaosVehiclePawn.cpp")
ptext = pawn.read_text(encoding="utf-8")
old_upd = """    const FVector LookAt = Pivot + FVector(-7000.0f, -11000.0f, 40.0f);\n    FRotator WorldRot = (LookAt - Pivot).Rotation();\n    WorldRot.Roll = 0.0f;\n    WorldRot.Pitch = FMath::Clamp(WorldRot.Pitch, -8.0f, -4.0f);"""
new_upd = """    // V13: flatter look so downtown facades fill horizon, not roof tops.\n    const FVector LookAt = Pivot + FVector(-7000.0f, -11000.0f, 220.0f);\n    FRotator WorldRot = (LookAt - Pivot).Rotation();\n    WorldRot.Roll = 0.0f;\n    WorldRot.Pitch = FMath::Clamp(WorldRot.Pitch, -3.5f, -0.8f);"""
if old_upd not in ptext:
    raise SystemExit("pawn Update chase block not found")
ptext = ptext.replace(old_upd, new_upd)
old_apply = """    SpringArm->TargetArmLength = 1450.0f;\n    SpringArm->SocketOffset = FVector(0.0f, 220.0f, 150.0f);\n    // V12: lower pitch so downtown sits on the horizon, not as an overhead roof cloud.\n    SpringArm->TargetOffset = FVector(-200.0f, -520.0f, 30.0f);\n    SpringArm->bDoCollisionTest = false;\n    SpringArm->ProbeSize = 16.0f;\n    ChaseCamera->SetFieldOfView(92.0f);"""
new_apply = """    SpringArm->TargetArmLength = 1650.0f;\n    SpringArm->SocketOffset = FVector(0.0f, 160.0f, 70.0f);\n    // V13: magazine-flat chase — skyline on horizon, less overhead HISM roof cloud.\n    SpringArm->TargetOffset = FVector(-150.0f, -480.0f, 90.0f);\n    SpringArm->bDoCollisionTest = false;\n    SpringArm->ProbeSize = 16.0f;\n    ChaseCamera->SetFieldOfView(68.0f);"""
if old_apply not in ptext:
    raise SystemExit("pawn Apply chase block not found")
ptext = ptext.replace(old_apply, new_apply)
ptext = ptext.replace(
    "applied showcase chase framing WORLD-SOUTH arm=1450 FOV=92",
    "applied showcase chase framing WORLD-SOUTH arm=1650 FOV=68",
)
pawn.write_text(ptext, encoding="utf-8")
print("Pawn patched")
print("ok")
