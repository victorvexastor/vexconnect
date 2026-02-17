# VexConnect App

BLE mesh relay. One button. No login. No account. No tracking.

## What It Does

Every phone running VexConnect becomes a node in a decentralized mesh network. Encrypted packets hop from phone to phone across the mesh. You relay traffic without ever seeing its contents.

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     React Native App                         │
│  ┌─────────────────────────────────────────────────────┐    │
│  │                    App.tsx                           │    │
│  │  • UI (toggle, stats, indicators)                    │    │
│  │  • JS fallback mode (react-native-ble-plx)           │    │
│  └────────────────────────┬────────────────────────────┘    │
│                           │                                  │
│  ┌────────────────────────▼────────────────────────────┐    │
│  │               NativeMesh.ts Bridge                   │    │
│  └────────────────────────┬────────────────────────────┘    │
└───────────────────────────┼─────────────────────────────────┘
                            │
        ┌───────────────────┴───────────────────┐
        ▼                                       ▼
┌───────────────────┐                 ┌───────────────────┐
│  Android Native   │                 │    iOS Native     │
│  ───────────────  │                 │  ───────────────  │
│  MeshService.java │                 │  MeshService.swift│
│  • Foreground svc │                 │  • Background BLE │
│  • GATT server    │                 │  • GATT server    │
│  • Boot restore   │                 │  • State restore  │
└───────────────────┘                 └───────────────────┘
```

## Features

### Protocol
- **Packet deduplication** — seen-packet cache prevents infinite loops
- **TTL (Time To Live)** — packets expire after 7 hops max
- **Encryption** — NaCl secretbox (XSalsa20 + Poly1305)
- **Connection pooling** — max 5 concurrent BLE connections

### Native Services
- **Android foreground service** — runs in background with notification
- **iOS background BLE** — state restoration across app kills
- **GATT server** — proper peripheral mode, accepts incoming connections
- **Boot restore** — Android auto-starts on device boot

### Fallback
- **JS-only mode** — works without native modules (limited background)
- **Dual-mode** — advertising + scanning in both native and JS

## Packet Structure

```
┌──────────┬──────────┬──────────┬──────────┬─────────────────┐
│ Version  │ PacketID │   TTL    │  Flags   │    Payload      │
│ (1 byte) │ (8 bytes)│ (1 byte) │ (1 byte) │  (up to 501)    │
└──────────┴──────────┴──────────┴──────────┴─────────────────┘
```

## Project Structure

```
app/
├── App.tsx                 # Main React Native app
├── src/
│   └── NativeMesh.ts       # TypeScript bridge to native
├── android/
│   └── app/src/main/java/com/vexconnect/
│       ├── MeshService.java    # Foreground service + GATT
│       ├── MeshModule.java     # RN bridge
│       ├── MeshPackage.java    # RN package
│       └── BootReceiver.java   # Boot restore
└── ios/VexConnect/
    ├── MeshService.swift       # BLE service
    ├── MeshModule.swift        # RN bridge
    └── MeshModule.m            # ObjC bridge header
```

## Build

```bash
# Initialize React Native project
npx react-native init VexConnect --template react-native-template-typescript
cd VexConnect

# Install dependencies
npm install react-native-ble-plx react-native-ble-advertiser tweetnacl tweetnacl-util

# Copy source files
cp -r ../app/src ./src
cp ../app/App.tsx ./App.tsx

# Android: Copy native modules
cp -r ../app/android/app/src/main/java/com/vexconnect/* android/app/src/main/java/com/vexconnect/

# Android: Update MainApplication.java to include MeshPackage
# Add: packages.add(new MeshPackage());

# iOS: Copy native modules
cp ../app/ios/VexConnect/*.swift ios/VexConnect/
cp ../app/ios/VexConnect/*.m ios/VexConnect/

# iOS: Add bridging header if needed
# iOS: Install pods
cd ios && pod install && cd ..

# Run
npx react-native run-android
npx react-native run-ios
```

## Android Setup

### MainApplication.java
Add to `getPackages()`:
```java
packages.add(new MeshPackage());
```

### AndroidManifest.xml
Already included in the template. Key permissions:
- `BLUETOOTH_SCAN`, `BLUETOOTH_CONNECT`, `BLUETOOTH_ADVERTISE`
- `FOREGROUND_SERVICE`, `FOREGROUND_SERVICE_CONNECTED_DEVICE`
- `RECEIVE_BOOT_COMPLETED`

## iOS Setup

### Info.plist
```xml
<key>NSBluetoothAlwaysUsageDescription</key>
<string>VexConnect uses Bluetooth to relay encrypted mesh traffic</string>
<key>UIBackgroundModes</key>
<array>
  <string>bluetooth-central</string>
  <string>bluetooth-peripheral</string>
</array>
```

### Bridging Header
Create `VexConnect-Bridging-Header.h`:
```objc
#import <React/RCTBridgeModule.h>
#import <React/RCTEventEmitter.h>
```

## How It Works

1. **Toggle ON** → Starts native service (or JS fallback)
2. **GATT server starts** → Advertises VexConnect service UUID
3. **Scanning starts** → Looks for other VexConnect nodes
4. **Connection established** → Subscribe to RX characteristic
5. **Packet received** → Check dedup cache → Decrement TTL → Relay to all peers
6. **Background** → Native service keeps running with notification

## What's Next

- [ ] Packet encryption end-to-end (current: transport only)
- [ ] Mesh routing tables for directed messages
- [ ] Bridge to LoRa (phone → BLE → Pi → LoRa → mesh)
- [ ] Incentive layer (token rewards for relaying)
- [ ] Web dashboard for mesh visualization

## Philosophy

No account. No tracking. No data collected.  
Your phone relays encrypted packets.  
You never see the contents. Nobody does.

---

*Part of the [Freedom Directory](../FREEDOM_DIRECTORY.md) — free mesh internet for all beings.*
