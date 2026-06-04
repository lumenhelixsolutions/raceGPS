import WebSocket from 'ws';
const API = 'http://localhost:8787';

async function main() {
  const r = await fetch(`${API}/api/session`, {
    method: 'POST', headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ displayName: 'HumanRacer' }),
  });
  const sid = (r.headers.get('set-cookie') || '').match(/racegps_sid=([^;]+)/)?.[1] || '';
  const room = await (await fetch(`${API}/api/rooms`, {
    method: 'POST', headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ title: 'AI Test', mode: 'race' }),
  })).json();

  const msgs: any[] = [];
  const ws = new WebSocket(`ws://localhost:8787/ws/rooms/${room.roomId}`, {
    headers: { Cookie: `racegps_sid=${sid}` },
  });
  ws.on('message', d => { try { msgs.push(JSON.parse(d.toString())); } catch {} });

  await new Promise<void>(r => ws.on('open', r));
  await new Promise(r => setTimeout(r, 400));

  // Create lobby → AI auto-spawn
  ws.send(JSON.stringify({ type: 'race_create_lobby', roomId: room.roomId, title: 'AI Sprint', checkpointIds: [], laps: 1, maxPlayers: 8 }));
  await new Promise(r => setTimeout(r, 400));

  const snap = msgs.find(m => m.type === 'race_lobby_snapshot')!;
  const lid = snap.lobby.lobbyId;
  console.log(`Lobby: ${lid} — ${snap.lobby.players.length} drivers`);
  snap.lobby.players.forEach((p: any) => console.log(`  ${p.displayName} ready=${p.ready}`));

  // Ready up and start
  ws.send(JSON.stringify({ type: 'race_toggle_ready', roomId: room.roomId, lobbyId: lid }));
  await new Promise(r => setTimeout(r, 200));
  ws.send(JSON.stringify({ type: 'race_start_countdown', roomId: room.roomId, lobbyId: lid }));

  // Wait for race_started
  await new Promise<void>((resolve) => {
    ws.on('message', function h(d) {
      const m = JSON.parse(d.toString());
      if (m.type === 'race_started') { ws.off('message', h); resolve(); }
    });
  });
  console.log('Race started!\n');

  // Let AI race for ~12s then human finishes
  await new Promise(r => setTimeout(r, 12000));
  ws.send(JSON.stringify({ type: 'race_finished', roomId: room.roomId, lobbyId: lid, elapsedMs: 12000, ghost: [] }));

  // Wait for results
  const results = await new Promise<any>((resolve, reject) => {
    const t = setTimeout(() => reject(new Error('timeout waiting for results')), 15000);
    ws.on('message', function h(d) {
      const m = JSON.parse(d.toString());
      if (m.type === 'race_results') { clearTimeout(t); ws.off('message', h); resolve(m); }
    });
  });

  console.log(`\n=== RESULTS (${results.results.length} drivers) ===`);
  results.results.forEach((r: any, i: number) => {
    const icon = r.playerId.startsWith('ai_') ? '🤖' : '👤';
    console.log(`  ${i+1}. ${icon} ${r.displayName} ${(r.elapsedMs/1000).toFixed(1)}s (${r.checkpointsHit}/${r.totalCheckpoints} CPs)`);
  });

  ws.close();
  process.exit(0);
}

main().catch(e => { console.error('FAIL:', e.message); process.exit(1); });
