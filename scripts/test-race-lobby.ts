// Race lobby conformance test — with message buffering
import WebSocket from 'ws';

const API = 'http://localhost:8787';
const WS_BASE = 'ws://localhost:8787/ws/rooms';

async function createSession(name: string) {
  const res = await fetch(`${API}/api/session`, {
    method: 'POST', headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ displayName: name }),
  });
  const session = await res.json();
  const match = (res.headers.get('set-cookie') || '').match(/racegps_sid=([^;]+)/);
  return { session, sid: match ? match[1] : '' };
}

async function createRoom() {
  const res = await fetch(`${API}/api/rooms`, {
    method: 'POST', headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ title: 'Test Race Room', mode: 'race' }),
  });
  return res.json();
}

// WS wrapper that buffers all incoming messages
class BufferedWS {
  ws: WebSocket;
  buffer: any[] = [];
  constructor(roomId: string, sid: string) {
    this.ws = new WebSocket(`${WS_BASE}/${roomId}`, {
      headers: { Cookie: `racegps_sid=${sid}` },
    });
    this.ws.on('message', data => {
      try { this.buffer.push(JSON.parse(data.toString())); } catch {}
    });
  }

  waitOpen(timeoutMs = 5000): Promise<void> {
    return new Promise((resolve, reject) => {
      const t = setTimeout(() => reject(new Error('WS open timeout')), timeoutMs);
      this.ws.on('open', () => { clearTimeout(t); resolve(); });
      this.ws.on('error', e => { clearTimeout(t); reject(e); });
    });
  }

  // Wait for a specific message type, checking buffer and new messages
  waitFor(type: string, timeoutMs = 5000): Promise<any> {
    // Check buffer first
    const idx = this.buffer.findIndex(m => m.type === type);
    if (idx >= 0) {
      const msg = this.buffer[idx];
      this.buffer = this.buffer.filter((_, i) => i !== idx);
      return Promise.resolve(msg);
    }
    return new Promise((resolve, reject) => {
      const t = setTimeout(() => reject(new Error(`timeout waiting for ${type}`)), timeoutMs);
      const handler = (data: WebSocket.Data) => {
        try {
          const msg = JSON.parse(data.toString());
          if (msg.type === type) { clearTimeout(t); this.ws.off('message', handler); resolve(msg); }
        } catch {}
      };
      this.ws.on('message', handler);
    });
  }

  send(obj: any) { this.ws.send(JSON.stringify(obj)); }
  close() { this.ws.close(); }
}

async function main() {
  console.log('=== raceGPS Race Lobby Test ===\n');

  const p1 = await createSession('RacerX');
  const p2 = await createSession('SpeedDemon');
  const room = await createRoom();
  console.log(`Room: ${room.roomId} | P1: ${p1.session.playerId} | P2: ${p2.session.playerId}\n`);

  const ws1 = new BufferedWS(room.roomId, p1.sid);
  const ws2 = new BufferedWS(room.roomId, p2.sid);
  await Promise.all([ws1.waitOpen(), ws2.waitOpen()]);

  await Promise.all([ws1.waitFor('room_snapshot'), ws2.waitFor('room_snapshot')]);
  console.log('✅ Both connected\n');

  // Create lobby
  ws1.send({ type: 'race_create_lobby', roomId: room.roomId, title: 'Cleveland Sprint', checkpointIds: [], laps: 1, maxPlayers: 8 });
  const snap = await ws1.waitFor('race_lobby_snapshot');
  const lid = snap.lobby.lobbyId;
  console.log(`✅ Lobby: ${lid} (${snap.lobby.checkpointIds.length} CPs)\n`);

  // P2 joins
  ws2.send({ type: 'race_join_lobby', roomId: room.roomId, lobbyId: lid });
  await ws2.waitFor('race_lobby_snapshot');
  await ws1.waitFor('race_lobby_updated');
  console.log('✅ P2 joined\n');

  // Ready up
  ws1.send({ type: 'race_toggle_ready', roomId: room.roomId, lobbyId: lid });
  await ws1.waitFor('race_lobby_updated');
  ws2.send({ type: 'race_toggle_ready', roomId: room.roomId, lobbyId: lid });
  await ws1.waitFor('race_lobby_updated');
  console.log('✅ Both ready\n');

  // Countdown → race start
  ws1.send({ type: 'race_start_countdown', roomId: room.roomId, lobbyId: lid });
  let started = false;
  ws1.ws.on('message', function h(d) {
    try {
      const m = JSON.parse(d.toString());
      if (m.type === 'race_countdown') console.log(`⏱  ${m.secondsRemaining}`);
      if (m.type === 'race_started') { console.log(`🏁 RACE STARTED\n`); started = true; ws1.ws.off('message', h); }
    } catch {}
  });
  while (!started) await new Promise(r => setTimeout(r, 100));

  // P1 hits checkpoints
  for (let i = 0; i < 5; i++) {
    ws1.send({ type: 'race_checkpoint_hit', roomId: room.roomId, lobbyId: lid, checkpointId: `cp_${i}`, seq: i + 1 });
    await new Promise(r => setTimeout(r, 80));
  }
  console.log('✅ P1 hit 5 CPs\n');

  // P1 finishes
  ws1.send({ type: 'race_finished', roomId: room.roomId, lobbyId: lid, elapsedMs: 32000, ghost: [1, 2, 3] });
  const f1 = await ws1.waitFor('race_player_finished');
  console.log(`✅ P1 finished: ${f1.result.elapsedMs}ms (order ${f1.result.finishOrder})\n`);

  // P2 finishes
  ws2.send({ type: 'race_finished', roomId: room.roomId, lobbyId: lid, elapsedMs: 28700, ghost: [1, 2] });
  await ws1.waitFor('race_player_finished');

  // Results
  const results = await ws1.waitFor('race_results');
  console.log(`🏆 Winner: ${results.winner.displayName} — ${(results.winner.elapsedMs/1000).toFixed(1)}s`);
  results.results.forEach((r: any, i: number) => console.log(`  ${i+1}. ${r.displayName} ${(r.elapsedMs/1000).toFixed(1)}s (${r.checkpointsHit}/${r.totalCheckpoints} CPs)`));

  // Cleanup
  await new Promise(r => setTimeout(r, 200));
  ws1.close(); ws2.close();

  console.log('\n🎉 ALL TESTS PASSED — Race lobby + countdown + checkpoints + finish flow works!');
  process.exit(0);
}

main().catch(e => { console.error('\n❌ FAIL:', e.message); process.exit(1); });
