# VexConnect Protocol Specification

## Overview

VexConnect is a BLE mesh relay that turns every phone into a packet router. The protocol ensures packets propagate through the mesh without loops, with end-to-end encryption, and minimal battery impact.

---

## Packet Structure

```
┌─────────────────────────────────────────────────────────────┐
│ VexConnect Packet (max 512 bytes)                           │
├──────────┬──────────┬──────────┬──────────┬─────────────────┤
│ Version  │ PacketID │   TTL    │  Flags   │    Payload      │
│ (1 byte) │ (8 bytes)│ (1 byte) │ (1 byte) │  (up to 501)    │
└──────────┴──────────┴──────────┴──────────┴─────────────────┘
```

### Fields

| Field | Size | Description |
|-------|------|-------------|
| **Version** | 1 byte | Protocol version (currently `0x01`) |
| **PacketID** | 8 bytes | Random unique ID (first 8 bytes of sha256 hash of payload + timestamp + sender nonce) |
| **TTL** | 1 byte | Hops remaining. Starts at 7, decrements each hop. Drop at 0. |
| **Flags** | 1 byte | Bit flags (see below) |
| **Payload** | ≤501 bytes | Encrypted application data |

### Flags

| Bit | Name | Description |
|-----|------|-------------|
| 0 | `ENCRYPTED` | Payload is encrypted (always 1 for now) |
| 1 | `BROADCAST` | No specific recipient (flood to all) |
| 2 | `ACK_REQUESTED` | Sender wants delivery confirmation |
| 3-7 | Reserved | Must be 0 |

---

## Deduplication

Every node maintains a **seen-packet cache**:

```
SeenCache = Map<PacketID, timestamp>
```

Rules:
1. On receive: if `PacketID` in cache → **drop** (already relayed)
2. On receive: if `PacketID` not in cache → **relay** and add to cache
3. Cache entries expire after **60 seconds**
4. Max cache size: **1000 entries** (LRU eviction)

This prevents infinite loops. A packet can only traverse each node once.

---

## TTL (Time To Live)

- New packets start with `TTL = 7`
- Each relay decrements TTL by 1
- If `TTL = 0` after decrement → drop, do not relay
- Maximum mesh diameter: 7 hops (covers ~700m in dense urban, several km in open areas)

---

## BLE Architecture

### Dual Role: Peripheral + Central

Every VexConnect node operates in **both modes simultaneously**:

**Peripheral (Advertiser)**
- Advertises service UUID `0000VC01-...`
- Accepts incoming connections
- Exposes GATT characteristics for packet exchange

**Central (Scanner)**
- Scans for other VexConnect peripherals
- Initiates connections to discovered nodes
- Reads/writes packets via GATT

### GATT Service

```
Service: 0000VC01-0000-1000-8000-00805F9B34FB (VexConnect)
├── Characteristic: 0000VC02-... (TX - Write)
│   └── Properties: Write, Write Without Response
│   └── Description: Send packets TO this node
│
├── Characteristic: 0000VC03-... (RX - Notify)
│   └── Properties: Read, Notify
│   └── Description: Receive packets FROM this node
│
└── Characteristic: 0000VC04-... (Stats - Read)
    └── Properties: Read
    └── Description: Node stats (uptime, relayed count)
```

### Connection Management

BLE limits: ~5-7 concurrent connections per device.

Strategy:
1. **Connection pool**: Max 5 active connections
2. **Round-robin**: Cycle through discovered devices
3. **Priority**: Prefer nodes with strong RSSI (closer = more reliable)
4. **Reconnect backoff**: Failed connections wait 30s before retry

---

## Encryption

All payloads are encrypted with **NaCl box** (X25519 + XSalsa20 + Poly1305).

### Key Management

Each node has:
- **Identity keypair**: Long-term Ed25519 for signing
- **Ephemeral keypair**: Short-term X25519 for encryption (rotated hourly)

### Broadcast Encryption

For mesh-wide broadcasts (most traffic):
- Use a **shared mesh key** derived from the service UUID
- All nodes can decrypt broadcast traffic
- This enables relay without knowing content (nodes decrypt, verify, re-encrypt)

For private messages:
- Use recipient's public key (out of scope for v0.1)

### Payload Encryption

```
EncryptedPayload = nonce (24 bytes) || ciphertext
```

---

## Relay Algorithm

```
function onPacketReceived(packet, sourceDeviceId):
    // 1. Parse header
    version, packetId, ttl, flags, payload = parse(packet)
    
    // 2. Version check
    if version != 0x01:
        return  // Unknown protocol
    
    // 3. Deduplication
    if seenCache.has(packetId):
        return  // Already relayed
    seenCache.set(packetId, now())
    
    // 4. TTL check
    if ttl <= 1:
        return  // End of the line
    
    // 5. Decrement TTL
    newTtl = ttl - 1
    
    // 6. Rebuild packet with new TTL
    relayPacket = buildPacket(version, packetId, newTtl, flags, payload)
    
    // 7. Forward to all connected nodes except source
    for device in connectedDevices:
        if device.id != sourceDeviceId:
            device.write(TX_CHARACTERISTIC, relayPacket)
    
    // 8. Update stats
    stats.packetsRelayed++
```

---

## Battery Optimization

### Scan Duty Cycling

Instead of continuous scanning:

| State | Scan Duration | Interval | When |
|-------|---------------|----------|------|
| **Active** | 5s on, 10s off | 15s cycle | Mesh enabled, app in foreground |
| **Background** | 2s on, 30s off | 32s cycle | App backgrounded |
| **Low Power** | 2s on, 60s off | 62s cycle | Battery < 20% |

### Connection Keepalive

- Ping connected devices every 30s
- Disconnect idle connections after 2 minutes
- Reconnect on demand when packets need relaying

---

## Platform Considerations

### Android

- Requires `BLUETOOTH_SCAN`, `BLUETOOTH_CONNECT`, `BLUETOOTH_ADVERTISE`, `ACCESS_FINE_LOCATION`
- Use **Foreground Service** with persistent notification for background operation
- `android:foregroundServiceType="connectedDevice"`

### iOS

- Requires `NSBluetoothAlwaysUsageDescription`
- Background modes: `bluetooth-central`, `bluetooth-peripheral`
- iOS aggressively throttles background BLE - expect reduced throughput
- Use **State Restoration** for connection persistence across app kills

---

## Security Considerations

1. **No metadata leakage**: PacketIDs are random, not derived from content
2. **No sender identification**: Packets don't include source addresses
3. **Plausible deniability**: Every node relays every packet - no way to prove origin
4. **Forward secrecy**: Ephemeral keys rotated hourly
5. **No logs**: Nodes don't store packet contents or routing history

---

## Future Extensions

- **Mesh routing tables**: For directed messages (not just flood)
- **Packet fragmentation**: For payloads > 512 bytes
- **QoS priorities**: Urgent packets get precedence
- **Incentive layer**: Token rewards for relay participation
- **Bridge to LoRa**: Phone → BLE → Pi → LoRa → distant mesh

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 0.1 | 2026-02-17 | Initial protocol spec |
