package com.vexconnect;

import android.content.Intent;
import android.os.Build;
import android.util.Base64;

import androidx.annotation.NonNull;

import com.facebook.react.bridge.Arguments;
import com.facebook.react.bridge.Promise;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReactContextBaseJavaModule;
import com.facebook.react.bridge.ReactMethod;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.modules.core.DeviceEventManagerModule;

/**
 * React Native bridge for VexConnect mesh service
 */
public class MeshModule extends ReactContextBaseJavaModule implements MeshService.PacketCallback {
    private static final String MODULE_NAME = "VexConnectMesh";
    private final ReactApplicationContext reactContext;

    public MeshModule(ReactApplicationContext reactContext) {
        super(reactContext);
        this.reactContext = reactContext;
        MeshService.setPacketCallback(this);
    }

    @NonNull
    @Override
    public String getName() {
        return MODULE_NAME;
    }

    @ReactMethod
    public void startService(Promise promise) {
        try {
            Intent intent = new Intent(reactContext, MeshService.class);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                reactContext.startForegroundService(intent);
            } else {
                reactContext.startService(intent);
            }
            promise.resolve(true);
        } catch (Exception e) {
            promise.reject("START_ERROR", e.getMessage());
        }
    }

    @ReactMethod
    public void stopService(Promise promise) {
        try {
            Intent intent = new Intent(reactContext, MeshService.class);
            reactContext.stopService(intent);
            promise.resolve(true);
        } catch (Exception e) {
            promise.reject("STOP_ERROR", e.getMessage());
        }
    }

    @ReactMethod
    public void addListener(String eventName) {
        // Required for RN event emitter
    }

    @ReactMethod
    public void removeListeners(Integer count) {
        // Required for RN event emitter
    }

    // =========================================================================
    // PACKET CALLBACK IMPLEMENTATION
    // =========================================================================

    @Override
    public void onPacketReceived(byte[] packet, String sourceDeviceId) {
        WritableMap params = Arguments.createMap();
        params.putString("packet", Base64.encodeToString(packet, Base64.NO_WRAP));
        params.putString("sourceDeviceId", sourceDeviceId);
        
        sendEvent("onPacketReceived", params);
    }

    @Override
    public void onStatsUpdated(int nodesNearby, int relayed, int seen) {
        WritableMap params = Arguments.createMap();
        params.putInt("nodesNearby", nodesNearby);
        params.putInt("packetsRelayed", relayed);
        params.putInt("packetsSeen", seen);
        
        sendEvent("onStatsUpdated", params);
    }

    private void sendEvent(String eventName, WritableMap params) {
        if (reactContext.hasActiveReactInstance()) {
            reactContext
                .getJSModule(DeviceEventManagerModule.RCTDeviceEventEmitter.class)
                .emit(eventName, params);
        }
    }
}
