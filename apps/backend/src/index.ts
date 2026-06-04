import http from 'node:http';
import express from 'express';
import cors from 'cors';
import cookie from 'cookie';
import { nanoid } from 'nanoid';
import { WebSocketServer, WebSocket } from 'ws';
import type {
  ClientMessage, RaceGPSMode, RoomPlayer, RoomSnapshot, ServerMessage,
  RaceLobby, RaceLobbyPlayer, RaceState, RaceResult,
} from '@racegps/protocol';
import { parseClientMessage } from '@racegps/protocol';
import {
  createRacerState, validateCheckpoint, allCheckpointsHit, recordGhostTick,
  createGhostReplay, createAIDriver, tickAIDriver, generateDefaultRoute,
  routeToCheckpoints,
  type Checkpoint, type RacerState, type AIDriver,
} from '@racegps/race-engine';

const PORT = Number(process.env.RACEGPS_PORT || 8787);
const WEB_ORIGIN = process.env.RACEGPS_WEB_ORIGIN || 'http://localhost:5173';

// ── Types ────────────────────────────────────────────────────

interface Session { playerId: string; displayName: string }
interface SocketState extends Session { roomId?: string; lobbyId?: string }
interface Room {
  roomId: string;
  title: string;
  mode: RaceGPSMode;
  createdAt: string;
  sockets: Map<WebSocket, SocketState>;
  players: Map<string, RoomPlayer>;
  lobbies: Map<string, RaceLobbyInternal>;
}
interface RaceLobbyInternal {
  lobby: RaceLobby;
  checkpoints: Checkpoint[];
  racers: Map<string, RacerState>;
  aiDrivers: AIDriver[];
  aiTickInterval?: ReturnType<typeof setInterval>;
  countdownTimer?: ReturnType<typeof setTimeout>;
  raceStartTimer?: ReturnType<typeof setTimeout>;
  raceStartMs: number;
  results: RaceResult[];
}

// ── State ────────────────────────────────────────────────────

const sessions = new Map<string, Session>();
const rooms = new Map<string, Room>();

const app = express();
app.use(express.json());
app.use(cors({ origin: WEB_ORIGIN, credentials: true }));

// ── HTTP Endpoints ───────────────────────────────────────────

app.get('/health', (_req, res) => res.json({ ok: true, service: 'racegps-backend' }));

app.post('/api/session', (req, res) => {
  const sid = nanoid();
  const displayName = String(req.body?.displayName || `Driver-${sid.slice(0, 4)}`).slice(0, 32);
  const session = { playerId: `p_${nanoid(10)}`, displayName };
  sessions.set(sid, session);
  res.cookie('racegps_sid', sid, {
    httpOnly: true, sameSite: 'lax', secure: false,
    maxAge: 1000 * 60 * 60 * 24 * 30,
  });
  res.json(session);
});

app.get('/api/me', (req, res) => {
  const sid = getSid(req.headers.cookie);
  const session = sid ? sessions.get(sid) : undefined;
  if (!session) return res.status(401).json({ error: 'not_authenticated' });
  return res.json(session);
});

app.get('/api/rooms', (_req, res) => {
  res.json([...rooms.values()].map(roomToSnapshot));
});

app.post('/api/rooms', (req, res) => {
  const title = String(req.body?.title || 'Cruise Room').slice(0, 80);
  const mode = sanitizeMode(req.body?.mode);
  const room = createRoom(title, mode);
  res.json(roomToSnapshot(room));
});

// ── HTTP Server + WebSocket Upgrade ─────────────────────────

const server = http.createServer(app);
const wss = new WebSocketServer({ noServer: true });

server.on('upgrade', (req, socket, head) => {
  if (!req.url?.startsWith('/ws/rooms/')) { socket.destroy(); return; }
  const sid = getSid(req.headers.cookie);
  const session = sid ? sessions.get(sid) : undefined;
  if (!session) {
    socket.write('HTTP/1.1 401 Unauthorized\r\n\r\n');
    socket.destroy();
    return;
  }
  wss.handleUpgrade(req, socket, head, ws => {
    (req as http.IncomingMessage & { racegpsSession?: Session }).racegpsSession = session;
    wss.emit('connection', ws, req);
  });
});

