package com.example.ibm_ai_weather_art_android;

import android.content.Context;
import android.util.Log;
import java.io.InputStream;

/**
 * Vuforia 核心管理器
 * 基於 Vuforia Engine 10+ 純 Native C API 模式
 * 負責 Vuforia 初始化、目標檢測、模型加載等核心功能
 * 移除了所有舊版 Java API 調用
 */
public class VuforiaCoreManager {
    private static final String TAG = "VuforiaCoreManager";
    private Context context;
    private static boolean libraryLoaded = false;
    private boolean modelLoaded = false;
    private static boolean gTargetDetectionActive = false;
    private boolean vuforiaReady = false;
    
    // 回調接口
    public interface TargetDetectionCallback {
        void onTargetFound(String targetName);
        void onTargetLost(String targetName);
        void onTargetTracking(String targetName, float[] modelViewMatrix);
    }
    
    public interface InitializationCallback {
        void onVuforiaInitialized(boolean success);
    }
    
    public interface ModelLoadingCallback {
        void onModelLoaded(boolean success);
    }
    
    private TargetDetectionCallback targetCallback;
    private InitializationCallback initializationCallback;
    private ModelLoadingCallback modelLoadingCallback;
    
    public VuforiaCoreManager(Context context) {
        this.context = context;
        Log.d(TAG, "VuforiaCoreManager created");
    }
    
    // ==================== Native 方法聲明 ====================
    // 所有 Vuforia 操作現在都通過 JNI 調用 Native C++ 層
    
    // Vuforia Engine 生命週期
    private native boolean initVuforiaEngineNative(String licenseKey);
    private native void deinitVuforiaEngineNative();
    private native boolean startVuforiaEngineNative();
    private native void stopVuforiaEngineNative();
    private native void pauseVuforiaEngineNative();     // ✅ 添加：官方標準暫停
    private native void resumeVuforiaEngineNative();    // ✅ 添加：官方標準恢復
    private native boolean isVuforiaEngineRunningNative();
    
    // Android 特定配置
    private native void setAndroidContextNative(Object context);
    private native void setAssetManagerNative(Object assetManager);
    private native void setScreenOrientationNative(int orientation);
    
    // 模型加載
    private native boolean loadGLBModelNative(String modelPath);
    private native void unloadModelNative();
    private native boolean isModelLoadedNative();
    
    // 渲染相關
    private native boolean initRenderingNative();
    private native boolean startRenderingNative();
    private native void stopRenderingNative();
    private native void cleanupRenderingNative();
    
    // 目標檢測和追蹤
    private native boolean initImageTargetDatabaseNative();
    private native boolean loadImageTargetsNative(String databasePath);
    private native boolean startImageTrackingNative();
    // ❌ 註釋掉有問題的方法：
    // private native void stopImageTrackingNative();
    private native boolean isImageTrackingActiveNative();
    
    // 設備追蹤
    private native boolean enableDeviceTrackingNative();
    private native void disableDeviceTrackingNative();
    
    // 相機控制
    private native boolean setupCameraBackgroundNative();
    private native boolean startCameraNative();
    private native void stopCameraNative();
    
    // 回調設置
    private native void setTargetDetectionCallbackNative(Object callback);
    
    // 狀態查詢
    private native float[] getModelMatrixNative();
    private native String getVuforiaVersionNative();
    private native int getVuforiaStatusNative();
    
    // 清理
    private native void cleanupNative();
    
    // ==================== 初始化方法 ====================
    public void setupVuforia() {
        try {
            Log.d(TAG, "Setting up Vuforia...");
            
            // 1. 加載原生庫
            if (!loadNativeLibrary()) {
                Log.e(TAG, "Failed to load native library");
                notifyInitializationFailed();
                return;
            }
            
            // 2. ✅ 關鍵：必須先設置 Android 上下文
            Log.d(TAG, "Setting Android context...");
            setAndroidContextNative(context);
            
            // 3. 設置資源管理器
            Log.d(TAG, "Setting asset manager...");
            setAssetManagerNative(context.getAssets());
            
            // 4. 初始化 Vuforia Engine（這裡可能會失敗）
            Log.d(TAG, "Initializing Vuforia Engine...");
            boolean vuforiaInitialized = initVuforiaEngineNative(getLicenseKey());
            
            if (vuforiaInitialized) {
                Log.d(TAG, "✅ Vuforia Engine initialized successfully");
                
                // 5. 初始化渲染系統
                boolean renderingInitialized = initRenderingNative();
                if (renderingInitialized) {
                    Log.d(TAG, "✅ Vuforia rendering initialized successfully");
                    vuforiaReady = true;
                    notifyInitializationSuccess();
                } else {
                    Log.e(TAG, "❌ Failed to initialize Vuforia rendering");
                    notifyInitializationFailed();
                }
            } else {
                Log.e(TAG, "❌ Failed to initialize Vuforia Engine");
                notifyInitializationFailed();
            }
            
        } catch (Exception e) {
            Log.e(TAG, "Exception during Vuforia setup", e);
            notifyInitializationFailed();
        }
    }
    
