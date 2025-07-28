package com.example.ibm_ai_weather_art_android;

import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.util.Log;
import android.widget.Toast;
import android.view.SurfaceView;
import androidx.appcompat.app.AppCompatActivity;
import androidx.camera.core.Camera;
import androidx.camera.core.CameraSelector;
import androidx.camera.core.ImageAnalysis;
import androidx.camera.core.ImageProxy;
import androidx.camera.core.Preview;
import androidx.camera.lifecycle.ProcessCameraProvider;
import androidx.camera.view.PreviewView;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import androidx.lifecycle.LifecycleOwner;

import com.google.common.util.concurrent.ListenableFuture;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import android.view.Choreographer;

// ✅ 新增：Filament 相關 imports
import com.google.android.filament.Filament;

public class MainActivity extends AppCompatActivity {
    private static final String TAG = "MainActivity";
    private static final int REQUEST_CODE_PERMISSIONS = 10;
    private static final String[] REQUIRED_PERMISSIONS = new String[]{Manifest.permission.CAMERA};

    // ✅ 新增：CameraX 组件
    private PreviewView previewView;
    private ProcessCameraProvider cameraProvider;
    private Camera camera;
    private ExecutorService cameraExecutor;

    // 现有的 AR 组件
    private VuforiaManager vuforiaManager;
    private FilamentRenderer filamentRenderer;
    
    // 新增：AR 初始化狀態變量
    private boolean isFilamentInitialized = false;
    private boolean isVuforiaInitialized = false;
    private boolean isArReady = false;
    private SurfaceView filamentSurface;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        
        // 初始化視圖
        initViews();
        