// ── WebSocket Connection ─────────────────────────────────────

wss.on('connection', (ws: WebSocket, req: http.IncomingMessage) => {
  const session = (req as http.IncomingMessage & { racegpsSession?: Session }).racegpsSession;
  if (!session) { ws.close(1008, 'Missing session'); return; }
  const state: SocketState = { ...session };
  send(ws, { type: 'welcome', ...session });

  const roomId = req.url?.split('/').pop() || 'global-cruise';
  joinRoom(ws, state, roomId, 'Cruise Room', 'cruise');

  ws.on('message', raw => {
    const msg = parseClientMessage(raw.toString());
    if (!msg) return send(ws, { type: 'system_message', message: 'Invalid message.', level: 'error' });
    handleClientMessage(ws, state, msg);
  });

  ws.on('close', () => leaveCurrentRoom(ws, state));
});

// ── Client Message Router ────────────────────────────────────

function handleClientMessage(ws: WebSocket, state: SocketState, msg: ClientMessage): void {
  switch (msg.type) {
    case 'join_room':
      joinRoom(ws, state, msg.roomId, msg.roomId, msg.mode || 'cruise');
      break;
    case 'leave_room':
      leaveCurrentRoom(ws, state);
      break;
    case 'player_position': {
      const room = state.roomId ? rooms.get(state.roomId) : undefined;
      if (!room) return;
      const player = room.players.get(state.playerId);
      if (player) { player.lat = msg.lat; player.lon = msg.lon; player.heading = msg.heading; player.speed = msg.speed; }
      broadcast(room, { type: 'player_position', playerId: state.playerId, lat: msg.lat, lon: msg.lon, heading: msg.heading, speed: msg.speed, seq: msg.seq }, ws);
      break;
    }
    case 'chat_message': {
      const room = state.roomId ? rooms.get(state.roomId) : undefined;
      if (!room) return;
      const clean = msg.message.replace(/\s+/g, ' ').trim().slice(0, 240);
      if (!clean) return;
      broadcast(room, { type: 'chat_message', playerId: state.playerId, displayName: state.displayName, message: clean, sentAt: new Date().toISOString() });
      break;
    }
    case 'challenge_signal': {
      const room = state.roomId ? rooms.get(state.roomId) : undefined;
      if (!room) return;
      broadcast(room, { type: 'challenge_signal', fromPlayerId: state.playerId, toPlayerId: msg.toPlayerId, signal: msg.signal, challengeType: msg.challengeType });
      break;
    }
    case 'game_event': {
      const room = state.roomId ? rooms.get(state.roomId) : undefined;
      if (!room) return;
      broadcast(room, { type: 'game_event', playerId: state.playerId, event: msg.event });
      break;
    }
    // ── Race Lobby ───────────────────────────────────────────
    case 'race_create_lobby':
      handleRaceCreateLobby(ws, state, msg.roomId, msg.title, msg.checkpointIds, msg.laps ?? 1, msg.maxPlayers ?? 8);
      break;
    case 'race_join_lobby':
      handleRaceJoinLobby(ws, state, msg.roomId, msg.lobbyId);
      break;
    case 'race_leave_lobby':
      handleRaceLeaveLobby(ws, state, msg.roomId, msg.lobbyId);
      break;
    case 'race_toggle_ready':
      handleRaceToggleReady(ws, state, msg.roomId, msg.lobbyId);
      break;
    case 'race_start_countdown':
      handleRaceStartCountdown(ws, state, msg.roomId, msg.lobbyId);
      break;
    case 'race_checkpoint_hit':
      handleRaceCheckpointHit(ws, state, msg.roomId, msg.lobbyId, msg.checkpointId, msg.seq);
      break;
    case 'race_finished':
      handleRaceFinished(ws, state, msg.roomId, msg.lobbyId, msg.elapsedMs, msg.ghost);
      break;
  }
}

// ── Race Lobby Handlers ──────────────────────────────────────

