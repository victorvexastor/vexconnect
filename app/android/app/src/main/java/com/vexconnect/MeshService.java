package com.vexconnect;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattServer;
import android.bluetooth.BluetoothGattServerCallback;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.le.AdvertiseCallback;
import android.bluetooth.le.AdvertiseData;
import android.bluetooth.le.AdvertiseSettings;
import android.bluetooth.le.BluetoothLeAdvertiser;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.IBinder;
import android.os.ParcelUuid;
import android.util.Log;

import androidx.annotation.Nullable;
import androidx.core.app.NotificationCompat;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentLinkedQueue;

/**
 * VexConnect Mesh Service
 * 
 * Runs as a foreground service to maintain BLE mesh operations in background.
 * Implements a GATT server for receiving packets from other nodes.
 */
public class MeshService extends Service {
    private static final String TAG = "VexConnect";
    private static final String CHANNEL_ID = "vexconnect_mesh";
    private static final int NOTIFICATION_ID = 1;

    // Protocol constants
    private static final UUID SERVICE_UUID = UUID.fromString("0000VC01-0000-1000-8000-00805F9B34FB");
    private static final UUID TX_CHAR_UUID = UUID.fromString("0000VC02-0000-1000-8000-00805F9B34FB");
    private static final UUID RX_CHAR_UUID = UUID.fromString("0000VC03-0000-1000-8000-00805F9B34FB");
    
    private static final int PROTOCOL_VERSION = 0x01;
    private static final int DEFAULT_TTL = 7;
    private static final int SEEN_CACHE_MAX = 1000;
    private static final long SEEN_CACHE_TTL_MS = 60_000;

    // BLE components
    private BluetoothManager bluetoothManager;
    private BluetoothAdapter bluetoothAdapter;
    private BluetoothLeAdvertiser advertiser;
    private BluetoothGattServer gattServer;

    // State
    private boolean isRunning = false;
    private final ConcurrentHashMap<String, Long> seenCache = new ConcurrentHashMap<>();
    private final ConcurrentLinkedQueue<byte[]> packetQueue = new ConcurrentLinkedQueue<>();
    private final Set<BluetoothDevice> connectedDevices = ConcurrentHashMap.newKeySet();

    // Stats
    private int packetsRelayed = 0;
    private int packetsSeen = 0;

    // Callbacks
    private static PacketCallback packetCallback;

    public interface PacketCallback {
        void onPacketReceived(byte[] packet, String sourceDeviceId);
        void onStatsUpdated(int nodesNearby, int relayed, int seen);
    }

    public static void setPacketCallback(PacketCallback callback) {
        packetCallback = callback;
    }

