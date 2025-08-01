package com.example.ibm_ai_weather_art_android;

import android.content.Context;
import android.util.Log;
import java.io.InputStream;
import android.os.Handler;
import android.view.Surface;
import android.view.SurfaceView;
import android.view.SurfaceHolder;
import android.Manifest;
import android.content.pm.PackageManager;
import androidx.core.content.ContextCompat;

/**
 * Vuforia 核心管理器
 * 基於 Vuforia Engine 10+ 純 Native C API 模式
 * 負責 Vuforia 初始化、目標檢測、模型加載等核心功能
 * 移除了所有舊版 Java API 調用
 */
public class VuforiaCoreManager {
    private static volatile boolean isInitialized = false;
    private static volatile boolean isInitializing = false;
    private static final Object initLock = new Object();
    private static Thread currentInitThread = null;
    private static final String TAG = "VuforiaCoreManager";
    private Context context;
    private static boolean libraryLoaded = false;
    private boolean modelLoaded = false;
    private static boolean gTargetDetectionActive = false;
    private boolean vuforiaReady = false;
        // OpenGL 渲染相關
    private native boolean initializeOpenGLResourcesNative();
    private native boolean setupVideoBackgroundRenderingNative();
    private native boolean validateRenderingSetupNative();
    private native void renderFrameWithVideoBackgroundNative();
    
    // Surface 管理相關
    private native void onSurfaceChangedNative(int width, int height);
    // 🔧 添加：渲染相關變量
    private Thread renderingThread;
    private volatile boolean isRenderingActive = false;
    
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
    
    // 🔧 添加：渲染循環
    private native void renderFrameNative();
    
    // ==================== 渲染循环控制 - 解决编译错误的关键 ====================
    private native void stopRenderingLoopNative();
    private native void startRenderingLoopNative();
    private native boolean isRenderingActiveNative();
    
    // ==================== 相机和状态查询 ====================
    private native boolean isCameraActiveNative();
    
    // ==================== Surface管理 ====================
    private native void setSurfaceNative(Object surface);
    private native void onSurfaceCreatedNative(int width, int height);
    private native void onSurfaceDestroyedNative();
    
    // ==================== 诊断方法 ====================
    private native String getEngineStatusDetailNative();
    private native String getMemoryUsageNative();
    
    // ==================== 相机权限检查方法 ====================
    private boolean mPermissionChecked = false;
    
    /**
     * 初始化前的权限检查 - 配合 C++ preCheckCameraPermission()
     */
    public boolean checkCameraPermissionBeforeInit() {
        if (context == null) {
            Log.e(TAG, "Context not set before permission check");
            return false;
        }
        boolean hasPermission = ContextCompat.checkSelfPermission(context, Manifest.permission.CAMERA) 
            == PackageManager.PERMISSION_GRANTED;
        Log.i(TAG, "Camera permission status: " + (hasPermission ? "GRANTED" : "DENIED"));
        mPermissionChecked = true;
        return hasPermission;
    }
    
    /**
     * 完整的 Vuforia 初始化流程 - 包含权限检查
     */
    public boolean initializeVuforiaWithPermissionCheck(String licenseKey) {
        Log.i(TAG, "Starting Vuforia initialization with permission check...");
        
        // 1. 检查 Context 是否设置
        if (context == null) {
            Log.e(TAG, "Context must be set before initialization");
            return false;
        }
        
        // 2. 检查相机权限
        if (!checkCameraPermissionBeforeInit()) {
            Log.e(TAG, "Camera permission not granted - cannot initialize Vuforia");
            return false;
        }
        
        // 3. 设置 Android Context（必须在引擎创建前）
        setAndroidContextNative(context);
        
        // 4. 初始化 Vuforia Engine
        boolean success = initVuforiaEngineNative(licenseKey);
        if (success) {
            Log.i(TAG, "✅ Vuforia initialized successfully with camera permission");
            return true;
        } else {
            Log.e(TAG, "❌ Vuforia initialization failed");
            return false;
        }
    }
    
    /**
     * 检查当前权限状态
     */
    public boolean isPermissionGranted() {
        if (context == null) return false;
        return ContextCompat.checkSelfPermission(context, Manifest.permission.CAMERA) 
            == PackageManager.PERMISSION_GRANTED;
    }
    
