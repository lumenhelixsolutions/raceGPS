import './styles.css';
import { MockMapAdapter } from '@racegps/map-adapters';
import type { ServerMessage, RaceGPSMode, RaceLobby, RaceResult } from '@racegps/protocol';

const API = 'http://localhost:8787';
let ws: WebSocket | undefined;
let playerId = '';
let displayName = '';
let roomId = 'global-cruise';
let seq = 0;
let lat = 41.4993;
let lon = -81.6944;
const map = new MockMapAdapter();

// Race state
let currentLobbyId = '';
let raceActive = false;
let raceStartMs = 0;
let cpIndex = 0;
const totalCPs = 5; // matches default

document.querySelector<HTMLDivElement>('#app')!.innerHTML = `
  <main class="shell">
    <aside class="panel">
      <div class="brand">
        <img src="/assets/racegps-mark.svg" class="brand-mark" alt="raceGPS" />
        <div><h1>race<span>GPS</span></h1><p class="tagline">Real World Driving</p></div>
      </div>

      <section class="card" id="connectCard">
        <p class="status"><strong>One Earth.</strong> Many cities. Many rooms. One player identity. One global career.</p>
        <label>Display name</label>
        <input id="displayName" value="Raziel13" />
        <label>Room ID</label>
        <input id="roomId" value="global-cruise" />
        <label>Mode</label>
        <select id="mode">
          <option value="cruise">Cruise</option>
          <option value="race">Race</option>
          <option value="challenge">Challenge</option>
          <option value="hot_pursuit">Hot Pursuit</option>
          <option value="explore">Explore</option>
        </select>
        <div class="btn-row"><button id="connectBtn">Connect</button><button class="secondary" id="moveBtn">Move Marker</button></div>
      </section>

      <!-- Race Lobby Panel (hidden until race lobby active) -->
      <section class="card" id="lobbyCard" style="display:none">
        <h3>🏁 Race Lobby</h3>
        <div id="lobbyInfo"></div>
        <div id="lobbyPlayers" class="lobby-players"></div>
        <div class="btn-row" id="lobbyActions">
          <button id="readyBtn">Ready Up</button>
          <button class="secondary" id="startRaceBtn" disabled>Start Race</button>
        </div>
        <button class="secondary" id="leaveLobbyBtn" style="width:100%;margin-top:8px">Leave Lobby</button>
      </section>

      <!-- Race HUD (hidden until race active) -->
      <section class="card" id="raceHud" style="display:none">
        <h3>🏎️ Race</h3>
        <div class="race-timer" id="raceTimer">00:00.0</div>
        <div class="cp-bar"><div class="cp-fill" id="cpBar" style="width:0%"></div></div>
        <p class="status" id="cpText">Checkpoints: 0/5</p>
        <button class="secondary" id="finishBtn" style="width:100%;margin-top:8px">Cross Finish Line</button>
      </section>

      <!-- Race Results (hidden until race over) -->
      <section class="card" id="resultsCard" style="display:none">
        <h3>🏆 Results</h3>
        <div id="resultsList"></div>
        <button id="backToRoomBtn" style="width:100%;margin-top:8px">Back to Room</button>
      </section>

      <!-- Signal Challenge -->
      <section class="card">
        <h3>Signal Challenge</h3>
        <div class="signal-row">
          <button data-signal="flash_lights">Flash</button>
          <button data-signal="rev_engine">Rev</button>
          <button data-signal="drop_pin">Drop Pin</button>
          <button data-signal="hot_signal">Hot Signal</button>
          <button data-signal="ghost_signal">Ghost</button>
        </div>
      </section>

      <section class="card">
        <h3>Race Lobby</h3>
        <label>Lobby Name</label>
        <input id="lobbyName" value="Cleveland Sprint" />
        <button id="createLobbyBtn" style="width:100%;margin-top:8px">Create Race Lobby</button>
      </section>

      <section class="card">
        <h3>Mode Spine</h3>
        <p class="status">Cruise → signal → lobby → race/chase/challenge → score → share.</p>
      </section>
    </aside>

    <section class="map-wrap">
      <div id="map"></div>
      <!-- Countdown Overlay -->
      <div class="countdown-overlay" id="countdownOverlay" style="display:none">
        <div class="countdown-number" id="countdownNumber">5</div>
      </div>
      <div class="hud">
        <div class="hud-card"><div class="status" id="status">Disconnected. Connect to enter the raceGPS world.</div></div>
        <div class="hud-card">
          <div class="chat-log" id="chatLog"></div>
          <form class="chat-form" id="chatForm"><input id="chatInput" placeholder="room chat..." maxlength="240" /><button>Send</button></form>
        </div>
      </div>
    </section>
  </main>
`;

