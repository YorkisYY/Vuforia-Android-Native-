package com.example.ibm_ai_weather_art_android.camera;

import android.app.Activity;
import android.content.Context;
import com.example.ibm_ai_weather_art_android.VuforiaManager;

/**
 * Vuforia Camera Controller
 * Manages Vuforia camera and AR session
 */
public class ARCameraController {
    private Context context;
    private VuforiaManager vuforiaManager;
    private ARCallback callback;

    public ARCameraController(Context context, ARCallback callback) {
        this.context = context;
        this.callback = callback;
        this.vuforiaManager = new VuforiaManager(context);
    }

    /**
     * Initialize Vuforia
     */
    public void initializeVuforia() {
        try {
            if (vuforiaManager.setupVuforia()) {
                callback.onVuforiaInitialized();
            } else {
                callback.onVuforiaError("Vuforia initialization failed");
            }
        } catch (Exception e) {
            callback.onVuforiaError("Vuforia initialization error: " + e.getMessage());
        }
    }

    /**
     * Start Vuforia session
     */
    public void startVuforiaSession() {
        if (vuforiaManager == null) {
            callback.onVuforiaError("Vuforia not initialized");
            return;
        }
        try {
            callback.onVuforiaSessionStarted();
        } catch (Exception e) {
            callback.onVuforiaError("Failed to start Vuforia session: " + e.getMessage());
        }
    }

    /**
     * Pause Vuforia session
     */
    public void pauseVuforiaSession() {
        if (vuforiaManager != null) {
            callback.onVuforiaSessionPaused();
        }
    }

    /**
     * Stop Vuforia session
     */
    public void stopVuforiaSession() {
        if (vuforiaManager != null) {
            vuforiaManager.onDestroy();
            callback.onVuforiaSessionStopped();
        }
    }

    /**
     * Get Vuforia manager
     */
    public VuforiaManager getVuforiaManager() {
        return vuforiaManager;
    }

    /**
     * Check if Vuforia is initialized
     */
    public boolean isVuforiaInitialized() {
        return vuforiaManager != null;
    }

    /**
     * Vuforia event callback interface
     */
    public interface ARCallback {
        void onVuforiaInitialized();
        void onVuforiaError(String error);
        void onVuforiaSessionStarted();
        void onVuforiaSessionPaused();
        void onVuforiaSessionStopped();
    }
} 