    /**
     * 启动相机前的安全检查
     */
    public boolean startCameraWithPermissionCheck() {
        Log.i(TAG, "Starting camera with permission check...");
        
        // 双重检查权限
        if (!isPermissionGranted()) {
            Log.e(TAG, "Camera permission lost - cannot start camera");
            return false;
        }
        
        // 检查 Vuforia 引擎状态
        if (!isVuforiaEngineRunningNative()) {
            Log.e(TAG, "Vuforia engine not running - cannot start camera");
            return false;
        }
        
        // 调用 C++ 方法启动相机
        return startCameraNative();
    }
    
    // ==================== 初始化方法 ====================
    public void setupVuforia() {
        synchronized (initLock) {
            // ✅ 如果已經初始化成功，直接返回成功
            if (isInitialized) {
                Log.d(TAG, "✅ Vuforia already initialized successfully, skipping...");
                new android.os.Handler(android.os.Looper.getMainLooper()).post(() -> {
                    notifyInitializationSuccess();
                });
                return;
            }
            
            // ✅ 如果正在初始化，忽略重複調用
            if (isInitializing) {
                Log.w(TAG, "⚠️ Vuforia initialization already in progress, ignoring duplicate call");
                return;
            }
            
            // ✅ 中斷舊線程（如果存在）
            if (currentInitThread != null && currentInitThread.isAlive()) {
                Log.w(TAG, "🛑 Stopping previous initialization thread");
                currentInitThread.interrupt();
                try {
                    currentInitThread.join(500);
                } catch (InterruptedException e) {
                    Log.w(TAG, "Interrupted while waiting for previous thread", e);
                }
            }
            
            // ✅ 標記開始初始化
            isInitializing = true;
        }
        
        // ✅ 創建單一初始化線程
        currentInitThread = new Thread(() -> {
            Log.d(TAG, "🚀 Starting single Vuforia initialization thread...");
            
            final boolean[] success = {false};  // ✅ 使用 final 數組解決 lambda 問題
            final int maxAttempts = 3;
            
            try {
                for (int attempt = 1; attempt <= maxAttempts; attempt++) {
                    // 檢查線程是否被中斷
                    if (Thread.currentThread().isInterrupted()) {
                        Log.w(TAG, "Thread interrupted, stopping initialization");
                        break;
                    }
                    
                    Log.d(TAG, "🔄 Vuforia初始化嘗試 " + attempt + "/" + maxAttempts);
                    
                    try {
                        // 1. 加載原生庫
                        if (!loadNativeLibrary()) {
                            Log.e(TAG, "第" + attempt + "次嘗試：Failed to load native library");
                            if (attempt < maxAttempts) {
                                Thread.sleep(500);
                                continue;
                            } else {
                                break;
                            }
                        }
                        
                        // 2. 檢查相機權限
                        Log.d(TAG, "第" + attempt + "次嘗試：Checking camera permission...");
                        if (!checkCameraPermissionBeforeInit()) {
                            Log.e(TAG, "第" + attempt + "次嘗試：Camera permission not granted");
                            if (attempt < maxAttempts) {
                                Thread.sleep(500);
                                continue;
                            } else {
                                break;
                            }
                        }
                        
                        // 3. 設置 Android 上下文
                        Log.d(TAG, "第" + attempt + "次嘗試：Setting Android context...");
                        setAndroidContextNative(context);
                        
                        // 4. 設置資源管理器
                        Log.d(TAG, "第" + attempt + "次嘗試：Setting asset manager...");
                        setAssetManagerNative(context.getAssets());
                        
                        // 5. 初始化 Vuforia Engine
                        Log.d(TAG, "第" + attempt + "次嘗試：Initializing Vuforia Engine...");
                        boolean vuforiaInitialized = initVuforiaEngineNative(getLicenseKey());
                        
                        if (vuforiaInitialized) {
                            Log.d(TAG, "✅ 第" + attempt + "次嘗試：Vuforia Engine initialized successfully!");
                            
                            // 5. 初始化渲染系統
                            boolean renderingInitialized = initRenderingNative();
                            if (renderingInitialized) {
                                Log.d(TAG, "✅ 第" + attempt + "次嘗試：Vuforia rendering initialized successfully!");
                                
                                // ✅ 標記為永久成功
                                synchronized (initLock) {
                                    vuforiaReady = true;
                                    isInitialized = true;  // 這個永遠不會重置
                                    success[0] = true;  // ✅ 使用數組方式
                                }
                                break;
                            } else {
                                Log.e(TAG, "⚠️ 第" + attempt + "次嘗試：Failed to initialize Vuforia rendering");
                            }
                        } else {
                            Log.e(TAG, "⚠️ 第" + attempt + "次嘗試：Failed to initialize Vuforia Engine");
                        }
                        
                        // 等待後重試
                        if (attempt < maxAttempts && !Thread.currentThread().isInterrupted()) {
                            int waitTime = 1000 * attempt;
                            Log.d(TAG, "😴 等待 " + waitTime + "ms 後進行第" + (attempt + 1) + "次嘗試...");
                            Thread.sleep(waitTime);
                        }
                        
                    } catch (InterruptedException e) {
                        Log.w(TAG, "Initialization thread interrupted", e);
                        Thread.currentThread().interrupt();
                        break;
                    } catch (Exception e) {
                        Log.e(TAG, "❌ 第" + attempt + "次嘗試出現異常: " + e.getMessage(), e);
                        if (attempt < maxAttempts && !Thread.currentThread().isInterrupted()) {
                            try {
                                Thread.sleep(1000 * attempt);
                            } catch (InterruptedException ie) {
                                Thread.currentThread().interrupt();
                                break;
                            }
                        }
                    }
                }
            } finally {
                // ✅ 釋放初始化鎖，但保持 isInitialized 狀態
                synchronized (initLock) {
                    isInitializing = false;
                    if (currentInitThread == Thread.currentThread()) {
                        currentInitThread = null;
                    }
                }
            }
            
            // ✅ 通知結果
            new android.os.Handler(android.os.Looper.getMainLooper()).post(() -> {
                if (success[0]) {  // ✅ 使用數組方式讀取
                    Log.d(TAG, "🎉 Vuforia permanently initialized! No more threads needed.");
                    notifyInitializationSuccess();
                } else {
                    Log.e(TAG, "❌ Vuforia initialization failed after " + maxAttempts + " attempts");
                    notifyInitializationFailed();
                }
            });
            
        }, "VuforiaInitThread");
        
        currentInitThread.start();
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
        
        // 🔧 添加：停止渲染循環
        stopRenderingLoop();
        
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
            
            // 🔧 添加：如果Surface已經準備好，重新啟動渲染
            // 這會在 surfaceCreated 回調中自動處理
            
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
    public void forceResetForTesting() {
        synchronized (initLock) {
            Log.w(TAG, "🔄 Force resetting Vuforia state (testing only)");
            isInitialized = false;
            isInitializing = false;
            vuforiaReady = false;
            
            if (currentInitThread != null && currentInitThread.isAlive()) {
                currentInitThread.interrupt();
                currentInitThread = null;
            }
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
            if (!isVuforiaInitialized()) {
                Log.e(TAG, "Cannot start engine - Vuforia not initialized");
                return false;
            }
            
            // 1. 啟動 Vuforia 引擎
            boolean engineStarted = startVuforiaEngineNative();
            
            if (engineStarted) {
                Log.d(TAG, "✅ Vuforia Engine started successfully");
                // ⏳ OpenGL 初始化會在有上下文時自動進行
                Log.d(TAG, "⏳ OpenGL 初始化會在有上下文時自動進行");

                return true;
            } else {
                Log.e(TAG, "❌ Failed to start Vuforia Engine");
                return false;
            }
        } catch (Exception e) {
            Log.e(TAG, "Error starting Vuforia Engine", e);
            return false;
        }
    }

    // ✅ 添加新方法：初始化 Vuforia OpenGL
    public boolean initializeVuforiaOpenGLWhenReady() {
        Log.d(TAG, "🎨 Initializing Vuforia OpenGL rendering (with context)...");
        
        try {
            // 1. 初始化 OpenGL 資源
            boolean glInit = initializeOpenGLResourcesNative();
            Log.d(TAG, "OpenGL initialized: " + glInit);
            
            // 2. 設置視頻背景渲染
            boolean bgSetup = setupVideoBackgroundRenderingNative();
            Log.d(TAG, "Video background setup: " + bgSetup);
            
            // 3. 驗證渲染設置
            boolean renderValid = validateRenderingSetupNative();
            Log.d(TAG, "Rendering setup valid: " + renderValid);
            
            // 4. 開始持續渲染
            if (glInit && bgSetup && renderValid) {
                startContinuousVuforiaRendering();
                Log.d(TAG, "✅ OpenGL rendering initialized successfully with context");
                return true;
            } else {
                Log.e(TAG, "❌ Failed to setup OpenGL rendering properly");
                return false;
            }
            
        } catch (Exception e) {
            Log.e(TAG, "❌ Error initializing Vuforia OpenGL", e);
            return false;
        }
    }

    // ✅ 添加新方法：持續渲染
    private void startContinuousVuforiaRendering() {
        Log.d(TAG, "🚀 Starting continuous Vuforia rendering...");
        
        isRenderingActive = true;
        
        // 創建渲染線程
        if (renderingThread == null || !renderingThread.isAlive()) {
            renderingThread = new Thread(() -> {
                Log.d(TAG, "📸 Vuforia rendering thread started");
                
                while (isRenderingActive && isVuforiaEngineRunningNative()) {
                    try {
                        // ⭐ 關鍵：持續調用 Vuforia 渲染
                        renderFrameWithVideoBackgroundNative();
                        Thread.sleep(16); // 60 FPS
                    } catch (InterruptedException e) {
                        Log.d(TAG, "Rendering thread interrupted");
                        break;
                    } catch (Exception e) {
                        Log.e(TAG, "Rendering error: " + e.getMessage());
                        // 繼續運行，不要停止
                    }
                }
                
                Log.d(TAG, "📸 Vuforia rendering thread stopped");
            });
            
            renderingThread.start();
        }
    }
    
    private boolean isVuforiaRunning() {
        return isVuforiaInitialized() && isRenderingActive;
    }
    
    private void startContinuousRendering() {
        Log.d(TAG, "Starting continuous rendering...");
        
        setupCameraBackgroundNative();
        
        // ⭐ 測試：手動啟動一次渲染循環
        new android.os.Handler(android.os.Looper.getMainLooper()).postDelayed(() -> {
            try {
                for (int i = 0; i < 10; i++) {
                    renderFrameNative();
                    Thread.sleep(50);
                }
                Log.d(TAG, "📷 Manual render frames completed");
            } catch (Exception e) {
                Log.e(TAG, "Manual render failed: " + e.getMessage());
            }
        }, 1000);
        
        Log.d(TAG, "✅ Rendering system ready (stopped infinite loop)");
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
        synchronized (initLock) {
            return isInitialized;  // 使用新的靜態標記
        }
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
        
        // 🔧 添加：停止渲染循環
        stopRenderingLoop();
        
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
    
    // ==================== 🔧 新增：Surface 管理 ====================
    
    /**
     * 🔧 修复：停止渲染循环 - 解决编译错误的核心方法
     */
    private void stopRenderingLoop() {
        Log.d(TAG, "🛑 Stopping rendering loop...");
        try {
            // 调用native方法停止渲染循环
            stopRenderingLoopNative();
            isRenderingActive = false;
            // 中断Java层的渲染线程（如果存在）
            if (renderingThread != null && renderingThread.isAlive()) {
                renderingThread.interrupt();
                try {
                    renderingThread.join(1000); // 等待最多1秒
                } catch (InterruptedException e) {
                    Log.w(TAG, "Interrupted while stopping rendering thread", e);
                }
                renderingThread = null;
            }
            Log.d(TAG, "✅ Rendering loop stopped successfully");
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "❌ Native method not found: stopRenderingLoopNative", e);
            // 至少设置状态标志
            isRenderingActive = false;
        } catch (Exception e) {
            Log.e(TAG, "❌ Error stopping rendering loop", e);
            isRenderingActive = false;
        }
    }
    
    /**
     * 设置渲染Surface - 用于更好的相机显示控制
     */
    public void setupRenderingSurface(SurfaceView surfaceView) {
        Log.d(TAG, "🖼️ Setting up rendering surface...");
        try {
            if (surfaceView != null) {
                // 设置Surface到native层
                setSurfaceNative(surfaceView.getHolder().getSurface());
                // 设置Surface生命周期回调
                surfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        Log.d(TAG, "🖼️ Surface created - Ready for rendering");
                        // 不需要特別處理，等待 surfaceChanged
                    }

                    @Override
                    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                        Log.d(TAG, "🖼️ Surface changed: " + width + "x" + height);
                        try {
                            // 通知 native 層 surface 變化
                            onSurfaceChangedNative(width, height);
                            
                            // 如果 Vuforia 已經運行但 OpenGL 沒初始化，現在初始化
                            if (isVuforiaEngineRunningNative() && !isRenderingActive) {
                                Log.d(TAG, "🎨 Surface ready - Starting OpenGL rendering");
                                Log.d(TAG, "⏳ OpenGL 初始化會在有上下文時自動進行");;
                            }
                            
                        } catch (Exception e) {
                            Log.e(TAG, "Error handling surface change", e);
                        }
                    }

                    @Override
                    public void surfaceDestroyed(SurfaceHolder holder) {
                        Log.d(TAG, "🖼️ Surface destroyed");
                        
                        // 停止渲染
                        isRenderingActive = false;
                        
                        try {
                            onSurfaceDestroyedNative();
                        } catch (Exception e) {
                            Log.e(TAG, "Error handling surface destruction", e);
                        }
                    }
                });
                Log.d(TAG, "✅ Rendering surface setup completed");
            } else {
                Log.e(TAG, "❌ SurfaceView is null");
            }
        } catch (Exception e) {
            Log.e(TAG, "❌ Error setting up rendering surface", e);
        }
    }
     /** 處理 Surface 變化
     */
    public void handleSurfaceChanged(int width, int height) {
        Log.d(TAG, "🖼️ Handling surface change: " + width + "x" + height);
        
        try {
            // 通知 native 層 surface 變化
            onSurfaceChangedNative(width, height);
            
            // 如果 Vuforia 已經運行但 OpenGL 沒初始化，現在初始化
            if (isVuforiaEngineRunningNative() && !isRenderingActive) {
                Log.d(TAG, "🎨 Surface ready - Initializing OpenGL rendering");
                if (initializeVuforiaOpenGLWhenReady()) {
                    Log.d(TAG, "✅ OpenGL initialized successfully after surface change");
                } else {
                    Log.e(TAG, "❌ Failed to initialize OpenGL after surface change");
                }
            }
            
        } catch (Exception e) {
            Log.e(TAG, "❌ Error handling surface change", e);
        }
    }

    /**
     * 安全渲染方法
     */
    public void renderFrameSafely() {
        if (!isReadyForRendering()) {
            return; // 靜默返回，不要打印太多日誌
        }
        
        try {
            // 🔥 關鍵：這會渲染相機背景 + AR 內容
            renderFrameWithVideoBackgroundNative();
        } catch (Exception e) {
            Log.e(TAG, "Rendering error: " + e.getMessage());
            // 不要停止渲染，繼續嘗試
        }
    }

    /**
     * 檢查是否可以開始渲染
     */
    public boolean isReadyForRendering() {
        try {
            return isVuforiaEngineRunningNative() && 
                   isRenderingActive && 
                   validateRenderingSetupNative();
        } catch (Exception e) {
            Log.e(TAG, "Error checking rendering readiness", e);
            return false;
        }
    }
    
    /**
     * 檢查 OpenGL 是否已初始化
     */
    public boolean isOpenGLInitialized() {
        try {
            return validateRenderingSetupNative();
        } catch (Exception e) {
            Log.e(TAG, "Error checking OpenGL status", e);
            return false;
        }
    }
    
    /**
     * 獲取渲染狀態診斷信息
     */
    public String getRenderingDiagnostics() {
        try {
            StringBuilder diag = new StringBuilder();
            diag.append("OpenGL initialized: ").append(isOpenGLInitialized()).append("\n");
            diag.append("Vuforia running: ").append(isVuforiaEngineRunningNative()).append("\n");
            diag.append("Camera active: ").append(isCameraActiveNative()).append("\n");
            diag.append("Rendering active: ").append(isRenderingActive).append("\n");
            return diag.toString();
        } catch (Exception e) {
            return "Diagnostics error: " + e.getMessage();
        }
    }
    public boolean validateRenderingSetupSafely() {
    Log.d(TAG, "🔍 Validating rendering setup safely...");
    
    try {
        // 檢查 Vuforia 引擎是否運行
        if (!isVuforiaEngineRunningNative()) {
            Log.w(TAG, "⚠️ Vuforia engine not running - rendering setup invalid");
            return false;
        }
        
        // 檢查 OpenGL 上下文是否有效
        boolean isGLValid = validateRenderingSetupNative();
        Log.d(TAG, "OpenGL context valid: " + isGLValid);
        
        // 檢查相機是否活躍
        boolean isCameraActive = isCameraActiveNative();
        Log.d(TAG, "Camera active: " + isCameraActive);
        
        // 檢查渲染狀態
        boolean isRenderingReady = isRenderingActiveNative();
        Log.d(TAG, "Rendering ready: " + isRenderingReady);
        
        boolean result = isGLValid && isCameraActive;
        Log.d(TAG, "✅ Rendering setup validation result: " + result);
        
        return result;
        
    } catch (UnsatisfiedLinkError e) {
        Log.e(TAG, "❌ Native method not available: validateRenderingSetupSafely", e);
        // 回退到基本檢查
        return isVuforiaInitialized() && isRenderingActive;
        
    } catch (Exception e) {
        Log.e(TAG, "❌ Error validating rendering setup", e);
        return false;
    }
}