const mapEl = document.querySelector<HTMLElement>('#map')!;
map.mount(mapEl);
map.setCenter({ lat, lon });

const markImg = document.querySelector<HTMLImageElement>('.brand-mark');
if (markImg) markImg.onerror = () => markImg.style.display = 'none';

// ── Event Bindings ──────────────────────────────────────

document.querySelector('#connectBtn')!.addEventListener('click', connect);
document.querySelector('#moveBtn')!.addEventListener('click', () => sendPosition(true));
document.querySelector('#chatForm')!.addEventListener('submit', (e) => { e.preventDefault(); sendChat(); });
document.querySelectorAll<HTMLButtonElement>('[data-signal]').forEach(btn => btn.addEventListener('click', () => sendSignal(btn.dataset.signal!)));

// Race lobby
document.querySelector('#createLobbyBtn')!.addEventListener('click', createLobby);
document.querySelector('#readyBtn')!.addEventListener('click', toggleReady);
document.querySelector('#startRaceBtn')!.addEventListener('click', startRace);
document.querySelector('#leaveLobbyBtn')!.addEventListener('click', leaveLobby);
document.querySelector('#finishBtn')!.addEventListener('click', finishRace);
document.querySelector('#backToRoomBtn')!.addEventListener('click', backToRoom);

// ── Connection ──────────────────────────────────────────

async function connect(): Promise<void> {
  displayName = (document.querySelector<HTMLInputElement>('#displayName')!.value || 'Driver').trim();
  roomId = (document.querySelector<HTMLInputElement>('#roomId')!.value || 'global-cruise').trim();
  const mode = document.querySelector<HTMLSelectElement>('#mode')!.value as RaceGPSMode;

  const session = await fetch(`${API}/api/session`, {
    method: 'POST', credentials: 'include', headers: { 'content-type': 'application/json' }, body: JSON.stringify({ displayName })
  }).then(r => r.json());
  playerId = session.playerId;

  ws?.close();
  ws = new WebSocket(`ws://localhost:8787/ws/rooms/${roomId}`);
  ws.onopen = () => {
    status(`Connected as <span class="neon">${displayName}</span> in ${roomId}.`);
    ws?.send(JSON.stringify({ type: 'join_room', roomId, displayName, mode }));
    sendPosition(false);
    resetRaceUI();
  };
  ws.onmessage = ev => handleMessage(JSON.parse(ev.data));
  ws.onclose = () => status('Disconnected.');
}

function resetRaceUI(): void {
  currentLobbyId = '';
  raceActive = false;
  cpIndex = 0;
  document.querySelector<HTMLElement>('#lobbyCard')!.style.display = 'none';
  document.querySelector<HTMLElement>('#raceHud')!.style.display = 'none';
  document.querySelector<HTMLElement>('#resultsCard')!.style.display = 'none';
  document.querySelector<HTMLElement>('#countdownOverlay')!.style.display = 'none';
}

// ── Race Lobby Actions ──────────────────────────────────

function createLobby(): void {
  if (!ws || ws.readyState !== WebSocket.OPEN) return;
  const name = (document.querySelector<HTMLInputElement>('#lobbyName')!.value || 'Sprint Race').trim();
  ws.send(JSON.stringify({ type: 'race_create_lobby', roomId, title: name, checkpointIds: [], laps: 1, maxPlayers: 8 }));
}

function toggleReady(): void {
  if (!ws || !currentLobbyId) return;
  ws.send(JSON.stringify({ type: 'race_toggle_ready', roomId, lobbyId: currentLobbyId }));
  // Optimistic UI: toggle button text
  const btn = document.querySelector<HTMLButtonElement>('#readyBtn')!;
  btn.textContent = btn.textContent === 'Ready Up' ? 'Not Ready' : 'Ready Up';
}

function startRace(): void {
  if (!ws || !currentLobbyId) return;
  ws.send(JSON.stringify({ type: 'race_start_countdown', roomId, lobbyId: currentLobbyId }));
}

function leaveLobby(): void {
  if (!ws || !currentLobbyId) return;
  ws.send(JSON.stringify({ type: 'race_leave_lobby', roomId, lobbyId: currentLobbyId }));
  resetRaceUI();
}

