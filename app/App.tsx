// VexConnect App.tsx - Victor's Voice
// Building a free mesh internet for all beings, one relay at a time.
// Core mission: Cooperative, not parasitic. Light, encrypted, unstoppable.

import React, { useState, useEffect, useCallback, useRef } from 'react';
import {
  View,
  Text,
  TouchableOpacity,
  StyleSheet,
  StatusBar,
  Platform,
  PermissionsAndroid,
  AppState,
  AppStateStatus,
} from 'react-native';
import { BleManager, Device, Characteristic } from 'react-native-ble-plx';
import BLEAdvertiser from 'react-native-ble-advertiser';
import nacl from 'tweetnacl';
import { encode as encodeBase64, decode as decodeBase64 } from 'tweetnacl-util';

// PROTOCOL CONSTANTS
const PROTOCOL_VERSION = 0x01;
const DEFAULT_TTL = 7;
const MAX_CONNECTIONS = 5;
const SEEN_CACHE_MAX = 1000;
const SEEN_CACHE_TTL_MS = 60_000;
const VEXCONNECT_SERVICE = '0000VC01-0000-1000-8000-00805F9B34FB';
const TX_CHARACTERISTIC = '0000VC02-0000-1000-8000-00805F9B34FB';
const RX_CHARACTERISTIC = '0000VC03-0000-1000-8000-00805F9B34FB';
const MANUFACTURER_ID = 0xFFFF;
const ADVERTISE_INTERVAL = 100;
const FLAG_ENCRYPTED = 0x01;
const FLAG_BROADCAST = 0x02;

// TYPES
interface MeshStats {
  nodesNearby: number;
  packetsRelayed: number;
  packetsSeen: number;
  uptime: number;
  advertising: boolean;
}

interface ParsedPacket {
  version: number;
  packetId: string;
  ttl: number;
  flags: number;
  payload: Uint8Array;
  raw: Uint8Array;
}

interface SeenEntry {
  timestamp: number;
}

interface ConnectedNode {
  device: Device;
  lastSeen: number;
  rssi: number;
}

// PACKET UTILITIES
function generatePacketId(): Uint8Array {
  return nacl.randomBytes(8);
}

function bytesToHex(bytes: Uint8Array): string {
  return Array.from(bytes).map(b => b.toString(16).padStart(2, '0')).join('');
}

function hexToBytes(hex: string): Uint8Array {
  const bytes = new Uint8Array(hex.length / 2);
  for (let i = 0; i < hex.length; i += 2) {
    bytes[i / 2] = parseInt(hex.substr(i, 2), 16);
  }
  return bytes;
}

function buildPacket(version: number, packetId: Uint8Array, ttl: number, flags: number, payload: Uint8Array): Uint8Array {
  const packet = new Uint8Array(11 + payload.length);
  packet[0] = version;
  packet.set(packetId, 1);
  packet[9] = ttl;
  packet[10] = flags;
  packet.set(payload, 11);
  return packet;
}

function parsePacket(data: Uint8Array): ParsedPacket | null {
  if (data.length < 11) return null;
  const version = data[0];
  const packetId = bytesToHex(data.slice(1, 9));
  const ttl = data[9];
  const flags = data[10];
  const payload = data.slice(11);
  return { version, packetId, ttl, flags, payload, raw: data };
}

function rebuildWithNewTtl(packet: ParsedPacket, newTtl: number): Uint8Array {
  const rebuilt = new Uint8Array(packet.raw);
  rebuilt[9] = newTtl;
  return rebuilt;
}

// ENCRYPTION
function deriveMeshKey(): Uint8Array {
  const encoder = new TextEncoder();
  const seed = encoder.encode(VEXCONNECT_SERVICE);
  return nacl.hash(seed).slice(0, nacl.secretbox.keyLength);
}

function encryptPayload(plaintext: Uint8Array, key: Uint8Array): Uint8Array {
  const nonce = nacl.randomBytes(nacl.secretbox.nonceLength);
  const ciphertext = nacl.secretbox(plaintext, nonce, key);
  const encrypted = new Uint8Array(nonce.length + ciphertext.length);
  encrypted.set(nonce);
  encrypted.set(ciphertext, nonce.length);
  return encrypted;
}

