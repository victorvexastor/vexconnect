#!/usr/bin/env node
// VexConnect WebSocket Relay Server
// Simulates the radio layer — broadcasts packets to all connected peers
// Usage: node relay-server.js [port]

const { WebSocketServer } = require('ws');
const port = parseInt(process.argv[2]) || 7850;

const wss = new WebSocketServer({ port, host: '0.0.0.0' });
const clients = new Map(); // ws -> {id, joined}
let nextId = 1;

wss.on('connection', (ws, req) => {
  const id = `node-${nextId++}`;
  const ip = req.socket.remoteAddress;
  clients.set(ws, { id, joined: Date.now(), ip });
  
  console.log(`[+] ${id} connected from ${ip} (${clients.size} total)`);
  
  // Tell the new client their ID and peer count
  ws.send(JSON.stringify({ type: 'welcome', id, peers: clients.size - 1 }));
  
  // Tell everyone about the new peer
  broadcast(JSON.stringify({ type: 'peer-joined', id, total: clients.size }), ws);
  
  ws.on('message', (data) => {
    const info = clients.get(ws);
    
    // Binary = mesh packet, broadcast to all others
    if (Buffer.isBuffer(data) || data instanceof ArrayBuffer) {
      const buf = Buffer.from(data);
      if (buf.length >= 11 && buf[0] === 0x01) {
        const pktId = buf.slice(1, 9).toString('hex').substring(0, 8);
        const ttl = buf[9];
        const type = buf[10];
        console.log(`[📦] ${info.id} → packet ${pktId}… TTL:${ttl} type:0x${type.toString(16).padStart(2,'0')} (${buf.length}B)`);
      } else {
        console.log(`[📦] ${info.id} → raw ${buf.length}B`);
      }
      
      let relayed = 0;
      for (const [peer, peerInfo] of clients) {
        if (peer !== ws && peer.readyState === 1) {
          peer.send(data);
          relayed++;
        }
      }
      console.log(`    ↗ relayed to ${relayed} peer(s)`);
      
    } else {
      // Text = control message
      try {
        const msg = JSON.parse(data);
        if (msg.type === 'ping') {
          ws.send(JSON.stringify({ type: 'pong', peers: clients.size - 1 }));
        }
      } catch(e) {}
    }
  });
  
  ws.on('close', () => {
    const info = clients.get(ws);
    clients.delete(ws);
    console.log(`[-] ${info.id} disconnected (${clients.size} remaining)`);
    broadcast(JSON.stringify({ type: 'peer-left', id: info.id, total: clients.size }));
  });
  
  ws.on('error', (err) => {
    console.error(`[!] ${clients.get(ws)?.id}: ${err.message}`);
  });
});

function broadcast(data, exclude) {
  for (const [ws] of clients) {
    if (ws !== exclude && ws.readyState === 1) ws.send(data);
  }
}

console.log(`\n  🐾 VexConnect Relay Server`);
console.log(`  ─────────────────────────`);
console.log(`  Listening on 0.0.0.0:${port}`);
console.log(`  LAN: ws://192.168.1.8:${port}`);
console.log(`  Open demo.html on any device to connect\n`);
