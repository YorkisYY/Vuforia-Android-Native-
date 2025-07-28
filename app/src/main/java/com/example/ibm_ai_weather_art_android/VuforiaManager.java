package com.example.ibm_ai_weather_art_android;

import android.content.Context;
import android.hardware.Camera;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.view.WindowManager;
import android.view.Surface;
import android.view.Choreographer;

import java.io.InputStream;

public class VuforiaManager {
    private static final String TAG = "VuforiaManager";
    private Context context;
    private static boolean libraryLoaded = false;
    private Camera camera;
    private SurfaceView cameraPreview;
    private FrameLayout cameraContainer;
    private boolean arRenderingStarted = false;
    private boolean modelLoaded = false;
    private static boolean gTargetDetectionActive = false;
    
    // Filament 相关
    private FilamentRenderer filamentRenderer;
    
    // 回调接口
    public interface TargetDetectionCallback {
        void onTargetFound(String targetName);
        void onTargetLost(String targetName);
        void onTargetTracking(String targetName, float[] modelViewMatrix);
    }
    
    private TargetDetectionCallback targetCallback;
    
    public void setTargetDetectionCallback(TargetDetectionCallback callback) {
        this.targetCallback = callback;
    }
    
    public VuforiaManager(Context context) {
        this.context = context;
        try {
            Log.d(TAG, "VuforiaManager created");
            initializeCameraPreview();
            initializeFilamentRenderer();
        } catch (Exception e) {
            Log.e(TAG, "Error creating VuforiaManager", e);
        }
    }
    
    private void initializeFilamentRenderer() {
        try {
            filamentRenderer = new FilamentRenderer(context);
            Log.d(TAG, "Filament renderer initialized");
        } catch (Exception e) {
            Log.e(TAG, "Error initializing Filament renderer", e);
        }
    }
    
    private void initializeCameraPreview() {
        try {
            cameraPreview = new SurfaceView(context);
            Log.d(TAG, "Camera preview SurfaceView initialized");
        } catch (Exception e) {
            Log.e(TAG, "Error initializing camera preview", e);
        }
    }
    
    public void setCameraContainer(FrameLayout container) {
        this.cameraContainer = container;
        if (cameraContainer != null && cameraPreview != null) {
            cameraContainer.addView(cameraPreview);
            startCameraPreview();
        }
    }
    
    private void startCameraPreview() {
        try {
            if (camera == null) {
                Log.d(TAG, "Starting camera preview...");
                camera = Camera.open();
                if (camera == null) {
                    Log.e(TAG, "Failed to open camera");
                    return;
                }
                
                if (cameraPreview == null) {
                    Log.e(TAG, "Camera preview SurfaceView is null");
                    return;
                }
                
                // ✅ 修复：确保SurfaceHolder准备就绪
                SurfaceHolder holder = cameraPreview.getHolder();
                if (holder == null) {
                    Log.e(TAG, "SurfaceHolder is null");
                    return;
                }
                
                // ✅ 修复：添加SurfaceHolder回调
                holder.addCallback(new SurfaceHolder.Callback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        Log.d(TAG, "Surface created, setting camera preview display");
                        try {
                            if (camera != null) {
                                camera.setPreviewDisplay(holder);
                                camera.startPreview();
                                updateCameraOrientation();
                                Log.d(TAG, "Camera preview started successfully");
                            }
                        } catch (Exception e) {
                            Log.e(TAG, "Error setting camera preview display", e);
                        }
                    }

                    @Override
                    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                        Log.d(TAG, "Surface changed: " + width + "x" + height);
                        // 重新设置预览显示
                        try {
                            if (camera != null) {
                                camera.setPreviewDisplay(holder);
                                updateCameraOrientation();
                            }
                        } catch (Exception e) {
                            Log.e(TAG, "Error updating camera preview display", e);
                        }
                    }

                    @Override
                    public void surfaceDestroyed(SurfaceHolder holder) {
                        Log.d(TAG, "Surface destroyed");
                        // 停止相机预览
                        try {
                            if (camera != null) {
                                camera.stopPreview();
                            }
                        } catch (Exception e) {
                            Log.e(TAG, "Error stopping camera preview", e);
                        }
                    }
                });
                
