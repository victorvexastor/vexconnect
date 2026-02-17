package com.vexconnect;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Build;
import android.util.Log;

/**
 * Restarts mesh service on device boot if it was previously running
 */
public class BootReceiver extends BroadcastReceiver {
    private static final String TAG = "VexConnect";
    private static final String PREFS_NAME = "vexconnect_prefs";
    private static final String KEY_SERVICE_ENABLED = "service_enabled";
    
    @Override
    public void onReceive(Context context, Intent intent) {
        if (Intent.ACTION_BOOT_COMPLETED.equals(intent.getAction())) {
            SharedPreferences prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
            boolean wasEnabled = prefs.getBoolean(KEY_SERVICE_ENABLED, false);
            
            if (wasEnabled) {
                Log.d(TAG, "Boot completed, restarting mesh service");
                Intent serviceIntent = new Intent(context, MeshService.class);
                
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    context.startForegroundService(serviceIntent);
                } else {
                    context.startService(serviceIntent);
                }
            }
        }
    }
    
    /**
     * Save service state for boot restoration
     */
    public static void setServiceEnabled(Context context, boolean enabled) {
        SharedPreferences prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        prefs.edit().putBoolean(KEY_SERVICE_ENABLED, enabled).apply();
    }
}
