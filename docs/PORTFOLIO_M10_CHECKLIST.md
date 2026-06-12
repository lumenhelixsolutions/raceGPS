# Portfolio M10 — racegps Shippable Beta Checklist

Milestone: Second Shippable Beta & Orchestration Entry (portfolio M10)

## Level & world

- [ ] Replace placeholder `AkronWorld.umap` with real UE5 level (lighting, landscape, materials)
- [x] Validate semantic compiler on second city (Cleveland — `generated/cleveland_5.0km/`, 2026-06-12)

## Multiplayer

- [x] Backend tests for room utils + HTTP/WS stack (`apps/backend/tests/`, `npm test`)
- [x] Wire UE5 LAN multiplayer UI to Node.js backend WebSocket (`RaceGPSBackendClient`, `LANBrowserWidget`)

## Distribution prep

- [ ] Steamworks online subsystem interfaces for future leaderboards
- [ ] Document build + package flow in `.agentdock/project-brain/current-state.md`

## Verification

- [ ] Akron beta playable end-to-end in packaged build
- [ ] Second city pipeline produces valid OpenDRIVE → UE5 bundle without manual fixes