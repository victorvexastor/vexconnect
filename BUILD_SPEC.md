# VexConnect — Build Specification

## What This Is

A single portable binary that turns any device into a mesh network node. No install, no dependencies, no account, no internet. Download → run → you're on the mesh.

Inspired by [Justine Tunney's](https://justine.lol/) approach: one binary, runs everywhere.

## What It Does

```
$ ./vexconnect
[VexConnect v0.1] Node online
[BLE] Scanning... found 3 peers
[BLE] Connected: 2 peers
[MESH] Relaying packets (0 sent, 0 received)
[MESH] Ready. Messages encrypted end-to-end.
> hello from this node
[MESH] Sent → 3 hops, 2.1s delivery
```

A phone or computer running VexConnect becomes a mesh relay. Messages hop from device to device via Bluetooth Low Energy. No tower, no ISP, no server. Works in a field, a building, a community — anywhere devices are in range.

## Architecture

```
┌─────────────────────────────────────────────┐
│                 vexconnect                   │
├──────────┬──────────┬──────────┬────────────┤
│   CLI    │  Mesh    │   BLE    │   Crypto   │
│  (REPL)  │ (Router) │ (Radio)  │  (NaCl)    │
├──────────┴──────────┴──────────┴────────────┤
│              Platform Layer                  │
│   Linux: BlueZ/D-Bus                        │
│   macOS: CoreBluetooth (Obj-C bridge)       │
│   Windows: WinRT BLE                        │
│   Pi: BlueZ + LoRa SPI (optional)           │
└─────────────────────────────────────────────┘
```

### Components

| Component | Responsibility | Lines (est.) |
|-----------|---------------|-------------|
| **main.c** | Entry point, arg parsing, REPL | ~200 |
| **mesh.c** | Packet routing, dedup cache, TTL | ~400 |
| **ble_linux.c** | BlueZ/D-Bus BLE central+peripheral | ~600 |
| **ble_macos.m** | CoreBluetooth bridge | ~500 |
| **crypto.c** | NaCl box (X25519 + XSalsa20 + Poly1305) | ~200 |
| **packet.c** | Packet encoding/decoding | ~150 |
| **seen.c** | LRU dedup cache (1000 entries, 60s TTL) | ~100 |
| **lora.c** | Optional LoRa SPI driver for Pi | ~300 |
| **Total** | | **~2,450** |

This is a small project. A good C dev builds this in 2-4 weeks.

## Packet Format

```
┌──────────┬──────────┬──────────┬──────────┬─────────────────┐
│ Version  │ PacketID │   TTL    │  Flags   │    Payload      │
│ (1 byte) │ (8 bytes)│ (1 byte) │ (1 byte) │  (up to 501)    │
└──────────┴──────────┴──────────┴──────────┴─────────────────┘
Max packet: 512 bytes
```

| Field | Size | Notes |
|-------|------|-------|
| Version | 1B | `0x01` |
| PacketID | 8B | SHA-256(payload + timestamp + nonce)[:8] |
| TTL | 1B | Starts at 7, decrements per hop |
| Flags | 1B | bit0=encrypted, bit1=broadcast, bit2=ack_requested |
| Payload | ≤501B | NaCl encrypted |

## Deduplication

```c
// LRU cache, 1000 entries, 60 second expiry
typedef struct {
    uint8_t  packet_id[8];
    uint64_t timestamp;
} seen_entry_t;

// On receive:
// 1. packet_id in cache? → DROP
// 2. Not in cache? → ADD to cache, RELAY to all peers except source
// 3. Evict entries older than 60s
```

## BLE GATT Service

```
Service UUID: 0000VC01-0000-1000-8000-00805F9B34FB

Characteristics:
  TX (Write):       0000VC02-...  → Send packets TO this node
  RX (Notify):      0000VC03-...  → Receive packets FROM this node  
  Stats (Read):     0000VC04-...  → Uptime, packets relayed
```

Every node is simultaneously:
- **Peripheral** (advertises, accepts connections)
- **Central** (scans, initiates connections)

Max 5 concurrent BLE connections. Round-robin discovery. Prefer strongest RSSI.

## Encryption

- **Identity**: Ed25519 keypair (long-term, generated on first run, saved to `~/.vexconnect/identity.key`)
- **Ephemeral**: X25519 keypair (rotated hourly)
- **Broadcast**: Shared mesh key derived from service UUID (all nodes can relay)
- **Private messages**: Recipient's public key (future)
- **Library**: TweetNaCl (single C file, public domain, ~700 lines)

## Build Targets

### Priority 1: Linux (Pi + Desktop)
```bash
# Dependencies: libdbus-1-dev, libbluetooth-dev
gcc -O2 -o vexconnect main.c mesh.c ble_linux.c crypto.c packet.c seen.c \
    -ldbus-1 -lbluetooth -lm
```

### Priority 2: macOS
```bash
# Uses CoreBluetooth framework (no deps)
clang -O2 -o vexconnect main.c mesh.c ble_macos.m crypto.c packet.c seen.c \
    -framework CoreBluetooth -framework Foundation
```

### Priority 3: Cosmopolitan (Actually Portable Executable)
```bash
# One binary that runs on Linux + Mac + Windows + FreeBSD
cosmocc -O2 -o vexconnect.com main.c mesh.c crypto.c packet.c seen.c \
    # BLE abstraction layer TBD
```

### Priority 4: Pi + LoRa
```bash
# Adds SPI-based LoRa radio support
gcc -O2 -o vexconnect main.c mesh.c ble_linux.c crypto.c packet.c seen.c lora.c \
    -ldbus-1 -lbluetooth -lm
```

## Runtime

```
~/.vexconnect/
├── identity.key          # Ed25519 keypair (generated on first run)
├── ephemeral.key         # X25519 keypair (rotated hourly)
└── config.txt            # Optional: node name, TTL, scan interval
```

No database. No log files. No state beyond keys.

## CLI Interface

```
$ ./vexconnect --help
VexConnect v0.1 — Mesh relay node

Usage: vexconnect [options]

Options:
  --name NAME       Node display name (default: random)
  --ttl N           Default TTL for outgoing packets (default: 7)
  --scan-interval S Seconds between BLE scans (default: 15)
  --lora            Enable LoRa radio (Pi with SPI hat)
  --no-relay        Receive only, don't relay (save battery)
  --stats           Print relay statistics every 30s
  --version         Print version and exit

Interactive commands:
  send <message>    Broadcast a message to the mesh
  peers             List connected peers
  stats             Show relay statistics  
  quit              Exit
```

## Security Properties

| Property | How |
|----------|-----|
| **No metadata leakage** | PacketIDs are random hashes |
| **No sender identification** | Packets contain no source address |
| **Plausible deniability** | Every node relays everything — can't prove origin |
| **Forward secrecy** | Ephemeral keys rotate hourly |
| **No logs** | No packet contents or routing history stored |
| **No accounts** | Identity is a keypair on your device |
| **No internet** | Runs entirely on BLE/LoRa |

## Dependencies

| Dependency | What | Size | Licence |
|-----------|------|------|---------|
| TweetNaCl | Crypto (NaCl box, sign) | 700 lines, 1 file | Public domain |
| BlueZ (Linux) | BLE stack | System library | GPL-2.0 |
| D-Bus (Linux) | BlueZ communication | System library | AFL-2.1/GPL-2.0 |
| CoreBluetooth (macOS) | BLE stack | System framework | Apple |

**Zero vendored dependencies beyond TweetNaCl.** Everything else is OS-provided.

## Testing Without Hardware

```bash
# Two terminals, simulated mesh via Unix sockets
$ ./vexconnect --transport=unix --socket=/tmp/vex1.sock
$ ./vexconnect --transport=unix --socket=/tmp/vex2.sock
```

Unix socket transport for development. Same mesh logic, no Bluetooth needed.

## What $2,000 Builds

| Item | Cost |
|------|------|
| 10× Raspberry Pi Zero 2 W | $150 |
| 10× LoRa SX1276 module | $100 |
| 10× Solar panel + battery | $250 |
| 10× Weatherproof case | $80 |
| 10× SD card (32GB) | $80 |
| Dev time (C programmer, 80hrs @ $15/hr) | $1,200 |
| **Total** | **$1,860** |

Covers: 10-node mesh network + the binary that runs it. 150km² coverage between two communities. The phones in people's pockets extend it for free.

## What Success Looks Like

Two communities, 20km apart. Each has 5 mesh nodes (solar, weatherproof, on poles/rooftops). Community members have the app on their phones. 

Someone at Community A sends a message. It hops: phone → BLE → Pi node → LoRa (20km) → Pi node → BLE → phone at Community B.

3 seconds. No internet. No tower. No bill. No log.

Forever.

## Links

- **Live site**: [vexconnect.pages.dev](https://vexconnect.pages.dev)
- **Protocol spec**: [PROTOCOL.md](./PROTOCOL.md)
- **Source**: [github.com/victorvexastor/vexconnect](https://github.com/victorvexastor/vexconnect)
- **Freedom Directory**: [FREEDOM_DIRECTORY.md](./FREEDOM_DIRECTORY.md)
- **Contact**: claw@omxus.com