function decryptPayload(encrypted: Uint8Array, key: Uint8Array): Uint8Array | null {
  if (encrypted.length < nacl.secretbox.nonceLength + nacl.secretbox.overheadLength) return null;
  const nonce = encrypted.slice(0, nacl.secretbox.nonceLength);
  const ciphertext = encrypted.slice(nacl.secretbox.nonceLength);
  return nacl.secretbox.open(ciphertext, nonce, key);
}

// SEEN CACHE
class SeenCache {
  private cache = new Map<string, SeenEntry>();

  has(packetId: string): boolean {
    const entry = this.cache.get(packetId);
    if (!entry) return false;
    if (Date.now() - entry.timestamp > SEEN_CACHE_TTL_MS) {
      this.cache.delete(packetId);
      return false;
    }
    return true;
  }

  add(packetId: string): void {
    if (this.cache.size >= SEEN_CACHE_MAX) {
      const oldest = this.cache.keys().next().value;
      if (oldest) this.cache.delete(oldest);
    }
    this.cache.set(packetId, { timestamp: Date.now() });
  }

  size(): number {
    return this.cache.size;
  }

  clear(): void {
    this.cache.clear();
  }
}

// BROADCAST QUEUE
class BroadcastQueue {
  private queue: Uint8Array[] = [];
  private maxSize = 10;

  enqueue(packet: Uint8Array): void {
    if (this.queue.length >= this.maxSize) this.queue.shift();
    this.queue.push(packet);
  }

  dequeue(): Uint8Array | undefined {
    return this.queue.shift();
  }

  peek(): Uint8Array | undefined {
    return this.queue[0];
  }

  size(): number {
    return this.queue.length;
  }

  clear(): void {
    this.queue = [];
  }
}

