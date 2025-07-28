package com.example.ibm_ai_weather_art_android.camera;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

/**
 * Camera permission manager
 * Handles camera permission requests and checks
 */
public class CameraPermissionManager {
    public static final int CAMERA_PERMISSION_CODE = 100;
    
    private Activity activity;
    private PermissionCallback callback;

    public CameraPermissionManager(Activity activity, PermissionCallback callback) {
        this.activity = activity;
        this.callback = callback;
    }

    /**
     * Check if camera permission is granted
     */
    public boolean hasCameraPermission() {
        return ContextCompat.checkSelfPermission(activity, Manifest.permission.CAMERA) 
                == PackageManager.PERMISSION_GRANTED;
    }

    /**
     * Request camera permission
     */
    public void requestCameraPermission() {
        ActivityCompat.requestPermissions(activity, 
            new String[]{Manifest.permission.CAMERA}, CAMERA_PERMISSION_CODE);
    }

    /**
     * Check and request permission (one-stop method)
     */
    public void checkAndRequestPermission() {
        if (hasCameraPermission()) {
            callback.onPermissionGranted();
        } else {
            requestCameraPermission();
        }
    }

    /**
     * Handle permission request result
     */
    public void handlePermissionResult(int requestCode, int[] grantResults) {
        if (requestCode == CAMERA_PERMISSION_CODE) {
            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                callback.onPermissionGranted();
            } else {
                callback.onPermissionDenied();
            }
        }
    }

    /**
     * Permission callback interface
     */
    public interface PermissionCallback {
        void onPermissionGranted();
        void onPermissionDenied();
    }
} 