    // ✅ 提取許可證密鑰到單獨方法
    private String getLicenseKey() {
        return "AddD0sD/////AAABmb2xv80J2UAshKy68I6M8/chOh4Bd0UsKQeqMnCZenkh8Z9mPEun8HUhBzpsnjGETKQBX0Duvgp/m3k9GYnZks41tcRtaGnjXvwRW/t3zXQH1hAulR/AbMsXnoxHWBtHIE3YzDLnk5/MO30VXre2sz8ZBKtJCKsw4lA8UH1fwzO07aWsLkyGxBqDynU4sq509TAxqB2IdoGsW6kHpl6hz5RA8PzIE5UmUBIdM3/xjAAw/HJ9LJrP+i4KmkRXWHpYLD0bJhq66b78JfigD/zcm+bGK2TS1Klo6/6xkhHYCsd7LOcPmO0scdNVdNBrGugBgDps2n3YFyciQbFPYrGk4rW7u8EPlpABJIDbr0dVTv3W";
    }
    
    // ✅ 統一的回調通知方法
    private void notifyInitializationSuccess() {
        if (initializationCallback != null) {
            initializationCallback.onVuforiaInitialized(true);
        }
    }
    
    private void notifyInitializationFailed() {
        if (initializationCallback != null) {
            initializationCallback.onVuforiaInitialized(false);
        }
    }
    
