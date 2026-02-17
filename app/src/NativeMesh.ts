import { NativeModules, NativeEventEmitter, Platform } from 'react-native';

// ============================================================================
// NATIVE MODULE INTERFACE
// ============================================================================

interface MeshModuleInterface {
  startService(): Promise<boolean>;
  stopService(): Promise<boolean>;
}

interface PacketEvent {
  packet: string; // base64
  sourceDeviceId: string;
}

interface StatsEvent {
  nodesNearby: number;
  packetsRelayed: number;
  packetsSeen: number;
}

// ============================================================================
// MODULE ACCESS
// ============================================================================

const MeshModule: MeshModuleInterface | undefined = 
  Platform.OS === 'android' 
    ? NativeModules.VexConnectMesh 
    : NativeModules.MeshModule;

const eventEmitter = MeshModule 
  ? new NativeEventEmitter(NativeModules.VexConnectMesh || NativeModules.MeshModule)
  : null;

// ============================================================================
// PUBLIC API
// ============================================================================

/**
 * Check if native mesh module is available
 */
export function isNativeMeshAvailable(): boolean {
  return MeshModule !== undefined;
}

/**
 * Start the native mesh service (foreground service on Android)
 */
export async function startNativeService(): Promise<boolean> {
  if (!MeshModule) {
    console.warn('[NativeMesh] Native module not available');
    return false;
  }
  
  try {
    return await MeshModule.startService();
  } catch (err) {
    console.error('[NativeMesh] Failed to start service:', err);
    return false;
  }
}

/**
 * Stop the native mesh service
 */
export async function stopNativeService(): Promise<boolean> {
  if (!MeshModule) {
    return false;
  }
  
  try {
    return await MeshModule.stopService();
  } catch (err) {
    console.error('[NativeMesh] Failed to stop service:', err);
    return false;
  }
}

/**
 * Subscribe to incoming packets from native service
 */
export function onPacketReceived(
  callback: (packet: string, sourceDeviceId: string) => void
): () => void {
  if (!eventEmitter) {
    return () => {};
  }
  
  const subscription = eventEmitter.addListener(
    'onPacketReceived',
    (event: PacketEvent) => {
      callback(event.packet, event.sourceDeviceId);
    }
  );
  
  return () => subscription.remove();
}

/**
 * Subscribe to stats updates from native service
 */
export function onStatsUpdated(
  callback: (stats: StatsEvent) => void
): () => void {
  if (!eventEmitter) {
    return () => {};
  }
  
  const subscription = eventEmitter.addListener(
    'onStatsUpdated',
    (event: StatsEvent) => {
      callback(event);
    }
  );
  
  return () => subscription.remove();
}

// ============================================================================
// EXPORT TYPES
// ============================================================================

export type { PacketEvent, StatsEvent };
