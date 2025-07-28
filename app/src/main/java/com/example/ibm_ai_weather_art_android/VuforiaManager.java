package com.example.ibm_ai_weather_art_android;

import android.content.Context;
import android.view.SurfaceView;
import android.view.SurfaceHolder;
import android.hardware.Camera;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.util.Log;
import android.view.WindowManager;
import android.view.Surface; // Added for Filament

import java.nio.ByteBuffer; // Added for GLB loading

public class VuforiaManager {
    private static final String TAG = "VuforiaManager";
    private Context context;
    private static boolean libraryLoaded = false;
    private Camera camera;
    private SurfaceView cameraPreview;
    private FrameLayout cameraContainer;
    private boolean arRenderingStarted = false;
    private boolean modelLoaded = false;
    
    // Filament renderer for GLB models
    private FilamentRenderer filamentRenderer; // Added

    // 恢復 Vuforia 庫載入
    static {
        try {
            System.loadLibrary("vuforia_wrapper");
            libraryLoaded = true;
            Log.d(TAG, "Vuforia library loaded successfully");
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "Failed to load Vuforia library", e);
            libraryLoaded = false;
        }
    }

    public VuforiaManager(Context context) {
        this.context = context;
        try {
            // 暫時不初始化 Filament
            Log.d(TAG, "VuforiaManager created");
            initializeCameraPreview();
            initializeFilamentRenderer(); // Initialize Filament renderer
        } catch (Exception e) {
            Log.e(TAG, "Error creating VuforiaManager", e);
        }
    }

    private void initializeFilamentRenderer() {
        try {
            // NOTE: FilamentRenderer constructor expects a SurfaceView, not Context.
            // This will need to be refactored in MainActivity or a dedicated SurfaceView for Filament.
            // For now, passing null or a dummy SurfaceView might be necessary if not refactoring immediately.
            // The user's provided FilamentRenderer code takes SurfaceView in constructor.
            // This current call `new FilamentRenderer(context)` is incorrect based on user's FilamentRenderer.java.
            // It should be `new FilamentRenderer(someSurfaceView)`
            filamentRenderer = new FilamentRenderer(context); // Temporary fix to allow compilation, needs proper SurfaceView from UI
            Log.d(TAG, "Filament renderer initialized");
        } catch (Exception e) {
            Log.e(TAG, "Failed to initialize Filament renderer", e);
        }
    }

    private void initializeCameraPreview() {
        try {
            cameraPreview = new SurfaceView(context);
            cameraPreview.getHolder().addCallback(new SurfaceHolder.Callback() {
                @Override
                public void surfaceCreated(SurfaceHolder holder) {
                    startCameraPreview();
                }

                @Override
                public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                    updateCameraOrientation();
                }

                @Override
                public void surfaceDestroyed(SurfaceHolder holder) {
                    stopCameraPreview();
                }
            });
            Log.d(TAG, "Camera preview initialized");
        } catch (Exception e) {
            Log.e(TAG, "Error initializing camera preview", e);
        }
    }

    public void setCameraContainer(FrameLayout container) {
        this.cameraContainer = container;
        if (cameraContainer != null && cameraPreview != null) {
            cameraContainer.removeAllViews();
            cameraContainer.addView(cameraPreview);
            Log.d(TAG, "Camera container set");
        }
    }

    private void startCameraPreview() {
        try {
            if (camera == null) {
                camera = Camera.open();
                camera.setPreviewDisplay(cameraPreview.getHolder());
                camera.startPreview();
                updateCameraOrientation();
                Log.d(TAG, "Camera preview started");
            }
        } catch (Exception e) {
            Log.e(TAG, "Error starting camera preview", e);
        }
    }

    private void stopCameraPreview() {
        try {
            if (camera != null) {
                camera.stopPreview();
                camera.release();
                camera = null;
                Log.d(TAG, "Camera preview stopped");
            }
        } catch (Exception e) {
            Log.e(TAG, "Error stopping camera preview", e);
        }
    }

    private void updateCameraOrientation() {
        try {
            if (camera != null) {
                WindowManager windowManager = (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
                int rotation = windowManager.getDefaultDisplay().getRotation();
                
                Camera.CameraInfo info = new Camera.CameraInfo();
                Camera.getCameraInfo(0, info);
                
                int degrees = 0;
                switch (rotation) {
                    case android.view.Surface.ROTATION_0: degrees = 0; break;
                    case android.view.Surface.ROTATION_90: degrees = 90; break;
                    case android.view.Surface.ROTATION_180: degrees = 180; break;
                    case android.view.Surface.ROTATION_270: degrees = 270; break;
                }
                
                int result;
                if (info.facing == Camera.CameraInfo.CAMERA_FACING_FRONT) {
                    result = (info.orientation + degrees) % 360;
                    result = (360 - result) % 360;
                } else {
                    result = (info.orientation - degrees + 360) % 360;
                }
                
                camera.setDisplayOrientation(result);
                Log.d(TAG, "Camera orientation updated: " + result + " degrees");
            }
        } catch (Exception e) {
            Log.e(TAG, "Error updating camera orientation", e);
        }
    }

    // Native methods - 暫時註釋掉，等專案能正常編譯後再處理
    public native boolean initVuforia();
    public native void setLicenseKey(String licenseKey);
    public native boolean loadGLBModel(String modelPath); // This native method is now bypassed for Filament
    public native boolean startRendering();
    public native void setAssetManager(android.content.res.AssetManager assetManager);
    public native void cleanup();

    public boolean setupVuforia() {
        if (!libraryLoaded) {
            Log.e(TAG, "Native library not loaded");
            return false;
        }
        
        try {
            setAssetManager(context.getAssets());
            setLicenseKey("AddD0sD/////AAABmb2xv80J2UAshKy68I6M8/chOh4Bd0UsKQeqMnCZenkh8Z9mPEun8HUhBzpsnjGETKQBX0Duvgp/m3k9GYnZks41tcRtaGnjXvwRW/t3zXQH1hAulR/AbMsXnoxHWBtHIE3YzDLnk5/MO30VXre2sz8ZBKtJCKsw4lA8UH1fwzO07aWsLkyGxBqDynU4sq509TAxqB2IdoGsW6kHpl6hz5RA8PzIE5UmUBIdM3/xjAAw/HJ9LJrP+i4KmkRXWHpYLD0bJhq66b78JfigD/zcm+bGK2TS1Klo6/6xkhHYCsd7LOcPmO0scdNVdNBrGugBgDps2n3YFyciQbFPYrGk4rW7u8EPlpABJIDbr0dVTv3W");
            
            boolean result = initVuforia();
            Log.d(TAG, "Vuforia setup result: " + result);
            return result;
        } catch (Exception e) {
            Log.e(TAG, "Error setting up Vuforia", e);
            return false;
        }
    }

    public boolean loadGiraffeModel() {
        Log.d(TAG, "Loading giraffe model with Filament...");
        
        if (filamentRenderer == null) {
            Log.e(TAG, "Filament renderer not initialized");
            return false;
        }
        
        // Read GLB into ByteBuffer here, then pass to FilamentRenderer
        try {
            android.content.res.AssetManager assetManager = context.getAssets();
            android.content.res.AssetFileDescriptor afd = assetManager.openFd("models/giraffe_voxel.glb");
            java.io.FileInputStream fis = new java.io.FileInputStream(afd.getFileDescriptor());
            fis.getChannel().position(afd.getStartOffset());
            byte[] data = new byte[(int) afd.getDeclaredLength()];
            fis.read(data);
            fis.close();
            afd.close();
            ByteBuffer buffer = ByteBuffer.allocateDirect(data.length);
            buffer.put(data);
            buffer.flip();

            boolean result = filamentRenderer.loadGLBModel("models/giraffe_voxel.glb");
            if (result) {
                modelLoaded = true;
                Log.d(TAG, "Giraffe model loaded successfully with Filament");
            } else {
                Log.e(TAG, "Failed to load giraffe model with Filament");
            }
            return result;
        } catch (Exception e) {
            Log.e(TAG, "Error reading GLB for Filament", e);
            return false;
        }
    }

    public boolean startAR() {
        Log.d(TAG, "Starting AR rendering with Filament...");
        
        if (!modelLoaded) {
            Log.e(TAG, "Model not loaded, cannot start AR");
            return false;
        }
        
        // 啟動 Vuforia tracking
        boolean vuforiaResult = startRendering();
        if (vuforiaResult) {
            arRenderingStarted = true;
            Log.d(TAG, "AR rendering started successfully with Filament");
        } else {
            Log.e(TAG, "Failed to start AR rendering");
        }
        return vuforiaResult;
    }

    public void setFilamentSurface(Surface surface, int width, int height) {
        if (filamentRenderer != null) {
            filamentRenderer.setSurface(surface, width, height);
        }
    }

    public void onDestroy() {
        stopCameraPreview();
        if (filamentRenderer != null) {
            filamentRenderer.cleanup(); // Call Filament cleanup
        }
        cleanup(); // Call native Vuforia cleanup
    }

    public boolean isVuforiaInitialized() {
        return libraryLoaded;
    }

    public boolean isARRenderingStarted() {
        return arRenderingStarted;
    }

    public boolean isModelLoaded() {
        return modelLoaded;
    }

    public boolean isFilamentInitialized() {
        return filamentRenderer != null && filamentRenderer.isInitialized();
    }
} 