    @Override
    public void onCreate() {
        super.onCreate();
        Log.d(TAG, "MeshService onCreate");

        bluetoothManager = (BluetoothManager) getSystemService(Context.BLUETOOTH_SERVICE);
        if (bluetoothManager != null) {
            bluetoothAdapter = bluetoothManager.getAdapter();
        }

        createNotificationChannel();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.d(TAG, "MeshService onStartCommand");

        if (!isRunning) {
            startForeground(NOTIFICATION_ID, createNotification());
            startMesh();
            isRunning = true;
        }

        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        Log.d(TAG, "MeshService onDestroy");
        stopMesh();
        super.onDestroy();
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    // =========================================================================
    // NOTIFICATION
    // =========================================================================

    private void createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(
                CHANNEL_ID,
                "VexConnect Mesh",
                NotificationManager.IMPORTANCE_LOW
            );
            channel.setDescription("Mesh relay service");
            channel.setShowBadge(false);

            NotificationManager manager = getSystemService(NotificationManager.class);
            if (manager != null) {
                manager.createNotificationChannel(channel);
            }
        }
    }

    private Notification createNotification() {
        Intent notificationIntent = new Intent(this, MainActivity.class);
        PendingIntent pendingIntent = PendingIntent.getActivity(
            this, 0, notificationIntent,
            PendingIntent.FLAG_IMMUTABLE
        );

        return new NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("VexConnect")
            .setContentText("Mesh active • " + connectedDevices.size() + " nodes • " + packetsRelayed + " relayed")
            .setSmallIcon(android.R.drawable.ic_dialog_info)
            .setContentIntent(pendingIntent)
            .setOngoing(true)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .build();
    }

    private void updateNotification() {
        NotificationManager manager = getSystemService(NotificationManager.class);
        if (manager != null) {
            manager.notify(NOTIFICATION_ID, createNotification());
        }
    }

    // =========================================================================
    // MESH OPERATIONS
    // =========================================================================

    private void startMesh() {
        if (bluetoothAdapter == null || !bluetoothAdapter.isEnabled()) {
            Log.e(TAG, "Bluetooth not available");
            return;
        }

        startGattServer();
        startAdvertising();

        // Periodic cache cleanup
        new Thread(() -> {
            while (isRunning) {
                try {
                    Thread.sleep(30000);
                    cleanSeenCache();
                    updateNotification();
                } catch (InterruptedException e) {
                    break;
                }
            }
        }).start();
    }

    private void stopMesh() {
        isRunning = false;
        stopAdvertising();
        stopGattServer();
        seenCache.clear();
        connectedDevices.clear();
    }

    // =========================================================================
    // GATT SERVER
    // =========================================================================

    private void startGattServer() {
        gattServer = bluetoothManager.openGattServer(this, gattServerCallback);
        if (gattServer == null) {
            Log.e(TAG, "Failed to open GATT server");
            return;
        }

        // Create service
        BluetoothGattService service = new BluetoothGattService(
            SERVICE_UUID,
            BluetoothGattService.SERVICE_TYPE_PRIMARY
        );

        // TX characteristic (write) - other nodes write packets to us
        BluetoothGattCharacteristic txChar = new BluetoothGattCharacteristic(
            TX_CHAR_UUID,
            BluetoothGattCharacteristic.PROPERTY_WRITE | BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE,
            BluetoothGattCharacteristic.PERMISSION_WRITE
        );
        service.addCharacteristic(txChar);

        // RX characteristic (notify) - we notify connected clients of packets
        BluetoothGattCharacteristic rxChar = new BluetoothGattCharacteristic(
            RX_CHAR_UUID,
            BluetoothGattCharacteristic.PROPERTY_READ | BluetoothGattCharacteristic.PROPERTY_NOTIFY,
            BluetoothGattCharacteristic.PERMISSION_READ
        );
        service.addCharacteristic(rxChar);

        gattServer.addService(service);
        Log.d(TAG, "GATT server started");
    }

    private void stopGattServer() {
        if (gattServer != null) {
            gattServer.close();
            gattServer = null;
        }
    }

    private final BluetoothGattServerCallback gattServerCallback = new BluetoothGattServerCallback() {
        @Override
        public void onConnectionStateChange(BluetoothDevice device, int status, int newState) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                Log.d(TAG, "Device connected: " + device.getAddress());
                connectedDevices.add(device);
                notifyStats();
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                Log.d(TAG, "Device disconnected: " + device.getAddress());
                connectedDevices.remove(device);
                notifyStats();
            }
        }

        @Override
        public void onCharacteristicWriteRequest(
            BluetoothDevice device,
            int requestId,
            BluetoothGattCharacteristic characteristic,
            boolean preparedWrite,
            boolean responseNeeded,
            int offset,
            byte[] value
        ) {
            if (TX_CHAR_UUID.equals(characteristic.getUuid())) {
                // Received a packet from another node
                handleIncomingPacket(value, device.getAddress());

                if (responseNeeded) {
                    gattServer.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, 0, null);
                }
            }
        }

        @Override
        public void onCharacteristicReadRequest(
            BluetoothDevice device,
            int requestId,
            int offset,
            BluetoothGattCharacteristic characteristic
        ) {
            if (RX_CHAR_UUID.equals(characteristic.getUuid())) {
                // Return queued packet or empty
                byte[] packet = packetQueue.poll();
                gattServer.sendResponse(
                    device, requestId, BluetoothGatt.GATT_SUCCESS, 0,
                    packet != null ? packet : new byte[0]
                );
            }
        }
    };

    // =========================================================================
    // ADVERTISING
    // =========================================================================

    private void startAdvertising() {
        advertiser = bluetoothAdapter.getBluetoothLeAdvertiser();
        if (advertiser == null) {
            Log.e(TAG, "BLE advertising not supported");
            return;
        }

        AdvertiseSettings settings = new AdvertiseSettings.Builder()
            .setAdvertiseMode(AdvertiseSettings.ADVERTISE_MODE_BALANCED)
            .setTxPowerLevel(AdvertiseSettings.ADVERTISE_TX_POWER_MEDIUM)
            .setConnectable(true)
            .build();

        AdvertiseData data = new AdvertiseData.Builder()
            .setIncludeDeviceName(false)
            .addServiceUuid(new ParcelUuid(SERVICE_UUID))
            .build();

        advertiser.startAdvertising(settings, data, advertiseCallback);
    }

    private void stopAdvertising() {
        if (advertiser != null) {
            advertiser.stopAdvertising(advertiseCallback);
        }
    }

    private final AdvertiseCallback advertiseCallback = new AdvertiseCallback() {
        @Override
        public void onStartSuccess(AdvertiseSettings settingsInEffect) {
            Log.d(TAG, "Advertising started");
        }

        @Override
        public void onStartFailure(int errorCode) {
            Log.e(TAG, "Advertising failed: " + errorCode);
        }
    };

    // =========================================================================
    // PACKET HANDLING
    // =========================================================================

    private void handleIncomingPacket(byte[] data, String sourceDeviceId) {
        if (data == null || data.length < 11) return;

        // Parse header
        int version = data[0] & 0xFF;
        if (version != PROTOCOL_VERSION) return;

        // Extract packet ID (bytes 1-8)
        String packetId = bytesToHex(Arrays.copyOfRange(data, 1, 9));

        // Deduplication
        if (seenCache.containsKey(packetId)) return;
        seenCache.put(packetId, System.currentTimeMillis());
        packetsSeen++;

        // TTL check
        int ttl = data[9] & 0xFF;
        if (ttl <= 1) return;

        // Decrement TTL
        byte[] relayData = data.clone();
        relayData[9] = (byte) (ttl - 1);

        // Relay to all connected devices except source
        relayPacket(relayData, sourceDeviceId);

        // Notify JS layer
        if (packetCallback != null) {
            packetCallback.onPacketReceived(data, sourceDeviceId);
        }

        notifyStats();
    }

    private void relayPacket(byte[] packet, String excludeDeviceId) {
        if (gattServer == null) return;

        BluetoothGattService service = gattServer.getService(SERVICE_UUID);
        if (service == null) return;

        BluetoothGattCharacteristic rxChar = service.getCharacteristic(RX_CHAR_UUID);
        if (rxChar == null) return;

        rxChar.setValue(packet);

        for (BluetoothDevice device : connectedDevices) {
            if (!device.getAddress().equals(excludeDeviceId)) {
                gattServer.notifyCharacteristicChanged(device, rxChar, false);
                packetsRelayed++;
            }
        }
    }

    private void cleanSeenCache() {
        long now = System.currentTimeMillis();
        seenCache.entrySet().removeIf(entry -> 
            now - entry.getValue() > SEEN_CACHE_TTL_MS
        );

        // LRU eviction if still too large
        while (seenCache.size() > SEEN_CACHE_MAX) {
            String oldest = seenCache.entrySet().stream()
                .min((a, b) -> Long.compare(a.getValue(), b.getValue()))
                .map(e -> e.getKey())
                .orElse(null);
            if (oldest != null) {
                seenCache.remove(oldest);
            }
        }
    }

    private void notifyStats() {
        if (packetCallback != null) {
            packetCallback.onStatsUpdated(connectedDevices.size(), packetsRelayed, packetsSeen);
        }
    }

    // =========================================================================
    // UTILITIES
    // =========================================================================

    private static String bytesToHex(byte[] bytes) {
        StringBuilder sb = new StringBuilder();
        for (byte b : bytes) {
            sb.append(String.format("%02x", b));
        }
        return sb.toString();
    }
}