/**
 * 初始化 OpenGL 資源 - 解決 MainActivity 第261行編譯錯誤
 * 這個方法被 MainActivity.java:261 調用
 */
    public boolean initializeOpenGLResources() {
        Log.d(TAG, "🎨 Initializing OpenGL resources...");
        
        try {
            // 檢查前置條件
            if (!isVuforiaEngineRunningNative()) {
                Log.e(TAG, "❌ Cannot initialize OpenGL - Vuforia engine not running");
                return false;
            }
            
            // 1. 初始化 OpenGL 資源
            boolean glResourcesInit = initializeOpenGLResourcesNative();
            Log.d(TAG, "OpenGL resources initialized: " + glResourcesInit);
            
            if (!glResourcesInit) {
                Log.e(TAG, "❌ Failed to initialize OpenGL resources");
                return false;
            }
            
            // 2. 設置視頻背景渲染
            boolean videoBackgroundSetup = setupVideoBackgroundRenderingNative();
            Log.d(TAG, "Video background rendering setup: " + videoBackgroundSetup);
            
            if (!videoBackgroundSetup) {
                Log.w(TAG, "⚠️ Video background setup failed, but continuing...");
            }
            
            // 3. 驗證整體設置
            boolean setupValid = validateRenderingSetupNative();
            Log.d(TAG, "Overall rendering setup valid: " + setupValid);
            
            // 4. 如果一切正常，標記渲染為活躍
            if (glResourcesInit && setupValid) {
                isRenderingActive = true;
                Log.d(TAG, "✅ OpenGL resources initialized successfully");
                
                // 可選：啟動渲染循環
                startRenderingLoopNative();
                
                return true;
            } else {
                Log.e(TAG, "❌ OpenGL initialization incomplete");
                return false;
            }
            
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "❌ Native method not available: initializeOpenGLResources", e);
            
            // 回退策略：至少設置狀態標記
            isRenderingActive = true;
            Log.w(TAG, "⚠️ Using fallback OpenGL initialization");
            return true;
            
        } catch (Exception e) {
            Log.e(TAG, "❌ Error initializing OpenGL resources", e);
            return false;
        }
    }

    // ==================== 🔧 額外的輔助方法 ====================

    /**
     * 檢查 OpenGL 上下文是否準備就緒
     */
    public boolean isOpenGLContextReady() {
        try {
            return validateRenderingSetupNative() && isRenderingActiveNative();
        } catch (Exception e) {
            Log.e(TAG, "Error checking OpenGL context", e);
            return false;
        }
    }

    /**
     * 強制重新初始化 OpenGL（當上下文丟失時使用）
     */
    public boolean forceReinitializeOpenGL() {
        Log.d(TAG, "🔄 Force reinitializing OpenGL...");
        
        try {
            // 先清理現有資源
            cleanupRenderingNative();
            isRenderingActive = false;
            
            // 等待一會兒
            Thread.sleep(100);
            
            // 重新初始化
            return initializeOpenGLResources();
            
        } catch (Exception e) {
            Log.e(TAG, "Error force reinitializing OpenGL", e);
            return false;
        }
    }

    /**
     * 獲取 OpenGL 狀態詳細信息（用於調試）
     */
    public String getOpenGLStatusDetails() {
        try {
            StringBuilder status = new StringBuilder();
            status.append("=== OpenGL Status ===\n");
            status.append("Resources initialized: ").append(validateRenderingSetupSafely()).append("\n");
            status.append("Rendering active: ").append(isRenderingActiveNative()).append("\n");
            status.append("Camera active: ").append(isCameraActiveNative()).append("\n");
            status.append("Vuforia running: ").append(isVuforiaEngineRunningNative()).append("\n");
            status.append("Engine status: ").append(getEngineStatusDetailNative()).append("\n");
            return status.toString();
        } catch (Exception e) {
            return "Error getting OpenGL status: " + e.getMessage();
        }
    }

    // ==================== 🔧 修改：生命週期方法 ====================
    
    // 注意：pauseVuforia() 和 resumeVuforia() 方法已經存在於第307-340行
    // 這裡不再重複定義，而是在現有方法中添加渲染循環控制
}
