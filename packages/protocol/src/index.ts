export type RaceGPSMode = 'cruise' | 'race' | 'challenge' | 'hot_pursuit' | 'explore';
export type PlayerRole = 'driver' | 'cop' | 'runner' | 'ghost';

// ── Race Lobby ──────────────────────────────────────────────

export type RaceState = 'lobby' | 'countdown' | 'racing' | 'finished';

export interface RaceLobby {
  lobbyId: string;
  roomId: string;
  title: string;
  mode: RaceGPSMode;
  state: RaceState;
  checkpointIds: string[];
  laps: number;
  maxPlayers: number;
  players: RaceLobbyPlayer[];
  createdAt: string;
  countdownStart?: number;
  raceStart?: number;
}

export interface RaceLobbyPlayer {
  playerId: string;
  displayName: string;
  ready: boolean;
  carIndex: number;
}

export interface RaceResult {
  playerId: string;
  displayName: string;
  finishOrder: number;
  elapsedMs: number;
  checkpointsHit: number;
  totalCheckpoints: number;
  ghost?: number[]; // seq timestamps for ghost replay
}

// ── Client Messages ─────────────────────────────────────────

export type ClientMessage =
  | { type: 'join_room'; roomId: string; displayName: string; mode?: RaceGPSMode }
  | { type: 'leave_room'; roomId: string }
  | { type: 'player_position'; roomId: string; lat: number; lon: number; heading: number; speed: number; seq: number }
  | { type: 'chat_message'; roomId: string; message: string }
  | { type: 'challenge_signal'; roomId: string; toPlayerId?: string; signal: ChallengeSignal; challengeType: ChallengeType }
  | { type: 'game_event'; roomId: string; event: GameEvent }
  // Race lobby
  | { type: 'race_create_lobby'; roomId: string; title: string; checkpointIds: string[]; laps?: number; maxPlayers?: number }
  | { type: 'race_join_lobby'; roomId: string; lobbyId: string }
  | { type: 'race_leave_lobby'; roomId: string; lobbyId: string }
  | { type: 'race_toggle_ready'; roomId: string; lobbyId: string }
  | { type: 'race_start_countdown'; roomId: string; lobbyId: string }
  | { type: 'race_checkpoint_hit'; roomId: string; lobbyId: string; checkpointId: string; seq: number }
  | { type: 'race_finished'; roomId: string; lobbyId: string; elapsedMs: number; ghost: number[] };

// ── Server Messages ─────────────────────────────────────────

export type ServerMessage =
  | { type: 'welcome'; playerId: string; displayName: string }
  | { type: 'room_snapshot'; room: RoomSnapshot }
  | { type: 'player_joined'; player: RoomPlayer }
  | { type: 'player_left'; playerId: string }
  | { type: 'player_position'; playerId: string; lat: number; lon: number; heading: number; speed: number; seq: number }
  | { type: 'chat_message'; playerId: string; displayName: string; message: string; sentAt: string }
  | { type: 'challenge_signal'; fromPlayerId: string; toPlayerId?: string; signal: ChallengeSignal; challengeType: ChallengeType }
  | { type: 'system_message'; message: string; level?: 'info' | 'warning' | 'error' }
  | { type: 'game_event'; playerId: string; event: GameEvent }
  // Race lobby
  | { type: 'race_lobby_snapshot'; lobby: RaceLobby }
  | { type: 'race_lobby_updated'; lobby: RaceLobby }
  | { type: 'race_countdown'; secondsRemaining: number }
  | { type: 'race_started'; lobbyId: string; raceStart: number }
  | { type: 'race_player_finished'; lobbyId: string; result: RaceResult }
  | { type: 'race_results'; lobbyId: string; results: RaceResult[]; winner: RaceResult };

// ── Signals & Challenge Types ────────────────────────────────

export type ChallengeSignal = 'flash_lights' | 'rev_engine' | 'drop_pin' | 'hot_signal' | 'ghost_signal';
export type ChallengeType = 'quick_sprint' | 'drag_race' | 'route_race' | 'hot_pursuit' | 'ghost_race';

// ── Game Events ──────────────────────────────────────────────

export type GameEvent =
  | { kind: 'checkpoint_hit'; checkpointId: string; timestamp: number }
  | { kind: 'object_pickup'; objectId: string; timestamp: number }
  | { kind: 'pursuit_tag'; targetPlayerId: string; distanceMeters: number; timestamp: number }
  | { kind: 'race_start'; startsAt: number }
  | { kind: 'race_finish'; elapsedMs: number };

// ── Room Types ───────────────────────────────────────────────

export interface RoomPlayer {
  playerId: string;
  displayName: string;
  role: PlayerRole;
  mode: RaceGPSMode;
  lat?: number;
  lon?: number;
  heading?: number;
  speed?: number;
}

export interface RoomSnapshot {
  roomId: string;
  mode: RaceGPSMode;
  title: string;
  players: RoomPlayer[];
  createdAt: string;
}

// ── Parser ───────────────────────────────────────────────────

export function isObject(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null;
}

export function parseClientMessage(raw: string): ClientMessage | null {
  try {
    const value = JSON.parse(raw);
    if (!isObject(value) || typeof value.type !== 'string') return null;
    return value as ClientMessage;
  } catch {
    return null;
  }
}