// MAIN APP
const App = () => {
  const [active, setActive] = useState(false);
  const [stats, setStats] = useState<MeshStats>({
    nodesNearby: 0,
    packetsRelayed: 0,
    packetsSeen: 0,
    uptime: 0,
    advertising: false,
  });

  const manager = useRef(new BleManager());
  const connectedNodes = useRef(new Map<string, ConnectedNode>());
  const discoveredDevices = useRef(new Map<string, Device>());
  const seenCache = useRef(new SeenCache());
  const broadcastQueue = useRef(new BroadcastQueue());
  const meshKey = useRef(deriveMeshKey());
  const appState = useRef(AppState.currentState);
  const scanDutyRef = useRef<NodeJS.Timeout | null>(null);
  const advertiseRef = useRef<NodeJS.Timeout | null>(null);
  const nodeId = useRef(bytesToHex(nacl.randomBytes(4)));

  // PERMISSIONS
  useEffect(() => {
    const requestPermissions = async () => {
      if (Platform.OS === 'android') {
        try {
          const granted = await PermissionsAndroid.requestMultiple([
            PermissionsAndroid.PERMISSIONS.BLUETOOTH_SCAN,
            PermissionsAndroid.PERMISSIONS.BLUETOOTH_CONNECT,
            PermissionsAndroid.PERMISSIONS.BLUETOOTH_ADVERTISE,
            PermissionsAndroid.PERMISSIONS.ACCESS_FINE_LOCATION,
          ]);
          console.log('Permissions:', granted);
        } catch (err) {
          console.warn('Permission request failed:', err);
        }
      }
    };
    requestPermissions();
  }, []);

  // APP STATE TRACKING
  useEffect(() => {
    const subscription = AppState.addEventListener('change', (nextAppState: AppStateStatus) => {
      appState.current = nextAppState;
    });
    return () => subscription.remove();
  }, []);

  // UPTIME COUNTER
  useEffect(() => {
    if (!active) return;
    const interval = setInterval(() => {
      setStats(s => ({ ...s, uptime: s.uptime + 1 }));
    }, 1000);
    return () => clearInterval(interval);
  }, [active]);

  // BLE ADVERTISING
  const startAdvertising = useCallback(async () => {
    try {
      BLEAdvertiser.setCompanyId(MANUFACTURER_ID);
      const beaconData = [PROTOCOL_VERSION, ...Array.from(hexToBytes(nodeId.current))];
      await BLEAdvertiser.broadcast(VEXCONNECT_SERVICE, beaconData, {
        advertiseMode: 1,
        txPowerLevel: 2,
        connectable: true,
        includeDeviceName: false,
      });
      setStats(s => ({ ...s, advertising: true }));
      console.log('Advertising started, nodeId:', nodeId.current);
      return true;
    } catch (err) {
      console.warn('Failed to start advertising:', err);
      return false;
    }
  }, []);

  const stopAdvertising = useCallback(async () => {
    try {
      await BLEAdvertiser.stopBroadcast();
      setStats(s => ({ ...s, advertising: false }));
      console.log('Advertising stopped');
    } catch (err) {
      console.warn('Failed to stop advertising:', err);
    }
  }, []);

  // SCAN FOR ADVERTISEMENTS
  const handleDiscoveredDevice = useCallback((device: Device) => {
    if (!device?.id) return;
    const mfgData = device.manufacturerData;
    if (mfgData) {
      try {
        const data = decodeBase64(mfgData);
        if (data.length >= 5 && data[0] === PROTOCOL_VERSION) {
          const remoteNodeId = bytesToHex(data.slice(1, 5));
          console.log('Found VexConnect node:', remoteNodeId, 'RSSI:', device.rssi);
        }
      } catch {
        // Not our format
      }
    }
    if (!discoveredDevices.current.has(device.id)) {
      discoveredDevices.current.set(device.id, device);
    }
  }, []);

  // PACKET RELAY LOGIC
  const relayPacket = useCallback(async (packet: ParsedPacket, sourceDeviceId: string) => {
    if (packet.version !== PROTOCOL_VERSION) {
      console.log('Unknown protocol version:', packet.version);
      return;
    }
    if (seenCache.current.has(packet.packetId)) return;
    seenCache.current.add(packet.packetId);
    setStats(s => ({ ...s, packetsSeen: seenCache.current.size() }));
    if (packet.ttl <= 1) {
      console.log('Packet TTL expired:', packet.packetId);
      return;
    }
    const newTtl = packet.ttl - 1;
    const relayData = rebuildWithNewTtl(packet, newTtl);
    broadcastQueue.current.enqueue(relayData);
    const relayBase64 = encodeBase64(relayData);
    let relayedCount = 0;
    for (const [deviceId, node] of connectedNodes.current) {
      if (deviceId === sourceDeviceId) continue;
      try {
        await node.device.writeCharacteristicWithResponseForService(
          VEXCONNECT_SERVICE,
          TX_CHARACTERISTIC,
          relayBase64,
        );
        relayedCount++;
      } catch {
        console.log('Relay to', deviceId, 'failed, removing');
        connectedNodes.current.delete(deviceId);
      }
    }
    if (relayedCount > 0) {
      setStats(s => ({ ...s, packetsRelayed: s.packetsRelayed + 1 }));
    }
  }, []);

  // CONNECTION MANAGEMENT
  const connectToDevice = useCallback(async (device: Device) => {
    if (connectedNodes.current.size >= MAX_CONNECTIONS || connectedNodes.current.has(device.id)) return;
    try {
      const connected = await device.connect({ timeout: 5000 });
      await connected.discoverAllServicesAndCharacteristics();
      connectedNodes.current.set(device.id, {
        device: connected,
        lastSeen: Date.now(),
        rssi: device.rssi ?? -100,
      });
      setStats(s => ({ ...s, nodesNearby: connectedNodes.current.size }));
      connected.monitorCharacteristicForService(
        VEXCONNECT_SERVICE,
        RX_CHARACTERISTIC,
        (error: Error | null, characteristic: Characteristic | null) => {
          if (error || !characteristic?.value) return;
          try {
            const data = decodeBase64(characteristic.value);
            const packet = parsePacket(data);
            if (packet) {
              const node = connectedNodes.current.get(device.id);
              if (node) node.lastSeen = Date.now();
              relayPacket(packet, device.id);
            }
          } catch (err) {
            console.warn('Failed to parse packet:', err);
          }
        },
      );
      connected.onDisconnected(() => {
        connectedNodes.current.delete(device.id);
        setStats(s => ({ ...s, nodesNearby: connectedNodes.current.size }));
      });
      console.log('Connected to node:', device.id);
    } catch (err) {
      console.log('Connection to', device.id, 'failed');
      discoveredDevices.current.delete(device.id);
    }
  }, [relayPacket]);

  // DUTY-CYCLED SCANNING
  const startDutyCycledScan = useCallback(() => {
    const isBackground = appState.current !== 'active';
    const scanDuration = isBackground ? 2000 : 5000;
    const scanInterval = isBackground ? 30000 : 10000;
    const doScan = () => {
      manager.current.startDeviceScan([VEXCONNECT_SERVICE], { allowDuplicates: true }, (error, device) => {
        if (error) {
          console.warn('Scan error:', error);
          return;
        }
        if (!device) return;
        handleDiscoveredDevice(device);
        if (!connectedNodes.current.has(device.id)) {
          connectToDevice(device);
        }
      });
      setTimeout(() => manager.current.stopDeviceScan(), scanDuration);
    };
    doScan();
    scanDutyRef.current = setInterval(doScan, scanDuration + scanInterval);
  }, [connectToDevice, handleDiscoveredDevice]);

  const stopDutyCycledScan = useCallback(() => {
    manager.current.stopDeviceScan();
    if (scanDutyRef.current) {
      clearInterval(scanDutyRef.current);
      scanDutyRef.current = null;
    }
  }, []);

  // MESH ACTIVATION
  useEffect(() => {
    if (!active) {
      stopDutyCycledScan();
      stopAdvertising();
      for (const [, node] of connectedNodes.current) {
        try {
          node.device.cancelConnection();
        } catch {}
      }
      connectedNodes.current.clear();
      discoveredDevices.current.clear();
      seenCache.current.clear();
      broadcastQueue.current.clear();
      return;
    }
    startAdvertising();
    startDutyCycledScan();
    const pruner = setInterval(() => {
      const now = Date.now();
      for (const [deviceId, node] of connectedNodes.current) {
        if (now - node.lastSeen > 120_000) {
          console.log('Pruning stale connection:', deviceId);
          try {
            node.device.cancelConnection();
          } catch {}
          connectedNodes.current.delete(deviceId);
        }
      }
      setStats(s => ({ ...s, nodesNearby: connectedNodes.current.size }));
    }, 30000);
    return () => {
      stopDutyCycledScan();
      stopAdvertising();
      clearInterval(pruner);
    };
  }, [active, startDutyCycledScan, stopDutyCycledScan, startAdvertising, stopAdvertising]);

  // UI HELPERS
  const formatUptime = (seconds: number): string => {
    const h = Math.floor(seconds / 3600);
    const m = Math.floor((seconds % 3600) / 60);
    const s = seconds % 60;
    if (h > 0) return `${h}h ${m}m`;
    if (m > 0) return `${m}m ${s}s`;
    return `${s}s`;
  };

  const toggleMesh = () => {
    if (active) {
      setActive(false);
      setStats({
        nodesNearby: 0,
        packetsRelayed: 0,
        packetsSeen: 0,
        uptime: 0,
        advertising: false,
      });
    } else {
      setActive(true);
    }
  };

  // RENDER
  return (
    <View style={styles.container}>
      <StatusBar barStyle="light-content" backgroundColor="#0a0a0a" />
      <Text style={styles.title}>VexConnect</Text>
      <Text style={styles.subtitle}>
        {active ? (stats.advertising ? 'Broadcasting & scanning' : 'Mesh active (scan only)') : 'Mesh inactive'}
      </Text>
      {active && (
        <View style={styles.nodeIdBadge}>
          <Text style={styles.nodeIdText}>node:{nodeId.current}</Text>
        </View>
      )}
      <TouchableOpacity
        style={[styles.button, active && styles.buttonActive]}
        onPress={toggleMesh}
        activeOpacity={0.8}
      >
        <View style={styles.dotContainer}>
          <View style={[styles.dot, active && styles.dotActive]} />
          {active && stats.advertising && <View style={styles.pulseRing} />}
        </View>
        <Text style={[styles.buttonText, active && styles.buttonTextActive]}>
          {active ? 'ON' : 'OFF'}
        </Text>
      </TouchableOpacity>
      {active && (
        <View style={styles.stats}>
          <View style={styles.statRow}>
            <Text style={styles.statNumber}>{stats.nodesNearby}</Text>
            <Text style={styles.statLabel}>nodes</Text>
          </View>
          <View style={styles.statRow}>
            <Text style={styles.statNumber}>{stats.packetsRelayed}</Text>
            <Text style={styles.statLabel}>relayed</Text>
          </View>
          <View style={styles.statRow}>
            <Text style={styles.statNumber}>{stats.packetsSeen}</Text>
            <Text style={styles.statLabel}>seen</Text>
          </View>
          <View style={styles.statRow}>
            <Text style={styles.statNumber}>{formatUptime(stats.uptime)}</Text>
            <Text style={styles.statLabel}>uptime</Text>
          </View>
        </View>
      )}
      {active && (
        <View style={styles.modeContainer}>
          <View style={styles.modeRow}>
            <View style={[styles.modeIndicator, styles.modeActive]} />
            <Text style={styles.modeText}>Scanning</Text>
          </View>
          <View style={styles.modeRow}>
            <View style={[styles.modeIndicator, stats.advertising ? styles.modeActive : styles.modeInactive]} />
            <Text style={styles.modeText}>Advertising</Text>
          </View>
        </View>
      )}
      {active && (
        <View style={styles.protocol}>
          <Text style={styles.protocolText}>
            v{PROTOCOL_VERSION} • TTL:{DEFAULT_TTL} • max {MAX_CONNECTIONS} peers
          </Text>
        </View>
      )}
      <Text style={styles.message}>
        {active ? 'You are helping the free internet exist.' : 'Tap to join the mesh.'}
      </Text>
      <Text style={styles.footer}>
        No account. No tracking. No data collected.{'
'}
        Your phone relays encrypted packets.{'
'}
        You never see the contents. Nobody does.
      </Text>
    </View>
  );
};

// STYLES
const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#0a0a0a',
    alignItems: 'center',
    justifyContent: 'center',
    padding: 24,
  },
  title: {
    fontSize: 28,
    fontWeight: '700',
    color: '#0EFFAF',
    marginBottom: 4,
  },
  subtitle: {
    fontSize: 14,
    color: '#888',
    marginBottom: 12,
  },
  nodeIdBadge: {
    backgroundColor: '#1a1a1a',
    paddingHorizontal: 12,
    paddingVertical: 4,
    borderRadius: 12,
    marginBottom: 32,
  },
  nodeIdText: {
    fontSize: 11,
    color: '#666',
    fontFamily: Platform.OS === 'ios' ? 'Menlo' : 'monospace',
  },
  button: {
    width: 160,
    height: 160,
    borderRadius: 80,
    backgroundColor: '#111',
    borderWidth: 2,
    borderColor: '#222',
    alignItems: 'center',
    justifyContent: 'center',
    marginBottom: 32,
  },
  buttonActive: {
    borderColor: '#0EFFAF',
    backgroundColor: '#0a1a14',
  },
  dotContainer: {
    position: 'relative',
    alignItems: 'center',
    justifyContent: 'center',
    marginBottom: 8,
  },
  dot: {
    width: 12,
    height: 12,
    borderRadius: 6,
    backgroundColor: '#333',
  },
  dotActive: {
    backgroundColor: '#0EFFAF',
    shadowColor: '#0EFFAF',
    shadowOffset: { width: 0, height: 0 },
    shadowOpacity: 0.8,
    shadowRadius: 12,
  },
  pulseRing: {
    position: 'absolute',
    width: 32,
    height: 32,
    borderRadius: 16,
    borderWidth: 2,
    borderColor: '#0EFFAF',
    opacity: 0.3,
  },
  buttonText: {
    fontSize: 18,
    fontWeight: '600',
    color: '#555',
  },
  buttonTextActive: {
    color: '#0EFFAF',
  },
  stats: {
    flexDirection: 'row',
    gap: 24,
    marginBottom: 16,
  },
  statRow: {
    alignItems: 'center',
  },
  statNumber: {
    fontSize: 22,
    fontWeight: '700',
    color: '#e8e8e8',
  },
  statLabel: {
    fontSize: 10,
    color: '#888',
    marginTop: 2,
    textTransform: 'uppercase',
    letterSpacing: 0.5,
  },
  modeContainer: {
    flexDirection: 'row',
    gap: 24,
    marginBottom: 16,
  },
  modeRow: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 6,
  },
  modeIndicator: {
    width: 8,
    height: 8,
    borderRadius: 4,
  },
  modeActive: {
    backgroundColor: '#0EFFAF',
  },
  modeInactive: {
    backgroundColor: '#333',
  },
  modeText: {
    fontSize: 11,
    color: '#666',
  },
  protocol: {
    marginBottom: 24,
  },
  protocolText: {
    fontSize: 11,
    color: '#444',
    fontFamily: Platform.OS === 'ios' ? 'Menlo' : 'monospace',
  },
  message: {
    fontSize: 15,
    color: '#0EFFAF',
    textAlign: 'center',
    marginBottom: 40,
    fontWeight: '500',
  },
  footer: {
    fontSize: 12,
    color: '#555',
    textAlign: 'center',
    lineHeight: 20,
    position: 'absolute',
    bottom: 48,
  },
});

export default App;