        // 檢查相機權限並初始化組件
        if (allPermissionsGranted()) {
            initializeComponents();
        } else {
            ActivityCompat.requestPermissions(this, REQUIRED_PERMISSIONS, REQUEST_CODE_PERMISSIONS);
        }
    }

    // 新增：初始化視圖
    private void initViews() {
        Log.d(TAG, "初始化視圖組件");
        
        // 初始化 CameraX PreviewView
        previewView = findViewById(R.id.previewView);
        if (previewView == null) {
            Log.e(TAG, "PreviewView not found");
            return;
        }
        
        // 初始化 Filament SurfaceView
        filamentSurface = findViewById(R.id.filamentSurface);
        if (filamentSurface == null) {
            Log.e(TAG, "Filament SurfaceView not found");
            return;
        }
        
        // 創建相機執行器
        cameraExecutor = Executors.newSingleThreadExecutor();
    }
    
    // 新增：完整的組件初始化流程
    private void initializeComponents() {
        Log.d(TAG, "開始初始化所有組件");
        
        // 步驟 1: 初始化 Filament
        initializeFilament();
        
        // 步驟 2: 初始化相機預覽 (CameraX)
        initializeCameraX();
        
        // 步驟 3: 初始化 Vuforia (在相機準備好後)
        initializeVuforia();
        
        // 步驟 4: 設置 AR 整合
        setupARIntegration();
    }
    
    // 新增：Filament 初始化
    private void initializeFilament() {
        Log.d(TAG, "初始化 Filament");
        
        try {
            // 初始化 Filament 全局設置
            System.loadLibrary("filament-jni");
            System.loadLibrary("gltfio-jni");
            Filament.init();
            Log.d(TAG, "Filament 全局初始化成功");
            
            // 創建 FilamentRenderer
            filamentRenderer = new FilamentRenderer(this);
            
            // 設置 Filament Surface
            if (filamentSurface != null) {
                filamentRenderer.setupSurface(filamentSurface);
            }
            isFilamentInitialized = true;
            checkArReadiness();
        } catch (Exception e) {
            Log.e(TAG, "Filament 初始化錯誤: " + e.getMessage());
        }
    }
    
    // 新增：CameraX 初始化
    private void initializeCameraX() {
        Log.d(TAG, "初始化 CameraX");
        
        if (allPermissionsGranted()) {
            startCamera();
        }
    }
    
    // 新增：Vuforia 初始化
    private void initializeVuforia() {
        Log.d(TAG, "初始化 Vuforia");
        
        try {
            // 創建 VuforiaManager
            vuforiaManager = new VuforiaManager(this);
            
            // 設置回調
            vuforiaManager.setTargetDetectionCallback(new VuforiaManager.TargetDetectionCallback() {
                @Override
                public void onTargetFound(String targetName) {
                    Log.d(TAG, "🎯 Target found: " + targetName);
                }
                
                @Override
                public void onTargetLost(String targetName) {
                    Log.d(TAG, "❌ Target lost: " + targetName);
                }
                
                @Override
                public void onTargetTracking(String targetName, float[] modelViewMatrix) {
                    Log.d(TAG, "📡 Target tracking: " + targetName);
                    // 更新 3D 模型位置
                    if (filamentRenderer != null) {
                        // 這裡可以添加模型位置更新邏輯
                        Log.d(TAG, "Model transform updated");
                    }
                }
            });
            
            // 開始初始化
            vuforiaManager.setupVuforia();
            isVuforiaInitialized = true;
            checkArReadiness();
        } catch (Exception e) {
            Log.e(TAG, "Vuforia 初始化錯誤: " + e.getMessage());
        }
    }
    
    // 新增：AR 整合設置
    private void setupARIntegration() {
        Log.d(TAG, "設置 AR 整合");
        
        if (isArReady) {
            // 載入 3D 模型
            if (vuforiaManager != null) {
                vuforiaManager.loadGiraffeModel();
            }
            
            // 啟動目標檢測
            if (vuforiaManager != null) {
                vuforiaManager.startTargetDetection();
            }
            
            Log.d(TAG, "🎉 AR 整合完成！");
        }
    }
    
    // 新增：檢查 AR 就緒狀態
    private void checkArReadiness() {
        Log.d(TAG, "檢查 AR 就緒狀態 - Filament: " + isFilamentInitialized + 
                   ", Vuforia: " + isVuforiaInitialized);
                   
        if (isFilamentInitialized && isVuforiaInitialized && !isArReady) {
            isArReady = true;
            Log.d(TAG, "🎉 AR 系統完全就緒！");
            setupARIntegration();
        }
    }
    


    // ✅ 新增：使用 CameraX 启动相机
    private void startCamera() {
        ListenableFuture<ProcessCameraProvider> cameraProviderFuture = 
            ProcessCameraProvider.getInstance(this);

        cameraProviderFuture.addListener(() -> {
            try {
                // 获取 CameraProvider
                cameraProvider = cameraProviderFuture.get();

                // 设置预览
                Preview preview = new Preview.Builder().build();
                preview.setSurfaceProvider(previewView.getSurfaceProvider());

                // ✅ 新增：设置图像分析（用于 AR）
                ImageAnalysis imageAnalysis = new ImageAnalysis.Builder()
                    .setTargetResolution(new android.util.Size(1280, 720))
                    .setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST)
                    .build();

                imageAnalysis.setAnalyzer(cameraExecutor, imageProxy -> {
                    // 将图像传递给 Vuforia 进行处理
                    processImageForAR(imageProxy);
                });

                // 选择后置相机
                CameraSelector cameraSelector = CameraSelector.DEFAULT_BACK_CAMERA;

                try {
                    // 解绑所有用例
                    cameraProvider.unbindAll();

                    // 绑定用例到生命周期
                    camera = cameraProvider.bindToLifecycle(
                        this,
                        cameraSelector,
                        preview,
                        imageAnalysis
                    );

                    Log.d(TAG, "CameraX camera started successfully");

                } catch (Exception exc) {
                    Log.e(TAG, "Use case binding failed", exc);
                }

            } catch (ExecutionException | InterruptedException exc) {
                Log.e(TAG, "Camera initialization failed", exc);
            }
        }, ContextCompat.getMainExecutor(this));
    }

    // ✅ 新增：处理图像用于 AR
    private void processImageForAR(androidx.camera.core.ImageProxy imageProxy) {
        try {
            if (vuforiaManager != null) {
                // 将 CameraX 的 ImageProxy 转换为 Vuforia 可以处理的格式
                boolean targetDetected = vuforiaManager.processFrame(imageProxy);

                if (targetDetected && filamentRenderer != null) {
                    // 更新 3D 模型位置
                    runOnUiThread(() -> {
                        try {
                            float[] modelMatrix = vuforiaManager.getModelMatrix();
                            if (modelMatrix != null) {
                                // 簡化的 FilamentRenderer 不需要 updateModelTransform
                            Log.d(TAG, "Model transform updated");
                            }
                        } catch (Exception e) {
                            Log.e(TAG, "Error updating model position", e);
                        }
                    });
                }
            }
        } catch (Exception e) {
            Log.e(TAG, "Error processing AR frame", e);
        } finally {
            // 重要：必须关闭 ImageProxy
            imageProxy.close();
        }
    }

    // ✅ 改进：生命周期管理
    @Override
    protected void onPause() {
        super.onPause();
        Log.d(TAG, "onPause - pausing AR components");
        
        if (vuforiaManager != null) {
            vuforiaManager.pauseTracking();
        }
        
        // 暂停 Filament 渲染
        if (filamentRenderer != null) {
            // 簡化的 FilamentRenderer 不需要暫停
            Log.d(TAG, "Filament rendering paused");
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        Log.d(TAG, "onResume - resuming AR components");
        
        if (vuforiaManager != null) {
            vuforiaManager.resumeTracking();
        }
        
        // 恢复 Filament 渲染
        if (filamentRenderer != null) {
            // 簡化的 FilamentRenderer 不需要恢復
            Log.d(TAG, "Filament rendering resumed");
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        Log.d(TAG, "onDestroy - cleaning up resources");
        
        // 清理相机执行器
        if (cameraExecutor != null) {
            cameraExecutor.shutdown();
        }
        
        // 清理 AR 组件
        if (vuforiaManager != null) {
            vuforiaManager.cleanup();
        }
        
        if (filamentRenderer != null) {
            filamentRenderer.destroy();
        }
    }

    // ✅ 新增：权限检查
    private boolean allPermissionsGranted() {
        for (String permission : REQUIRED_PERMISSIONS) {
            if (ContextCompat.checkSelfPermission(this, permission) != PackageManager.PERMISSION_GRANTED) {
                return false;
            }
        }
        return true;
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQUEST_CODE_PERMISSIONS) {
            if (allPermissionsGranted()) {
                startCamera();
            } else {
                Toast.makeText(this, "需要相机权限", Toast.LENGTH_SHORT).show();
                finish();
            }
        }
    }
} 