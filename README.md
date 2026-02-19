# VexConnect

> Free internet for everyone. Your phone is the network.

[![Live](https://img.shields.io/badge/LIVE-vexconnect.pages.dev-0EFFAF?style=flat-square&labelColor=0A0A0A)](https://vexconnect.pages.dev)

---

## What is this

VexConnect is a BLE mesh relay protocol that turns every phone into a packet router. When the internet goes down — or never existed — VexConnect keeps people connected.

No infrastructure. No ISP. No permission needed.

Every device that runs VexConnect extends the network for everyone around it.

---

## How it works

```
Phone A ──BLE──▶ Phone B ──BLE──▶ Phone C ──WiFi──▶ Internet
                     ↑
               also relaying
               to Phone D, E...
```

**Packet structure:**
```
┌──────────┬──────────┬──────────┬──────────┬─────────────────┐
│ Version  │ PacketID │   TTL    │  Flags   │    Payload      │
│ (1 byte) │ (8 bytes)│ (1 byte) │ (1 byte) │  (up to 501B)   │
└──────────┴──────────┴──────────┴──────────┴─────────────────┘
```

- **TTL starts at 7** — packets hop up to 7 nodes before expiring
- **Deduplication** — each node caches seen PacketIDs (60s window, 1000 max) to prevent loops
- **E2E encrypted** — payload is always encrypted, flag always set
- **Flood + ACK** — broadcast mode floods the mesh; ACK_REQUESTED asks for delivery confirmation

---

## Protocol principles

1. Every packet has a unique ID (8 bytes of sha256(payload + timestamp + nonce))
2. Seen-packet cache prevents infinite relay loops
3. TTL=0 means drop — no zombies
4. Max packet size: 512 bytes (BLE advertisement limits)
5. Battery-conscious: relay only when in range, sleep otherwise

---

## Files

| File | Description |
|------|-------------|
| `index.html` | Manifesto + live demo |
| `PROTOCOL.md` | Full protocol specification |
| `FREEDOM_DIRECTORY.md` | Community resources |
| `app/` | Application code |

---

## Status

🟢 Manifesto live  
📡 Protocol spec complete  
📱 Mobile app: in development