function handleRaceCreateLobby(ws: WebSocket, state: SocketState, roomId: string, title: string, checkpointIds: string[], laps: number, maxPlayers: number): void {
  const room = rooms.get(roomId);
  if (!room) return send(ws, { type: 'system_message', message: 'Room not found.', level: 'error' });

  // Generate default checkpoints if none provided
  const checkpoints = generateLobbyCheckpoints(roomId, checkpointIds);
  const lobbyId = `lobby_${nanoid(8)}`;

  const lobby: RaceLobby = {
    lobbyId, roomId, title, mode: 'race', state: 'lobby',
    checkpointIds: checkpoints.map(c => c.id), laps, maxPlayers,
    players: [], createdAt: new Date().toISOString(),
  };

  const internal: RaceLobbyInternal = {
    lobby, checkpoints,
    racers: new Map(), aiDrivers: [], raceStartMs: 0, results: [],
  };

  // Auto-join creator
  const lp: RaceLobbyPlayer = { playerId: state.playerId, displayName: state.displayName, ready: false, carIndex: 0 };
  lobby.players.push(lp);
  internal.racers.set(state.playerId, createRacerState(state.playerId, state.displayName));
  state.lobbyId = lobbyId;

  // Spawn AI opponents
  spawnAIDrivers(room, internal);

  room.lobbies.set(lobbyId, internal);
  send(ws, { type: 'race_lobby_snapshot', lobby });
  broadcast(room, { type: 'system_message', message: `${state.displayName} created race lobby "${title}". ${internal.aiDrivers.length} AI drivers joined.` });
}

function handleRaceJoinLobby(ws: WebSocket, state: SocketState, roomId: string, lobbyId: string): void {
  const room = rooms.get(roomId);
  if (!room) return;
  const li = room.lobbies.get(lobbyId);
  if (!li) return send(ws, { type: 'system_message', message: 'Lobby not found.', level: 'error' });
  if (li.lobby.state !== 'lobby') return send(ws, { type: 'system_message', message: 'Race already in progress.', level: 'error' });

  // Leave any existing lobby in this room
  if (state.lobbyId) handleRaceLeaveLobby(ws, state, roomId, state.lobbyId);

  state.lobbyId = lobbyId;
  const lp: RaceLobbyPlayer = { playerId: state.playerId, displayName: state.displayName, ready: false, carIndex: li.lobby.players.length };
  li.lobby.players.push(lp);
  li.racers.set(state.playerId, createRacerState(state.playerId, state.displayName));

  broadcastLobby(room, li);
  send(ws, { type: 'race_lobby_snapshot', lobby: li.lobby });
}

function handleRaceLeaveLobby(ws: WebSocket, state: SocketState, roomId: string, lobbyId: string): void {
  const room = rooms.get(roomId);
  if (!room) return;
  const li = room.lobbies.get(lobbyId);
  if (!li) return;

  li.lobby.players = li.lobby.players.filter(p => p.playerId !== state.playerId);
  li.racers.delete(state.playerId);
  state.lobbyId = undefined;

  if (li.lobby.players.length === 0) {
    clearLobbyTimers(li);
    room.lobbies.delete(lobbyId);
  } else {
    broadcastLobby(room, li);
  }
}

function handleRaceToggleReady(ws: WebSocket, state: SocketState, roomId: string, lobbyId: string): void {
  const room = rooms.get(roomId);
  if (!room) return;
  const li = room.lobbies.get(lobbyId);
  if (!li) return;

  const p = li.lobby.players.find(p => p.playerId === state.playerId);
  if (!p) return;
  p.ready = !p.ready;
  broadcastLobby(room, li);
}

function handleRaceStartCountdown(ws: WebSocket, state: SocketState, roomId: string, lobbyId: string): void {
  const room = rooms.get(roomId);
  if (!room) return;
  const li = room.lobbies.get(lobbyId);
  if (!li) return;
  if (li.lobby.state !== 'lobby') return;

  // Require all players ready (minimum 1)
  const allReady = li.lobby.players.length > 0 && li.lobby.players.every(p => p.ready);
  if (!allReady) return send(ws, { type: 'system_message', message: 'All players must be ready.', level: 'warning' });

  startRaceCountdown(room, li);
}

