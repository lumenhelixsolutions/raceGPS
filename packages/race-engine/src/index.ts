export interface GeoPoint { lat: number; lon: number }
export interface RouteObject { id: string; type: string; point: GeoPoint; score: number; radiusMeters: number }
export interface Checkpoint { id: string; point: GeoPoint; radiusMeters: number; order: number }

// ── Geography ────────────────────────────────────────────────

export function haversineMeters(a: GeoPoint, b: GeoPoint): number {
  const r = 6371000;
  const dLat = toRad(b.lat - a.lat);
  const dLon = toRad(b.lon - a.lon);
  const lat1 = toRad(a.lat);
  const lat2 = toRad(b.lat);
  const h = Math.sin(dLat / 2) ** 2 + Math.cos(lat1) * Math.cos(lat2) * Math.sin(dLon / 2) ** 2;
  return 2 * r * Math.asin(Math.sqrt(h));
}

function toRad(deg: number): number { return deg * Math.PI / 180; }

export function isWithinRadius(player: GeoPoint, target: GeoPoint, radiusMeters: number): boolean {
  return haversineMeters(player, target) <= radiusMeters;
}

// ── Scoring ──────────────────────────────────────────────────

export function scorePickup(baseScore: number, heatMultiplier = 1): number {
  return Math.round(baseScore * Math.max(1, heatMultiplier));
}

export function computeHeat(currentHeat: number, delta: number): number {
  return Math.max(0, Math.min(6, currentHeat + delta));
}

export function validatePursuitTag(cop: GeoPoint, runner: GeoPoint, captureRadiusMeters = 8): boolean {
  return isWithinRadius(cop, runner, captureRadiusMeters);
}

// ── Race Checkpoints ─────────────────────────────────────────

export interface CheckpointState {
  checkpointId: string;
  hitAt?: number; // ms since race start
  seq?: number;
}

export interface RacerState {
  playerId: string;
  displayName: string;
  checkpoints: Map<string, CheckpointState>;
  nextIndex: number;
  finished: boolean;
  finishMs?: number;
  ghost: number[]; // seq-per-100ms snapshots
  lastGhostSnap: number;
}

export function createRacerState(playerId: string, displayName: string): RacerState {
  return { playerId, displayName, checkpoints: new Map(), nextIndex: 0, finished: false, ghost: [], lastGhostSnap: 0 };
}

export function validateCheckpoint(
  racer: RacerState,
  checkpointId: string,
  checkpoints: Checkpoint[],
  playerPos: GeoPoint,
  raceStartMs: number,
  nowMs: number,
): { valid: boolean; reason?: string } {
  const target = checkpoints.find(c => c.id === checkpointId);
  if (!target) return { valid: false, reason: 'unknown_checkpoint' };
  if (target.order !== racer.nextIndex) return { valid: false, reason: 'wrong_order' };
  if (!isWithinRadius(playerPos, target.point, target.radiusMeters)) return { valid: false, reason: 'out_of_range' };

  const elapsed = nowMs - raceStartMs;
  racer.checkpoints.set(checkpointId, { checkpointId, hitAt: elapsed });
  racer.nextIndex++;

  return { valid: true };
}

export function allCheckpointsHit(racer: RacerState, totalCheckpoints: number): boolean {
  return racer.nextIndex >= totalCheckpoints;
}

// ── AI / Ghost Opponents ─────────────────────────────────────

export interface GhostReplay {
  playerId: string;
  displayName: string;
  ghost: number[];   // seq timestamps
  elapsedMs: number;
  checkpointsHit: number;
}

export function recordGhostTick(racer: RacerState, seq: number, nowMs: number, tickMs = 100): void {
  if (nowMs - racer.lastGhostSnap >= tickMs) {
    racer.ghost.push(seq);
    racer.lastGhostSnap = nowMs;
  }
}

export function createGhostReplay(racer: RacerState, elapsedMs: number, totalCheckpoints: number): GhostReplay {
  return {
    playerId: racer.playerId,
    displayName: racer.displayName,
    ghost: [...racer.ghost],
    elapsedMs,
    checkpointsHit: racer.nextIndex,
  };
}

// AI driver: follows a route of checkpoints at realistic but fallible speed
export interface AIDriver {
  playerId: string;
  displayName: string;
  routePoints: GeoPoint[];
  checkpointOrder: number[];
  baseSpeedMs: number;  // meters per second
  currentIndex: number;
  finished: boolean;
  finishTimeMs: number;
  ghost: number[];
  elapsedMs: number;
}

export function createAIDriver(
  playerId: string,
  displayName: string,
  routePoints: GeoPoint[],
  baseSpeedMs = 12, // ~43 km/h — arcade street speed
): AIDriver {
  return {
    playerId,
    displayName,
    routePoints,
    checkpointOrder: routePoints.map((_, i) => i),
    baseSpeedMs,
    currentIndex: 0,
    finished: false,
    finishTimeMs: 0,
    ghost: [],
    elapsedMs: 0,
  };
}

/**
 * Advance the AI driver by `deltaMs` along its route.
 * Returns the new position and whether it just hit a waypoint.
 */
export function tickAIDriver(ai: AIDriver, deltaMs: number, route: GeoPoint[]): { point: GeoPoint; hitWaypoint: boolean; finished: boolean } {
  if (ai.finished) return { point: route[route.length - 1], hitWaypoint: false, finished: true };

  ai.elapsedMs += deltaMs;
  const speedVariation = 0.8 + Math.random() * 0.4; // 0.8x – 1.2x
  const dist = ai.baseSpeedMs * (deltaMs / 1000) * speedVariation;

  let remaining = dist;
  let hitWaypoint = false;

  while (remaining > 0 && ai.currentIndex < route.length - 1) {
    const current = route[ai.currentIndex];
    const next = route[ai.currentIndex + 1];
    const segDist = haversineMeters(current, next);

    if (remaining >= segDist) {
      remaining -= segDist;
      ai.currentIndex++;
      hitWaypoint = true;
      ai.ghost.push(ai.elapsedMs);
    } else {
      remaining = 0;
    }
  }

  if (ai.currentIndex >= route.length - 1) {
    ai.finished = true;
    ai.finishTimeMs = ai.elapsedMs;
    return { point: route[route.length - 1], hitWaypoint, finished: true };
  }

  return { point: route[ai.currentIndex], hitWaypoint, finished: false };
}

// ── Default Checkpoint Routes ────────────────────────────────

export function generateDefaultRoute(center: GeoPoint, count = 5, spreadMeters = 200): GeoPoint[] {
  const points: GeoPoint[] = [center];
  for (let i = 1; i < count; i++) {
    const angle = (Math.PI * 2 * i) / count;
    const dist = spreadMeters * (0.6 + Math.random() * 0.8);
    const dLat = (dist / 111320) * Math.cos(angle);
    const dLon = (dist / (111320 * Math.cos(toRad(center.lat)))) * Math.sin(angle);
    points.push({ lat: center.lat + dLat, lon: center.lon + dLon });
  }
  return points;
}

export function routeToCheckpoints(route: GeoPoint[], startIndex = 0): Checkpoint[] {
  return route.slice(startIndex).map((p, i) => ({
    id: `cp_${i}`,
    point: p,
    radiusMeters: 8 + i * 0.5, // progressive: tighter early, looser late
    order: i,
  }));
}