function finishRace(): void {
  if (!ws || !currentLobbyId || !raceActive) return;
  const elapsed = Date.now() - raceStartMs;
  ws.send(JSON.stringify({ type: 'race_finished', roomId, lobbyId: currentLobbyId, elapsedMs: elapsed, ghost: [] }));
}

function backToRoom(): void {
  resetRaceUI();
}

// ── Message Handler ─────────────────────────────────────

function handleMessage(msg: ServerMessage): void {
  switch (msg.type) {
    case 'welcome': playerId = msg.playerId; break;
    case 'room_snapshot':
      chat(`[System] joined ${msg.room.title}. Players: ${msg.room.players.length}`);
      msg.room.players.forEach(p => { if (p.lat && p.lon) map.upsertPlayerMarker(p.playerId, { lat: p.lat, lon: p.lon }, p.displayName); });
      break;
    case 'player_joined': chat(`[System] ${msg.player.displayName} joined.`); break;
    case 'player_left': map.removePlayerMarker(msg.playerId); chat(`[System] player left.`); break;
    case 'player_position': map.upsertPlayerMarker(msg.playerId, { lat: msg.lat, lon: msg.lon }, msg.playerId === playerId ? displayName : msg.playerId); break;
    case 'chat_message': chat(`<strong>${msg.displayName}</strong>: ${escapeHtml(msg.message)}`); break;
    case 'challenge_signal': chat(`<span class="hot">[Signal]</span> ${msg.fromPlayerId} sent ${msg.signal} for ${msg.challengeType}.`); break;
    case 'system_message': chat(`[System] ${escapeHtml(msg.message)}`); break;
    case 'game_event': {
      if (msg.event.kind === 'checkpoint_hit' && raceActive) {
        cpIndex++;
        updateCPBar();
      }
      break;
    }
    // ── Race Lobby Messages ─────────────────────────────
    case 'race_lobby_snapshot':
    case 'race_lobby_updated':
      updateLobbyUI(msg.lobby);
      break;
    case 'race_countdown':
      showCountdown(msg.secondsRemaining);
      break;
    case 'race_started':
      raceActive = true;
      raceStartMs = msg.raceStart;
      cpIndex = 0;
      showRaceHUD();
      break;
    case 'race_player_finished':
      chat(`[Race] ${msg.result.displayName} finished in ${(msg.result.elapsedMs / 1000).toFixed(1)}s`);
      break;
    case 'race_results':
      showResults(msg.results, msg.winner);
      break;
  }
}

// ── Lobby UI ────────────────────────────────────────────

function updateLobbyUI(lobby: RaceLobby): void {
  currentLobbyId = lobby.lobbyId;
  document.querySelector<HTMLElement>('#lobbyCard')!.style.display = 'block';

  const info = document.querySelector<HTMLDivElement>('#lobbyInfo')!;
  info.innerHTML = `<p><strong>${escapeHtml(lobby.title)}</strong> — ${lobby.state === 'lobby' ? 'Waiting for players' : lobby.state === 'countdown' ? 'Starting...' : lobby.state === 'racing' ? 'Racing!' : 'Finished'}</p>`;

  const playersEl = document.querySelector<HTMLDivElement>('#lobbyPlayers')!;
  playersEl.innerHTML = lobby.players.map(p =>
    `<div class="lobby-player ${p.ready ? 'ready' : ''}">
      <span class="player-name">${escapeHtml(p.displayName)}</span>
      <span class="player-ready">${p.ready ? '✅' : '⏳'}</span>
    </div>`
  ).join('');

  // Enable start button if all ready (host check — first player)
  const allReady = lobby.players.length > 0 && lobby.players.every(p => p.ready);
  const startBtn = document.querySelector<HTMLButtonElement>('#startRaceBtn')!;
  startBtn.disabled = !allReady || lobby.state !== 'lobby';
}

// ── Countdown Overlay ───────────────────────────────────

function showCountdown(seconds: number): void {
  const overlay = document.querySelector<HTMLElement>('#countdownOverlay')!;
  const num = document.querySelector<HTMLDivElement>('#countdownNumber')!;
  overlay.style.display = 'flex';
  if (seconds === 0) {
    num.textContent = 'GO!';
    num.className = 'countdown-number go';
    setTimeout(() => { overlay.style.display = 'none'; }, 800);
  } else {
    num.textContent = String(seconds);
    num.className = 'countdown-number';
  }
}