function startRaceCountdown(room: Room, li: RaceLobbyInternal): void {
  let count = 5;
  li.lobby.state = 'countdown';
  li.lobby.countdownStart = Date.now();
  broadcastLobby(room, li);

  const tick = () => {
    broadcast(room, { type: 'race_countdown', secondsRemaining: count });
    count--;
    if (count < 0) {
      startRace(room, li);
    } else {
      li.countdownTimer = setTimeout(tick, 1000);
    }
  };
  li.countdownTimer = setTimeout(tick, 1000);
}

function startRace(room: Room, li: RaceLobbyInternal): void {
  li.lobby.state = 'racing';
  li.lobby.raceStart = Date.now();
  li.raceStartMs = li.lobby.raceStart;
  li.results = [];

  broadcast(room, { type: 'race_started', lobbyId: li.lobby.lobbyId, raceStart: li.raceStartMs });
  broadcast(room, { type: 'system_message', message: `Race started! ${li.lobby.players.length} drivers on the grid.` });
  broadcastLobby(room, li);

  // Start AI drivers
  startAITicks(room, li);
}

function handleRaceCheckpointHit(ws: WebSocket, state: SocketState, roomId: string, lobbyId: string, checkpointId: string, seq: number): void {
  const room = rooms.get(roomId);
  if (!room) return;
  const li = room.lobbies.get(lobbyId);
  if (!li) return;
  if (li.lobby.state !== 'racing') return;

  const racer = li.racers.get(state.playerId);
  if (!racer || racer.finished) return;

  const player = room.players.get(state.playerId);
  const pos = player ? { lat: player.lat ?? 0, lon: player.lon ?? 0 } : { lat: 0, lon: 0 };
  const nowMs = Date.now();
  const result = validateCheckpoint(racer, checkpointId, li.checkpoints, pos, li.raceStartMs, nowMs);

  if (result.valid) {
    recordGhostTick(racer, seq, nowMs);
    broadcast(room, { type: 'game_event', playerId: state.playerId, event: { kind: 'checkpoint_hit', checkpointId, timestamp: nowMs } });
  }
}

function handleRaceFinished(ws: WebSocket, state: SocketState, roomId: string, lobbyId: string, elapsedMs: number, ghost: number[]): void {
  const room = rooms.get(roomId);
  if (!room) return;
  const li = room.lobbies.get(lobbyId);
  if (!li) return;
  if (li.lobby.state !== 'racing') return;

  const racer = li.racers.get(state.playerId);
  if (!racer || racer.finished) return;

  racer.finished = true;
  racer.finishMs = elapsedMs;

  const result: RaceResult = {
    playerId: state.playerId,
    displayName: state.displayName,
    finishOrder: li.results.length + 1,
    elapsedMs,
    checkpointsHit: racer.nextIndex,
    totalCheckpoints: li.checkpoints.length,
    ghost,
  };
  li.results.push(result);

  broadcast(room, { type: 'race_player_finished', lobbyId, result });

  // Check if all human players have finished
  const humanPlayers = li.lobby.players.filter(p => p.playerId.startsWith('p_'));
  const humansFinished = humanPlayers.filter(p => li.results.some(r => r.playerId === p.playerId));
  if (humansFinished.length >= humanPlayers.length) {
    finishRace(room, li);
  }
}

