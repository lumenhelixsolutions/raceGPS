# UE5 ↔ Node Backend Wire

M10 multiplayer lane: Unreal client talks to `apps/backend` on port **8787**.

## Flow

1. `POST /api/session` — cookie `racegps_sid`
2. `GET /api/rooms` or `POST /api/rooms` — list/create rooms
3. `WebSocket ws://127.0.0.1:8787/ws/rooms/{roomId}` with `Cookie: racegps_sid=...`

## UE5 implementation

| File | Role |
|------|------|
| `RaceGPSBackendClient.h/.cpp` | HTTP session + WebSocket client |
| `LANBrowserWidget` | `bPreferNodeBackend=true` uses backend instead of OnlineSubsystem LAN |

## Local dev

```powershell
# Terminal 1 — backend
cd apps/backend
npm run dev

# Terminal 2 — UE5 Editor
# Open raceGPSAkronBeta.uproject → LAN Browser → Host / Refresh
```

## Protocol

See `docs/WEBSOCKET_PROTOCOL.md` and `packages/protocol/src/index.ts`.

## Remaining (operator)

- PIE test with backend running
- Packaged build end-to-end Akron play
- Replace `AkronWorld.umap.placeholder` with imported level spec