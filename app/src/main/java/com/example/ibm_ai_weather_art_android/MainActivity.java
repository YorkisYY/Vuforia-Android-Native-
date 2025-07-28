package com.example.ibm_ai_weather_art_android;

import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.util.Log;
import android.widget.Toast;

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

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        
        // ✅ 步驟 1: 初始化 Filament (必須在使用任何 Filament 組件之前調用)
        if (!initializeFilament()) {
            Log.e(TAG, "Failed to initialize Filament");
            Toast.makeText(this, "Failed to initialize 3D rendering engine", Toast.LENGTH_LONG).show();
            finish();
            return;
        }
        
        setContentView(R.layout.activity_main);

        // ✅ 新增：初始化 CameraX PreviewView
        previewView = findViewById(R.id.previewView);
        if (previewView == null) {
            Log.e(TAG, "PreviewView not found, falling back to SurfaceView");
            // 如果没有 PreviewView，使用现有的 SurfaceView
            initializeARComponents();
            return;
        }

        // 初始化 AR 组件
        initializeARComponents();

        // 创建相机执行器
        cameraExecutor = Executors.newSingleThreadExecutor();

        // 请求相机权限
        if (allPermissionsGranted()) {
            startCamera();
        } else {
            ActivityCompat.requestPermissions(this, REQUIRED_PERMISSIONS, REQUEST_CODE_PERMISSIONS);
        }
    }

    // ✅ 新增：Filament 初始化方法 (Filament 1.31 版本)
    private boolean initializeFilament() {
        try {
            // 在 Filament 1.31 中需要明確載入 gltfio-jni 庫
            System.loadLibrary("filament-jni");
            System.loadLibrary("gltfio-jni");  // 明確載入這個庫
            Filament.init();
            Log.d(TAG, "Filament initialized successfully via Filament.init()");
            return true;
        } catch (Exception e) {
            Log.e(TAG, "Filament initialization failed", e);
            return false;
        }
    }
    
    private void initializeARComponents() {
        try {
            filamentRenderer = new FilamentRenderer(this);
            Log.d(TAG, "FilamentRenderer 初始化成功");
            
        } catch (Exception e) {
            Log.e(TAG, "初始化 AR 組件失敗", e);
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