function finishRace(room: Room, li: RaceLobbyInternal): void {
  clearLobbyTimers(li);
  li.lobby.state = 'finished';

  // Add unfinished AI drivers as DNF results
  for (const ai of li.aiDrivers) {
    if (!ai.finished) {
      li.results.push({
        playerId: ai.playerId,
        displayName: ai.displayName,
        finishOrder: 999,
        elapsedMs: ai.elapsedMs,
        checkpointsHit: ai.currentIndex,
        totalCheckpoints: li.checkpoints.length,
        ghost: ai.ghost,
      });
    }
  }

  // Sort by elapsedMs then by checkpoints for DNF ordering
  const sorted = [...li.results].sort((a, b) => {
    if (a.elapsedMs !== b.elapsedMs) return a.elapsedMs - b.elapsedMs;
    return b.checkpointsHit - a.checkpointsHit;
  });

  // Reassign finish order
  sorted.forEach((r, i) => r.finishOrder = i + 1);
  li.results = sorted;

  const raceResults: RaceResult[] = sorted;
  const winner = sorted[0];

  broadcast(room, { type: 'race_results', lobbyId: li.lobby.lobbyId, results: raceResults, winner });
  broadcast(room, { type: 'system_message', message: `🏁 Race finished! ${winner.displayName} wins in ${(winner.elapsedMs / 1000).toFixed(1)}s.` });
  broadcastLobby(room, li);
}

function clearLobbyTimers(li: RaceLobbyInternal): void {
  if (li.countdownTimer) { clearTimeout(li.countdownTimer); li.countdownTimer = undefined; }
  if (li.raceStartTimer) { clearTimeout(li.raceStartTimer); li.raceStartTimer = undefined; }
  if (li.aiTickInterval) { clearInterval(li.aiTickInterval); li.aiTickInterval = undefined; }
}

function broadcastLobby(room: Room, li: RaceLobbyInternal): void {
  broadcast(room, { type: 'race_lobby_updated', lobby: li.lobby });
}

// ── AI Driver System ────────────────────────────────────────

const AI_NAMES = ['Blitz', 'Nitro', 'DriftKing', 'Turbo', 'Apex', 'GhostRider', 'Slipstream', 'Overdrive'];

function spawnAIDrivers(room: Room, li: RaceLobbyInternal): void {
  const count = Math.min(3, 8 - li.lobby.players.length);
  const route = generateDefaultRoute({ lat: 41.4993, lon: -81.6944 }, li.checkpoints.length + 1);

  for (let i = 0; i < count; i++) {
    const aiId = `ai_${nanoid(8)}`;
    const name = AI_NAMES[i % AI_NAMES.length] + (count > 1 ? ` ${i + 1}` : '');
    const speed = 10 + Math.random() * 4; // 10-14 m/s (~36-50 km/h)

    const ai = createAIDriver(aiId, `🤖 ${name}`, route, speed);
    li.aiDrivers.push(ai);

    // Add as lobby player
    const lp: RaceLobbyPlayer = { playerId: aiId, displayName: ai.displayName, ready: true, carIndex: li.lobby.players.length };
    li.lobby.players.push(lp);

    // Add as room player (so position broadcasts work)
    const rp: RoomPlayer = { playerId: aiId, displayName: ai.displayName, role: 'driver', mode: 'race', lat: route[0].lat, lon: route[0].lon };
    room.players.set(aiId, rp);
  }
}

function startAITicks(room: Room, li: RaceLobbyInternal): void {
  if (li.aiDrivers.length === 0) return;

  li.aiTickInterval = setInterval(() => {
    const now = Date.now();
    for (const ai of li.aiDrivers) {
      if (ai.finished) continue;
      const result = tickAIDriver(ai, 100, ai.routePoints);

      // Broadcast AI position
      broadcast(room, {
        type: 'player_position',
        playerId: ai.playerId,
        lat: result.point.lat, lon: result.point.lon,
        heading: 0, speed: ai.baseSpeedMs, seq: Math.floor(ai.elapsedMs / 100),
      });

      // AI auto-finishes when done
      if (result.finished) {
        const aiResult: RaceResult = {
          playerId: ai.playerId,
          displayName: ai.displayName,
          finishOrder: li.results.length + 1,
          elapsedMs: ai.elapsedMs,
          checkpointsHit: ai.currentIndex,
          totalCheckpoints: li.checkpoints.length,
          ghost: ai.ghost,
        };
        li.results.push(aiResult);
        broadcast(room, { type: 'race_player_finished', lobbyId: li.lobby.lobbyId, result: aiResult });
      }
    }

    // Check if race is done
    const totalRacers = li.lobby.players.length;
    if (li.results.length >= totalRacers && li.lobby.state === 'racing') {
      finishRace(room, li);
    }
  }, 100);
}