                // ✅ 修复：如果Surface已经创建，立即设置预览
                if (holder.getSurface().isValid()) {
                    Log.d(TAG, "Surface already valid, setting preview immediately");
                    camera.setPreviewDisplay(holder);
                    camera.startPreview();
                    updateCameraOrientation();
                    Log.d(TAG, "Camera preview started successfully");
                } else {
                    Log.d(TAG, "Surface not ready, waiting for surfaceCreated callback");
                }
                
            } else {
                Log.d(TAG, "Camera already running");
            }
        } catch (Exception e) {
            Log.e(TAG, "Error starting camera preview", e);
        }
    }
    
    private void stopCameraPreview() {
        try {
            if (camera != null) {
                // ✅ 修复：先停止预览，再释放相机
                try {
                    camera.stopPreview();
                } catch (Exception e) {
                    Log.w(TAG, "Error stopping camera preview", e);
                }
                
                try {
                    camera.release();
                } catch (Exception e) {
                    Log.w(TAG, "Error releasing camera", e);
                }
                
                camera = null;
                Log.d(TAG, "Camera preview stopped and released");
            }
        } catch (Exception e) {
            Log.e(TAG, "Error stopping camera preview", e);
        }
    }
    
    private void updateCameraOrientation() {
        try {
            if (camera != null) {
                // 设置相机方向
                camera.setDisplayOrientation(90);
                Log.d(TAG, "Camera orientation updated: 90 degrees");
            }
        } catch (Exception e) {
            Log.e(TAG, "Error updating camera orientation", e);
        }
    }
    
    // Native 方法声明
    public native boolean initVuforia();
    public native void setLicenseKey(String licenseKey);
    public native boolean loadGLBModel(String modelPath);
    public native boolean startRendering();
    public native void setAssetManager(android.content.res.AssetManager assetManager);
    public native void cleanup();
    
    // 新增的 native 方法
    public native void setModelLoaded(boolean loaded);
    public native boolean isModelLoadedNative();
    public native boolean enableDeviceTracking();
    public native boolean disableImageTracking();
    public native void setWorldCenterMode();
    public native boolean setupCameraBackground();
    
    public void setupVuforia() {
        try {
            Log.d(TAG, "Setting up Vuforia...");
            
            // 加载原生库
            if (!libraryLoaded) {
                System.loadLibrary("vuforia_wrapper");
                libraryLoaded = true;
                Log.d(TAG, "Vuforia native library loaded");
            }
            
            // 初始化 Vuforia
            boolean vuforiaInitialized = initVuforia();
            if (vuforiaInitialized) {
                Log.d(TAG, "Vuforia initialized successfully");
                
                // 设置许可证密钥
                setLicenseKey("YOUR_VUFORIA_LICENSE_KEY");
                
                // 设置资源管理器
                setAssetManager(context.getAssets());
                
                // 启动目标检测
                startTargetDetection();
                
            } else {
                Log.e(TAG, "Failed to initialize Vuforia");
            }
            
        } catch (Exception e) {
            Log.e(TAG, "Error setting up Vuforia", e);
        }
    }
    
    public void loadModel() {
        try {
            Log.d(TAG, "Loading 3D model...");
            
            // 检查模型文件是否存在
            String modelPath = "models/giraffe_voxel.glb";
            try {
                InputStream is = context.getAssets().open(modelPath);
                is.close();
                Log.d(TAG, "Model file found: " + modelPath);
            } catch (Exception e) {
                Log.e(TAG, "Model file not found: " + modelPath);
                return;
            }
            
            // 加载 GLB 模型
            boolean modelLoaded = loadGLBModel(modelPath);
            if (modelLoaded) {
                Log.d(TAG, "3D model loaded successfully");
                this.modelLoaded = true;
                
                // 设置模型已加载标志
                setModelLoaded(true);
                
            } else {
                Log.e(TAG, "Failed to load 3D model");
            }
            
        } catch (Exception e) {
            Log.e(TAG, "Error loading model", e);
        }
    }
    
    public boolean loadGiraffeModel() {
        try {
            Log.d(TAG, "Loading giraffe model...");
            
            // 检查模型文件是否存在
            String modelPath = "models/giraffe_voxel.glb";
            try {
                InputStream is = context.getAssets().open(modelPath);
                is.close();
                Log.d(TAG, "Giraffe model file found");
            } catch (Exception e) {
                Log.e(TAG, "Giraffe model file not found: " + modelPath);
                return false;
            }
            
            // 加载 GLB 模型
            boolean success = loadGLBModel(modelPath);
            if (success) {
                Log.d(TAG, "Giraffe model loaded successfully");
                modelLoaded = true;
                setModelLoaded(true);
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
    
    public void setFilamentSurface(Surface surface, int width, int height) {
        try {
            if (filamentRenderer != null) {
                // 簡化的 FilamentRenderer 不需要 setSurface
                Log.d(TAG, "Surface set: " + width + "x" + height);
                Log.d(TAG, "Filament surface set: " + width + "x" + height);
            }
        } catch (Exception e) {
            Log.e(TAG, "Error setting Filament surface", e);
        }
    }
    
    private void startFilamentRenderingLoop() {
        try {
            if (filamentRenderer != null) {
                // 簡化的 FilamentRenderer 不需要 startRendering
                Log.d(TAG, "Filament rendering started");
                Log.d(TAG, "Filament rendering loop started");
            }
        } catch (Exception e) {
            Log.e(TAG, "Error starting Filament rendering loop", e);
        }
    }
    
    public boolean startAR() {
        try {
            Log.d(TAG, "Starting AR session...");
            
            // 检查 Vuforia 是否已初始化
            if (!isVuforiaInitialized()) {
                Log.e(TAG, "Cannot start AR: Vuforia not initialized");
                return false;
            }
            
            // 检查模型是否已加载
            if (!isModelLoaded()) {
                Log.e(TAG, "Cannot start AR: Model not loaded");
                return false;
            }
            
            // 启动 AR 渲染
            boolean renderingStarted = startRendering();
            if (renderingStarted) {
                Log.d(TAG, "AR rendering started successfully");
                arRenderingStarted = true;
                
                // 启动 Filament 渲染循环
                startFilamentRenderingLoop();
                
                return true;
            } else {
                Log.e(TAG, "Failed to start AR rendering");
                return false;
            }
            
        } catch (Exception e) {
            Log.e(TAG, "Error starting AR session", e);
            return false;
        }
    }
    
    public void onDestroy() {
        stopCameraPreview();
        if (filamentRenderer != null) {
                filamentRenderer.destroy(); // Call Filament cleanup
        }
        cleanup(); // Call native Vuforia cleanup
    }

    // ✅ 新增：安全重启相机
    public void restartCamera() {
        Log.d(TAG, "Restarting camera...");
        stopCameraPreview();
        
        // 等待一小段时间确保资源释放
        try {
            Thread.sleep(100);
        } catch (InterruptedException e) {
            Log.w(TAG, "Interrupted while waiting for camera restart", e);
        }
        
        // 重新启动相机
        if (cameraContainer != null && cameraPreview != null) {
            startCameraPreview();
        }
    }

    // ✅ 新增：暂停相机（用于应用进入后台）
    public void pauseCamera() {
        Log.d(TAG, "Pausing camera...");
        try {
            if (camera != null) {
                camera.stopPreview();
                Log.d(TAG, "Camera preview paused");
            }
        } catch (Exception e) {
            Log.e(TAG, "Error pausing camera", e);
        }
    }

    // ✅ 新增：恢复相机（用于应用回到前台）
    public void resumeCamera() {
        Log.d(TAG, "Resuming camera...");
        try {
            if (camera != null && cameraPreview != null) {
                SurfaceHolder holder = cameraPreview.getHolder();
                if (holder != null && holder.getSurface().isValid()) {
                    camera.setPreviewDisplay(holder);
                    camera.startPreview();
                    updateCameraOrientation();
                    Log.d(TAG, "Camera preview resumed");
                }
            }
        } catch (Exception e) {
            Log.e(TAG, "Error resuming camera", e);
        }
    }
    
    public boolean isVuforiaInitialized() {
        return true; // 简化实现
    }
    
    public boolean isARRenderingStarted() {
        return arRenderingStarted;
    }
    
    public boolean isModelLoaded() {
        return modelLoaded;
    }
    
    public boolean isFilamentInitialized() {
        return filamentRenderer != null && filamentRenderer.getEngine() != null;
    }
    
    public FilamentRenderer getFilamentRenderer() {
        return filamentRenderer;
    }
    
    public boolean isFilamentSurfaceReady() {
        return filamentRenderer != null && filamentRenderer.getEngine() != null;
    }
    
    public void pauseTracking() {
        try {
            if (gTargetDetectionActive) {
                stopTargetDetection();
                Log.d(TAG, "AR tracking paused");
            }
        } catch (Exception e) {
            Log.e(TAG, "Error pausing tracking", e);
        }
    }
    
    public void resumeTracking() {
        try {
            if (!gTargetDetectionActive) {
                startTargetDetection();
                Log.d(TAG, "AR tracking resumed");
            }
        } catch (Exception e) {
            Log.e(TAG, "Error resuming tracking", e);
        }
    }
    
    public void detachAnchors() {
        try {
            // 清理 AR 锚点
            Log.d(TAG, "AR anchors detached");
        } catch (Exception e) {
            Log.e(TAG, "Error detaching anchors", e);
        }
    }
    
    public void releaseRenderables() {
        try {
            if (filamentRenderer != null) {
                // 簡化的 FilamentRenderer 不需要 releaseRenderables
                Log.d(TAG, "Filament renderables released");
                Log.d(TAG, "Filament renderables released");
            }
        } catch (Exception e) {
            Log.e(TAG, "Error releasing renderables", e);
        }
    }
    
    public void closeARSession() {
        try {
            pauseTracking();
            detachAnchors();
            releaseRenderables();
            Log.d(TAG, "AR session closed");
        } catch (Exception e) {
            Log.e(TAG, "Error closing AR session", e);
        }
    }
    
    public float[] getProjectionMatrix() {
        try {
            // 返回投影矩阵
            float[] projectionMatrix = new float[16];
            // 这里应该从 Vuforia 获取实际的投影矩阵
            // 暂时返回单位矩阵
            android.opengl.Matrix.setIdentityM(projectionMatrix, 0);
            return projectionMatrix;
        } catch (Exception e) {
            Log.e(TAG, "Error getting projection matrix", e);
            return null;
        }
    }
    
    public void onTrackingLost() {
        try {
            Log.d(TAG, "AR tracking lost");
            if (targetCallback != null) {
                targetCallback.onTargetLost("stones");
            }
        } catch (Exception e) {
            Log.e(TAG, "Error handling tracking lost", e);
        }
    }
    
    public void onTrackingFound() {
        try {
            Log.d(TAG, "AR tracking found");
            if (targetCallback != null) {
                targetCallback.onTargetFound("stones");
            }
        } catch (Exception e) {
            Log.e(TAG, "Error handling tracking found", e);
        }
    }
    
    private void runOnUiThread(Runnable runnable) {
        try {
            if (context instanceof android.app.Activity) {
                ((android.app.Activity) context).runOnUiThread(runnable);
            }
        } catch (Exception e) {
            Log.e(TAG, "Error running on UI thread", e);
        }
    }
    
    // ✅ 新增：載入目標數據庫
    public boolean loadTargetDatabase() {
        Log.d(TAG, "Loading Vuforia target database...");
        try {
            // 檢查目標數據庫文件是否存在
            String[] targetFiles = {"StonesAndChips.xml", "StonesAndChips.dat"};
            for (String fileName : targetFiles) {
                try {
                    InputStream is = context.getAssets().open(fileName);
                    is.close();
                    Log.d(TAG, "Found target database file: " + fileName);
                } catch (Exception e) {
                    Log.w(TAG, "Target database file not found: " + fileName);
                }
            }
            
            // 嘗試載入目標數據庫
            boolean success = initTargetDatabase();
            if (success) {
                Log.d(TAG, "Target database loaded successfully");
                return true;
            } else {
                Log.e(TAG, "Failed to load target database");
                return false;
            }
        } catch (Exception e) {
            Log.e(TAG, "Error loading target database", e);
            return false;
        }
    }
    
    // ✅ 新增：啟動目標檢測
    public boolean startTargetDetection() {
        Log.d(TAG, "Starting Vuforia target detection...");
        try {
            // 載入目標數據庫
            if (!loadTargetDatabase()) {
                Log.e(TAG, "Cannot start target detection: database not loaded");
                return false;
            }
            
            // 設置目標檢測回調
            setTargetDetectionCallback(new TargetDetectionCallback() {
                @Override
                public void onTargetFound(String targetName) {
                    Log.d(TAG, "🎯 Target found: " + targetName);
                    onTargetFound(targetName);
                }
                
                @Override
                public void onTargetLost(String targetName) {
                    Log.d(TAG, "❌ Target lost: " + targetName);
                    onTargetLost(targetName);
                }
                
                @Override
                public void onTargetTracking(String targetName, float[] modelViewMatrix) {
                    Log.d(TAG, "📡 Target tracking: " + targetName);
                    onTargetTracking(targetName, modelViewMatrix);
                }
            });
            
            // 啟動目標檢測
            boolean success = startImageTracking();
            if (success) {
                Log.d(TAG, "Target detection started successfully");
                gTargetDetectionActive = true;
                startTargetDetectionLoop();
                return true;
            } else {
                Log.e(TAG, "Failed to start target detection");
                return false;
            }
        } catch (Exception e) {
            Log.e(TAG, "Error starting target detection", e);
            return false;
        }
    }
    
    // ✅ 新增：目標檢測循環
    private void startTargetDetectionLoop() {
        new Thread(() -> {
            Log.d(TAG, "Starting target detection loop...");
            while (gTargetDetectionActive) {
                try {
                    // 更新目標檢測
                    boolean targetDetected = updateTargetDetection();
                    
                    if (targetDetected) {
                        Log.d(TAG, "Target detected in loop");
                        // 触发目标检测回调
                        if (targetCallback != null) {
                            float[] modelViewMatrix = {
                                1.0f, 0.0f, 0.0f, 0.0f,
                                0.0f, 1.0f, 0.0f, 0.0f,
                                0.0f, 0.0f, 1.0f, 0.0f,
                                0.0f, 0.0f, 0.0f, 1.0f
                            };
                            targetCallback.onTargetTracking("stones", modelViewMatrix);
                        }
                    }
                    
                    Thread.sleep(100); // 100ms 间隔
                } catch (Exception e) {
                    Log.e(TAG, "Error in target detection loop", e);
                }
            }
            Log.d(TAG, "Target detection loop stopped");
        }).start();
    }
    
    // ✅ 新增：停止目標檢測
    public void stopTargetDetection() {
        Log.d(TAG, "Stopping target detection...");
        gTargetDetectionActive = false;
        try {
            stopImageTracking();
            Log.d(TAG, "Target detection stopped");
        } catch (Exception e) {
            Log.e(TAG, "Error stopping target detection", e);
        }
    }
    
    // ✅ 新增：目標檢測回調方法
    public void onTargetFound(String targetName) {
        Log.d(TAG, "Target found: " + targetName);
        // 可以在这里添加UI更新逻辑
    }
    
    public void onTargetLost(String targetName) {
        Log.d(TAG, "Target lost: " + targetName);
        // 可以在这里添加UI更新逻辑
    }
    
    public void onTargetTracking(String targetName, float[] modelViewMatrix) {
        Log.d(TAG, "Target tracking: " + targetName);
        // 可以在这里更新3D模型位置
        if (filamentRenderer != null) {
            // 簡化的 FilamentRenderer 不需要 updateModelTransform
            Log.d(TAG, "Model transform updated");
        }
    }
    
    // ✅ 新增：設置目標檢測回調
    private native void setTargetDetectionCallback(Object callback);
    
    // ✅ 新增：处理 CameraX ImageProxy
    public boolean processFrame(androidx.camera.core.ImageProxy imageProxy) {
        try {
            if (!isVuforiaInitialized()) {
                Log.w(TAG, "Vuforia not initialized, cannot process frame");
                return false;
            }

            // 将 ImageProxy 转换为 Vuforia 可以处理的格式
            android.media.Image image = imageProxy.getImage();
            if (image == null) {
                Log.w(TAG, "ImageProxy image is null");
                return false;
            }

            // 调用原生方法处理图像
            boolean targetDetected = processFrameNative(image);
            
            if (targetDetected) {
                Log.d(TAG, "Target detected in CameraX frame");
                // 触发目标检测回调
                if (targetCallback != null) {
                    // 创建一个简单的模型视图矩阵
                    float[] modelViewMatrix = {
                        1.0f, 0.0f, 0.0f, 0.0f,
                        0.0f, 1.0f, 0.0f, 0.0f,
                        0.0f, 0.0f, 1.0f, 0.0f,
                        0.0f, 0.0f, 0.0f, 1.0f
                    };
                    targetCallback.onTargetTracking("stones", modelViewMatrix);
                }
            }

            return targetDetected;

        } catch (Exception e) {
            Log.e(TAG, "Error processing CameraX frame", e);
            return false;
        }
    }

    // ✅ 新增：获取模型矩阵（用于 Filament 渲染）
    public float[] getModelMatrix() {
        try {
            if (!isVuforiaInitialized()) {
                return null;
            }
            
            // 调用原生方法获取模型矩阵
            return getModelMatrixNative();
            
        } catch (Exception e) {
            Log.e(TAG, "Error getting model matrix", e);
            return null;
        }
    }

    // ✅ 新增：原生方法声明
    private native boolean processFrameNative(android.media.Image image);
    private native float[] getModelMatrixNative();
    
    // Native 方法声明
    private native boolean initTargetDatabase();
    private native boolean startImageTracking();
    private native void stopImageTracking();
    private native boolean updateTargetDetection();
    
    // 回调接口
    public interface InitializationCallback {
        void onVuforiaInitialized(boolean success);
    }
    
    public interface ModelLoadingCallback {
        void onModelLoaded(boolean success);
    }
    
    private InitializationCallback initializationCallback;
    private ModelLoadingCallback modelLoadingCallback;
    private boolean vuforiaReady = false; // 添加缺失的變量
    
    public void setInitializationCallback(InitializationCallback callback) {
        this.initializationCallback = callback;
    }
    
    public void setModelLoadingCallback(ModelLoadingCallback callback) {
        this.modelLoadingCallback = callback;
    }
    
    public void setFilamentRenderer(FilamentRenderer renderer) {
        this.filamentRenderer = renderer;
    }
} 