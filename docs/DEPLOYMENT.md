# raceGPS Deployment Guide

## Release Packaging

### Quick Release (Windows)

```powershell
# 1. Run tests
python -m pytest tests/ -v

# 2. Build UE5 project
cd apps/unreal-akron-beta
.\Build.bat

# 3. Package release
cd ../..
.\scripts\package-release.ps1
```

Output in `dist/`:
- `racegps-tools-vX.X.X.zip` — Python semantic compiler + universal city compiler
- `racegps-game-vX.X.X.zip` — Packaged UE5 game
- `racegps-source-vX.X.X.zip` — Full source bundle

### CI/CD (GitHub Actions)

The `.github/workflows/ue5-build.yml` pipeline runs on every push:

1. **Python Tests** — pytest for all tools
2. **UE5 Build** — Compiles the C++ project
3. **Release Package** — Auto-creates GitHub release on tag push

```bash
# Tag a release
git tag -a v0.2.0 -m "Release v0.2.0"
git push origin v0.2.0
```

## Distribution Channels

| Channel | Status | Notes |
|---------|--------|-------|
| GitHub Releases | ✅ Ready | Primary distribution |
| Steam | ⬜ Planned | Requires Steamworks SDK integration |
| Epic Games Store | ⬜ Planned | Requires Epic partner account |
| Itch.io | ⬜ Optional | Good for beta builds |

## Server Deployment

### Linux Dedicated Server

```bash
# Build server target
cd apps/unreal-akron-beta
./Build.sh -target=Server -platform=Linux

# Deploy to cloud
scp -r Build/Linux/Server user@host:/opt/racegps/
ssh user@host "systemctl restart racegps-server"
```

## System Requirements

| Tier | Minimum | Recommended |
|------|---------|-------------|
| OS | Windows 10 64-bit | Windows 11 64-bit |
| CPU | Quad-core 2.5 GHz | 6-core 3.5 GHz |
| RAM | 8 GB | 16 GB |
| GPU | GTX 1060 / RX 580 | RTX 3060 / RX 6700 XT |
| Storage | 5 GB SSD | 5 GB NVMe SSD |
| DirectX | Version 12 | Version 12 |

## Post-Release Checklist

- [ ] Version bumped in all manifests
- [ ] CHANGELOG.md updated
- [ ] Git tag created
- [ ] GitHub release published
- [ ] Source package verified
- [ ] Game package smoke-tested
- [ ] Tools package tested on clean machine