// ── Room Management ──────────────────────────────────────────

function joinRoom(ws: WebSocket, state: SocketState, roomId: string, title: string, mode: RaceGPSMode): void {
  leaveCurrentRoom(ws, state);
  const room = rooms.get(roomId) || createRoom(title, mode, roomId);
  room.sockets.set(ws, state);
  state.roomId = room.roomId;
  const player: RoomPlayer = {
    playerId: state.playerId, displayName: state.displayName,
    role: mode === 'hot_pursuit' ? 'runner' : 'driver', mode,
  };
  room.players.set(state.playerId, player);
  send(ws, { type: 'room_snapshot', room: roomToSnapshot(room) });
  broadcast(room, { type: 'player_joined', player }, ws);
  broadcast(room, { type: 'system_message', message: `${state.displayName} joined ${room.title}.` });
}

function leaveCurrentRoom(ws: WebSocket, state: SocketState): void {
  if (!state.roomId) return;
  const room = rooms.get(state.roomId);
  if (!room) return;

  // Leave any lobby
  if (state.lobbyId) {
    const li = room.lobbies.get(state.lobbyId);
    if (li) {
      li.lobby.players = li.lobby.players.filter(p => p.playerId !== state.playerId);
      li.racers.delete(state.playerId);
      if (li.lobby.players.length === 0) {
        clearLobbyTimers(li);
        room.lobbies.delete(state.lobbyId);
      } else {
        broadcastLobby(room, li);
      }
    }
    state.lobbyId = undefined;
  }

  room.sockets.delete(ws);
  room.players.delete(state.playerId);
  broadcast(room, { type: 'player_left', playerId: state.playerId });
  if (room.sockets.size === 0 && room.roomId !== 'global-cruise') rooms.delete(room.roomId);
  state.roomId = undefined;
}

function createRoom(title: string, mode: RaceGPSMode, roomId = slugRoom(title)): Room {
  const room: Room = {
    roomId, title, mode, createdAt: new Date().toISOString(),
    sockets: new Map(), players: new Map(), lobbies: new Map(),
  };
  rooms.set(roomId, room);
  return room;
}

function roomToSnapshot(room: Room): RoomSnapshot {
  return {
    roomId: room.roomId, title: room.title, mode: room.mode,
    createdAt: room.createdAt, players: [...room.players.values()],
  };
}

// ── Utilities ────────────────────────────────────────────────

function generateLobbyCheckpoints(roomId: string, ids: string[]): Checkpoint[] {
  // Use provided checkpoint IDs with default locations around a demo center
  const center = { lat: 41.4993, lon: -81.6944 }; // Cleveland
  const defaults = [
    { lat: 41.4993, lon: -81.6944 },
    { lat: 41.5003, lon: -81.6934 },
    { lat: 41.5013, lon: -81.6924 },
    { lat: 41.5003, lon: -81.6914 },
    { lat: 41.4993, lon: -81.6904 },
  ];
  return (ids.length > 0 ? ids : ['cp_0', 'cp_1', 'cp_2', 'cp_3', 'cp_4']).map((id, i) => ({
    id,
    point: defaults[i] || defaults[0],
    radiusMeters: 8 + i * 0.5,
    order: i,
  }));
}

function send(ws: WebSocket, msg: ServerMessage): void {
  if (ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify(msg));
}

function broadcast(room: Room, msg: ServerMessage, except?: WebSocket): void {
  for (const ws of room.sockets.keys()) if (ws !== except) send(ws, msg);
}

function getSid(header?: string): string | undefined {
  return header ? cookie.parse(header).racegps_sid : undefined;
}

function slugRoom(title: string): string {
  return `${title.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '')}-${nanoid(5)}`;
}

function sanitizeMode(value: unknown): RaceGPSMode {
  return ['cruise', 'race', 'challenge', 'hot_pursuit', 'explore'].includes(String(value)) ? value as RaceGPSMode : 'cruise';
}

server.listen(PORT, () => console.log(`raceGPS backend running on http://localhost:${PORT}`));