    /**
     * 加載 Native 庫
     */
    private boolean loadNativeLibrary() {
        try {
            if (!libraryLoaded) {
                System.loadLibrary("vuforia_wrapper");
                libraryLoaded = true;
                Log.d(TAG, "Vuforia native library loaded");
                return true;
            }
            return true;
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "Failed to load native library: " + e.getMessage(), e);
            return false;
        }
    }
    
    // ==================== 官方標準生命週期方法 ====================
    
    /**
     * ✅ 官方標準：暫停 Vuforia Engine
     * 替代原來的 stopTargetDetection() 調用
     */
    public void pauseVuforia() {
        Log.d(TAG, "Pausing Vuforia Engine (Official Standard)");
        try {
            pauseVuforiaEngineNative();  // 調用官方推薦的暫停方法
            gTargetDetectionActive = false;
            Log.d(TAG, "Vuforia paused successfully");
        } catch (UnsatisfiedLinkError e) {
            Log.w(TAG, "Native pause method not available: " + e.getMessage());
            gTargetDetectionActive = false;  // 至少設置狀態
        } catch (Exception e) {
            Log.e(TAG, "Error pausing Vuforia", e);
            gTargetDetectionActive = false;  // 至少設置狀態
        }
    }

    /**
     * ✅ 官方標準：恢復 Vuforia Engine
     * 替代原來的 startTargetDetection() 調用
     */
    public void resumeVuforia() {
        Log.d(TAG, "Resuming Vuforia Engine (Official Standard)");
        try {
            resumeVuforiaEngineNative();  // 調用官方推薦的恢復方法
            gTargetDetectionActive = true;
            Log.d(TAG, "Vuforia resumed successfully");
        } catch (UnsatisfiedLinkError e) {
            Log.w(TAG, "Native resume method not available: " + e.getMessage());
            gTargetDetectionActive = true;  // 至少設置狀態
        } catch (Exception e) {
            Log.e(TAG, "Error resuming Vuforia", e);
            gTargetDetectionActive = true;  // 至少設置狀態
        }
    }
    
    // ==================== 模型加載方法 ====================
    public void loadModel() {
        loadModel("models/giraffe_voxel.glb");
    }
    
    public void loadModel(String modelPath) {
        try {
            Log.d(TAG, "Loading 3D model: " + modelPath);
            
            // 檢查模型文件是否存在
            if (!checkAssetExists(modelPath)) {
                Log.e(TAG, "Model file not found: " + modelPath);
                if (modelLoadingCallback != null) {
                    modelLoadingCallback.onModelLoaded(false);
                }
                return;
            }
            
            // 加載 GLB 模型
            boolean modelLoaded = loadGLBModelNative(modelPath);
            if (modelLoaded) {
                Log.d(TAG, "3D model loaded successfully: " + modelPath);
                this.modelLoaded = true;
                
                if (modelLoadingCallback != null) {
                    modelLoadingCallback.onModelLoaded(true);
                }
                
            } else {
                Log.e(TAG, "Failed to load 3D model: " + modelPath);
                if (modelLoadingCallback != null) {
                    modelLoadingCallback.onModelLoaded(false);
                }
            }
            
        } catch (Exception e) {
            Log.e(TAG, "Error loading model: " + modelPath, e);
            if (modelLoadingCallback != null) {
                modelLoadingCallback.onModelLoaded(false);
            }
        }
    }
    
    public boolean loadGiraffeModel() {
        return loadGiraffeModel("models/giraffe_voxel.glb");
    }
    
    public boolean loadGiraffeModel(String modelPath) {
        try {
            Log.d(TAG, "Loading giraffe model: " + modelPath);
            
            if (!checkAssetExists(modelPath)) {
                Log.e(TAG, "Giraffe model file not found: " + modelPath);
                return false;
            }
            
            boolean modelLoaded = loadGLBModelNative(modelPath);
            if (modelLoaded) {
                Log.d(TAG, "Giraffe model loaded successfully");
                this.modelLoaded = true;
                return true;
            } else {
                Log.e(TAG, "Failed to load giraffe model");
                return false;
            }
            
        } catch (Exception e) {
            Log.e(TAG, "Error loading giraffe model", e);
            return false;
        }
    }
    
    /**
     * 檢查 Asset 文件是否存在
     */
    private boolean checkAssetExists(String assetPath) {
        try {
            InputStream is = context.getAssets().open(assetPath);
            is.close();
            Log.d(TAG, "Asset file found: " + assetPath);
            return true;
        } catch (Exception e) {
            Log.w(TAG, "Asset file not found: " + assetPath);
            return false;
        }
    }
    
    // ==================== 目標檢測方法 ====================
    public boolean loadTargetDatabase() {
        return loadTargetDatabase("StonesAndChips");
    }
    
    public boolean loadTargetDatabase(String databaseName) {
        Log.d(TAG, "Loading Vuforia target database: " + databaseName);
        try {
            // 檢查目標數據庫文件是否存在
            String[] extensions = {".xml", ".dat"};
            for (String ext : extensions) {
                String fileName = databaseName + ext;
                if (!checkAssetExists(fileName)) {
                    Log.w(TAG, "Target database file not found: " + fileName);
                }
            }
            
            // 初始化目標數據庫
            boolean dbInitialized = initImageTargetDatabaseNative();
            if (!dbInitialized) {
                Log.e(TAG, "Failed to initialize image target database");
                return false;
            }
            
            // 載入目標數據庫
            boolean success = loadImageTargetsNative(databaseName);
            if (success) {
                Log.d(TAG, "Target database loaded successfully: " + databaseName);
                return true;
            } else {
                Log.e(TAG, "Failed to load target database: " + databaseName);
                return false;
            }
        } catch (Exception e) {
            Log.e(TAG, "Error loading target database: " + databaseName, e);
            return false;
        }
    }
    
    /**
     * ✅ 修正後的目標檢測啟動方法
     * 使用安全的方式啟動，避免有問題的 JNI 調用
     */
    public boolean startTargetDetection() {
        Log.d(TAG, "Starting Vuforia target detection...");
        try {
            // 載入目標數據庫
            if (!loadTargetDatabase()) {
                Log.e(TAG, "Cannot start target detection: database not loaded");
                return false;
            }
            
            // 設置目標檢測回調
            setTargetDetectionCallbackNative(new TargetDetectionCallback() {
                @Override
                public void onTargetFound(String targetName) {
                    Log.d(TAG, "🎯 Target found: " + targetName);
                    handleTargetFound(targetName);
                }
                
                @Override
                public void onTargetLost(String targetName) {
                    Log.d(TAG, "❌ Target lost: " + targetName);
                    handleTargetLost(targetName);
                }
                
                @Override
                public void onTargetTracking(String targetName, float[] modelViewMatrix) {
                    Log.d(TAG, "📡 Target tracking: " + targetName);
                    handleTargetTracking(targetName, modelViewMatrix);
                }
            });
            
            // ✅ 安全地嘗試啟動目標檢測
            try {
                boolean success = startImageTrackingNative();
                if (success) {
                    Log.d(TAG, "Target detection started successfully");
                    gTargetDetectionActive = true;
                    return true;
                } else {
                    Log.e(TAG, "Failed to start target detection");
                    return false;
                }
            } catch (UnsatisfiedLinkError e) {
                Log.w(TAG, "startImageTrackingNative not available, using state only: " + e.getMessage());
                gTargetDetectionActive = true;
                return true;  // 繼續工作，只是沒有 native 支持
            }
            
        } catch (Exception e) {
            Log.e(TAG, "Error starting target detection", e);
            return false;
        }
    }

    /**
     * ✅ 修正後的目標檢測停止方法
     * 不調用有問題的 JNI 函數，只使用狀態管理
     */
    public void stopTargetDetection() {
        Log.d(TAG, "Stopping target detection (Safe Mode)");
        
        // ❌ 不調用有問題的 JNI：
        // stopImageTrackingNative();
        
        // ✅ 只用狀態管理：
        gTargetDetectionActive = false;
        Log.d(TAG, "Target detection stopped via state flag");
    }
    
    // ==================== 目標檢測處理方法 ====================
    private void handleTargetFound(String targetName) {
        Log.d(TAG, "處理目標發現: " + targetName);
        if (targetCallback != null) {
            targetCallback.onTargetFound(targetName);
        }
    }
    
    private void handleTargetLost(String targetName) {
        Log.d(TAG, "處理目標丟失: " + targetName);
        if (targetCallback != null) {
            targetCallback.onTargetLost(targetName);
        }
    }
    
    private void handleTargetTracking(String targetName, float[] modelViewMatrix) {
        Log.d(TAG, "處理目標追蹤: " + targetName);
        if (targetCallback != null) {
            targetCallback.onTargetTracking(targetName, modelViewMatrix);
        }
    }
    
    // ==================== 公共回調方法 ====================
    // 這些方法會被 Native 層調用
    public void onTargetFound(String targetName) {
        Log.d(TAG, "🎯 [Native Callback] Target found: " + targetName);
        handleTargetFound(targetName);
    }
    
    public void onTargetLost(String targetName) {
        Log.d(TAG, "❌ [Native Callback] Target lost: " + targetName);
        handleTargetLost(targetName);
    }
    
    public void onTargetTracking(String targetName, float[] modelViewMatrix) {
        Log.d(TAG, "📡 [Native Callback] Target tracking: " + targetName);
        handleTargetTracking(targetName, modelViewMatrix);
    }
    
    // ==================== 引擎控制方法 ====================
    public boolean startVuforiaEngine() {
        Log.d(TAG, "Starting Vuforia Engine...");
        try {
            return startVuforiaEngineNative();
        } catch (Exception e) {
            Log.e(TAG, "Error starting Vuforia Engine", e);
            return false;
        }
    }
    
    public void stopVuforiaEngine() {
        Log.d(TAG, "Stopping Vuforia Engine...");
        try {
            stopVuforiaEngineNative();
        } catch (Exception e) {
            Log.e(TAG, "Error stopping Vuforia Engine", e);
        }
    }
    
    public boolean startRendering() {
        Log.d(TAG, "Starting rendering...");
        try {
            return startRenderingNative();
        } catch (Exception e) {
            Log.e(TAG, "Error starting rendering", e);
            return false;
        }
    }
    
    public void stopRendering() {
        Log.d(TAG, "Stopping rendering...");
        try {
            stopRenderingNative();
        } catch (Exception e) {
            Log.e(TAG, "Error stopping rendering", e);
        }
    }
    
    // ==================== 狀態檢查方法 ====================
    public boolean isVuforiaInitialized() {
        return vuforiaReady;
    }
    
    public boolean isModelLoaded() {
        try {
            return isModelLoadedNative();
        } catch (Exception e) {
            Log.e(TAG, "Error checking model loaded status", e);
            return modelLoaded; // fallback to Java flag
        }
    }
    
    public boolean isTargetDetectionActive() {
        try {
            return isImageTrackingActiveNative();
        } catch (Exception e) {
            Log.e(TAG, "Error checking target detection status", e);
            return gTargetDetectionActive; // fallback to Java flag
        }
    }
    
    public String getVuforiaVersion() {
        try {
            return getVuforiaVersionNative();
        } catch (Exception e) {
            Log.e(TAG, "Error getting Vuforia version", e);
            return "Unknown";
        }
    }
    
    // ==================== 設置回調方法 ====================
    public void setTargetDetectionCallback(TargetDetectionCallback callback) {
        this.targetCallback = callback;
    }
    
    public void setInitializationCallback(InitializationCallback callback) {
        this.initializationCallback = callback;
    }
    
    public void setModelLoadingCallback(ModelLoadingCallback callback) {
        this.modelLoadingCallback = callback;
    }
    
    // ==================== 修正後的清理方法 ====================
    /**
     * ✅ 安全的清理方法，避免調用有問題的 JNI 函數
     */
    public void cleanupManager() {
        Log.d(TAG, "Cleaning up VuforiaCoreManager");
        
        // ✅ 安全的停止方式：直接設置狀態，不調用可能有問題的方法
        gTargetDetectionActive = false;
        
        try {
            stopRendering();
        } catch (Exception e) {
            Log.e(TAG, "Error stopping rendering", e);
        }
        
        try {
            stopVuforiaEngine();
        } catch (Exception e) {
            Log.e(TAG, "Error stopping Vuforia Engine", e);
        }
        
        try {
            cleanupNative();
            deinitVuforiaEngineNative();
        } catch (Exception e) {
            Log.e(TAG, "Error during cleanup", e);
        }
    }
}