// ── Race HUD ────────────────────────────────────────────

function showRaceHUD(): void {
  document.querySelector<HTMLElement>('#lobbyCard')!.style.display = 'none';
  document.querySelector<HTMLElement>('#raceHud')!.style.display = 'block';
  document.querySelector<HTMLElement>('#resultsCard')!.style.display = 'none';
  updateCPBar();
  startRaceTimer();
}

function updateCPBar(): void {
  const pct = Math.min(100, (cpIndex / totalCPs) * 100);
  document.querySelector<HTMLElement>('#cpBar')!.style.width = `${pct}%`;
  document.querySelector<HTMLElement>('#cpText')!.textContent = `Checkpoints: ${cpIndex}/${totalCPs}`;
}

let raceTimerInterval: ReturnType<typeof setInterval> | undefined;
function startRaceTimer(): void {
  if (raceTimerInterval) clearInterval(raceTimerInterval);
  raceTimerInterval = setInterval(() => {
    if (!raceActive) { clearInterval(raceTimerInterval); return; }
    const elapsed = Date.now() - raceStartMs;
    const totalSec = (elapsed / 1000).toFixed(1);
    document.querySelector<HTMLElement>('#raceTimer')!.textContent = `${totalSec}s`;
  }, 100);
}

// ── Results ─────────────────────────────────────────────

function showResults(results: RaceResult[], winner: RaceResult): void {
  raceActive = false;
  if (raceTimerInterval) clearInterval(raceTimerInterval);
  document.querySelector<HTMLElement>('#raceHud')!.style.display = 'none';
  document.querySelector<HTMLElement>('#lobbyCard')!.style.display = 'none';
  document.querySelector<HTMLElement>('#resultsCard')!.style.display = 'block';

  const list = document.querySelector<HTMLDivElement>('#resultsList')!;
  list.innerHTML = `
    <div class="winner-banner">🏆 ${escapeHtml(winner.displayName)} — ${(winner.elapsedMs / 1000).toFixed(1)}s</div>
    ${results.map((r, i) => `
      <div class="result-row ${r.playerId === playerId ? 'me' : ''}">
        <span class="result-pos">${i + 1}.</span>
        <span class="result-name">${escapeHtml(r.displayName)}</span>
        <span class="result-time">${(r.elapsedMs / 1000).toFixed(1)}s</span>
      </div>
    `).join('')}
  `;
}

// ── Send Functions ──────────────────────────────────────

async function sendPosition(randomize: boolean): Promise<void> {
  if (!ws || ws.readyState !== WebSocket.OPEN) return;
  if (randomize) { lat += (Math.random() - .5) * .01; lon += (Math.random() - .5) * .01; }
  map.upsertPlayerMarker(playerId || 'me', { lat, lon }, displayName || 'ME');
  seq++;
  ws.send(JSON.stringify({ type: 'player_position', roomId, lat, lon, heading: Math.random() * 360, speed: Math.random() * 80, seq }));

  // Auto-hit checkpoint if in race and near one
  if (raceActive && currentLobbyId && cpIndex < totalCPs) {
    ws.send(JSON.stringify({ type: 'race_checkpoint_hit', roomId, lobbyId: currentLobbyId, checkpointId: `cp_${cpIndex}`, seq }));
  }
}

function sendChat(): void {
  const input = document.querySelector<HTMLInputElement>('#chatInput')!;
  const message = input.value.trim();
  if (!message || !ws || ws.readyState !== WebSocket.OPEN) return;
  ws.send(JSON.stringify({ type: 'chat_message', roomId, message }));
  input.value = '';
}

function sendSignal(signal: string): void {
  if (!ws || ws.readyState !== WebSocket.OPEN) return;
  ws.send(JSON.stringify({ type: 'challenge_signal', roomId, signal, challengeType: signal === 'hot_signal' ? 'hot_pursuit' : 'quick_sprint' }));
}

// ── Utilities ───────────────────────────────────────────

function chat(html: string): void {
  const log = document.querySelector<HTMLDivElement>('#chatLog')!;
  const line = document.createElement('div');
  line.className = 'chat-line';
  line.innerHTML = html;
  log.appendChild(line);
  log.scrollTop = log.scrollHeight;
}

function status(html: string): void { document.querySelector('#status')!.innerHTML = html; }
function escapeHtml(s: string): string { return s.replace(/[&<>\"]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','\"':'&quot;'}[c]!)); }
