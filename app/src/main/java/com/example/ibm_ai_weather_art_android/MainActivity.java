package com.example.ibm_ai_weather_art_android;

import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.util.Log;
import android.widget.Toast;
import android.view.SurfaceView;
import android.widget.FrameLayout;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import com.google.android.filament.Filament; 
import com.example.ibm_ai_weather_art_android.model.GLBReader;
import android.app.AlertDialog;
import android.content.Intent;
import android.net.Uri;
import android.provider.Settings;

public class MainActivity extends AppCompatActivity {
    private static final String TAG = "MainActivity";
    private static final int REQUEST_CODE_PERMISSIONS = 10;
    private static final String[] REQUIRED_PERMISSIONS = new String[]{Manifest.permission.CAMERA};
    
    // 狀態管理
    private volatile boolean arInitializationRequested = false;
    private volatile boolean filamentInitialized = false;
    private volatile boolean vuforiaInitialized = false;
    
    // 核心組件
    private VuforiaCoreManager vuforiaCoreManager;
    private FilamentRenderer filamentRenderer;
    private GLBReader glbReader;
    
    // UI 組件
    private FrameLayout cameraContainer;
    private SurfaceView filamentSurface;
    
    static {
        // 初始化 Filament
        Filament.init();
        
        // 載入 Filament 相關庫
        try {
            System.loadLibrary("gltfio-jni");
            Log.d("MainActivity", "✅ gltfio-jni library loaded successfully");
        } catch (UnsatisfiedLinkError e) {
            Log.e("MainActivity", "❌ Failed to load gltfio-jni library", e);
        }
        
        // 載入 Vuforia 庫
        try {
            System.loadLibrary("Vuforia");
            Log.d("MainActivity", "✅ libVuforia.so loaded successfully");
        } catch (UnsatisfiedLinkError e) {
            Log.e("MainActivity", "❌ Failed to load libVuforia.so: " + e.getMessage());
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        
        Log.d(TAG, "🚀 MainActivity onCreate started");
        
        // 初始化核心組件
        initializeCoreComponents();
        
        // 初始化視圖
        initViews();
        
        // 設置回調
        setupCallbacks();
        
        // 請求權限
        requestCameraPermissions();
        
        Log.d(TAG, "✅ MainActivity onCreate completed");
    }

    private void initializeCoreComponents() {
        Log.d(TAG, "Initializing core components...");
        
        try {
            // 創建 VuforiaCoreManager
            vuforiaCoreManager = new VuforiaCoreManager(this);
            Log.d(TAG, "✅ VuforiaCoreManager created");
            
            // 創建 FilamentRenderer
            filamentRenderer = new FilamentRenderer(this);
            filamentInitialized = true;
            Log.d(TAG, "✅ FilamentRenderer created");
            
            // 創建 GLBReader
            glbReader = new GLBReader(this);
            Log.d(TAG, "✅ GLBReader created");
            
        } catch (Exception e) {
            Log.e(TAG, "❌ Error initializing core components", e);
            showError("核心組件初始化失敗");
        }
    }
    
    private void initViews() {
        Log.d(TAG, "Initializing UI views...");
        
        try {
            cameraContainer = findViewById(R.id.cameraContainer);
            if (cameraContainer == null) {
                Log.e(TAG, "❌ Camera container not found in layout");
                showError("UI 初始化失敗：找不到相機容器");
                return;
            }
            
            filamentSurface = findViewById(R.id.filamentSurface);
            if (filamentSurface == null) {
                Log.e(TAG, "❌ Filament SurfaceView not found in layout");
                showError("UI 初始化失敗：找不到 Filament Surface");
                return;
            }
            
            // 確保 Surface 可見
            filamentSurface.setVisibility(android.view.View.VISIBLE);
            
            Log.d(TAG, "✅ UI views initialized successfully");
        } catch (Exception e) {
            Log.e(TAG, "❌ Error initializing views", e);
            showError("UI 初始化失敗");
        }
    }
    
    private void setupCallbacks() {
        Log.d(TAG, "Setting up callbacks...");
        
        // Vuforia 初始化回調
        vuforiaCoreManager.setInitializationCallback(new VuforiaCoreManager.InitializationCallback() {
            @Override
            public void onVuforiaInitialized(boolean success) {
                if (success) {
                    Log.d(TAG, "✅ Vuforia initialized successfully");
                    vuforiaInitialized = true;
                    
                    // 🔧 移除这行：不再设置 Filament Surface
                    // vuforiaCoreManager.setupRenderingSurface(filamentSurface);
                    
                    // 启动 Vuforia 引擎
                    if (vuforiaCoreManager.startVuforiaEngine()) {
                        Log.d(TAG, "✅ Vuforia Engine started - Camera should show automatically!");
                        
                        // 🔧 移除 Filament 相关
                        // ensureFilamentRendering();
                        
                        // 加载模型
                        loadGLBModel();
                    } else {
                        Log.e(TAG, "❌ Failed to start Vuforia Engine");
                        showError("Vuforia 引擎启动失败");
                    }
                } else {
                    Log.e(TAG, "❌ Vuforia initialization failed");
                    vuforiaInitialized = false;
                    showError("Vuforia 初始化失败");
                }
            }
        });
        
        // 模型載入回調
        vuforiaCoreManager.setModelLoadingCallback(new VuforiaCoreManager.ModelLoadingCallback() {
            @Override
            public void onModelLoaded(boolean success) {
                Log.d(TAG, "📦 GLB model loading result: " + (success ? "成功" : "失敗"));
                if (success) {
                    // 啟動 AR 會話
                    startARSession();
                } else {
                    showError("3D 模型載入失敗");
                }
            }
        });
        
        // 目標檢測回調
        vuforiaCoreManager.setTargetDetectionCallback(new VuforiaCoreManager.TargetDetectionCallback() {
            @Override
            public void onTargetFound(String targetName) {
                Log.d(TAG, "🎯 Target found: " + targetName);
                runOnUiThread(() -> {
                    Toast.makeText(MainActivity.this, "發現目標: " + targetName, Toast.LENGTH_SHORT).show();
                });
                
                // 通知 Filament 顯示 3D 模型
                if (filamentRenderer != null) {
                    // TODO: 在 Filament 中顯示 3D 模型
                    Log.d(TAG, "📱 Should display 3D model in Filament");
                }
            }
            
            @Override
            public void onTargetLost(String targetName) {
                Log.d(TAG, "❌ Target lost: " + targetName);
                runOnUiThread(() -> {
                    Toast.makeText(MainActivity.this, "目標丟失: " + targetName, Toast.LENGTH_SHORT).show();
                });
                
                // 隱藏 3D 模型
                if (filamentRenderer != null) {
                    // TODO: 在 Filament 中隱藏 3D 模型
                    Log.d(TAG, "📱 Should hide 3D model in Filament");
                }
            }
            
            @Override
            public void onTargetTracking(String targetName, float[] modelViewMatrix) {
                // 更新 3D 模型位置和姿態
                if (filamentRenderer != null) {
                    // TODO: 更新 Filament 中的變換矩陣
                    // Log.d(TAG, "📡 Updating model transform for: " + targetName);
                }
            }
        });
        
        Log.d(TAG, "✅ Callbacks setup completed");
    }
    
    private void requestCameraPermissions() {
        Log.d(TAG, "Requesting camera permissions...");
        
        if (allPermissionsGranted()) {
            Log.d(TAG, "✅ All permissions already granted");
            onPermissionsGranted();
        } else {
            Log.d(TAG, "📋 Requesting camera permissions");
            ActivityCompat.requestPermissions(this, REQUIRED_PERMISSIONS, REQUEST_CODE_PERMISSIONS);
        }
    }
    
    private void onPermissionsGranted() {
        Log.d(TAG, "🔑 Camera permissions granted");
        
        // 如果已經初始化過，不要重複初始化
        if (vuforiaInitialized) {
            Log.d(TAG, "✅ Vuforia already initialized, skipping initialization");
            return;
        }
        
        // 如果還沒請求過初始化，現在開始
        if (!arInitializationRequested) {
            arInitializationRequested = true;
            
            // 延遲初始化（給 Vuforia 11.x 足夠時間）
            new android.os.Handler(android.os.Looper.getMainLooper()).postDelayed(() -> {
                if (!vuforiaInitialized) {
                    Log.d(TAG, "🚀 Starting AR initialization with 3s delay");
                    initializeAR();
                } else {
                    Log.d(TAG, "✅ Vuforia initialized while waiting");
                }
            }, 3000);
        }
    }
    
    private void initializeAR() {
        Log.d(TAG, "🔄 Starting AR system initialization...");
        
        try {
            // 1. 設置 Filament Surface（用於3D模型渲染）
            if (filamentRenderer != null && filamentSurface != null) {
                Log.d(TAG, "🎬 Setting up Filament surface...");
                filamentRenderer.setupSurface(filamentSurface);
                
                // 啟動 Filament 渲染循環
                Log.d(TAG, "▶️ Starting Filament rendering...");
                filamentRenderer.startRendering();
                
                // 等待一下讓 Filament 初始化完成
                new android.os.Handler(android.os.Looper.getMainLooper()).postDelayed(() -> {
                    // 2. 檢查 GLB 檔案
                    checkGLBFiles();
                    
                    // 3. 初始化 Vuforia（這會觸發回調鏈，Surface會在回調中設置）
                    Log.d(TAG, "🎯 Starting Vuforia initialization...");
                    vuforiaCoreManager.setupVuforia();
                }, 500);
                
            } else {
                Log.e(TAG, "❌ FilamentRenderer or SurfaceView is null");
                showError("Filament 組件未就緒");
            }
            
        } catch (Exception e) {
            Log.e(TAG, "❌ Error during AR initialization", e);
            showError("AR 系統初始化失敗: " + e.getMessage());
        }
    }
    
    private void ensureFilamentRendering() {
        if (filamentRenderer != null) {
            if (!filamentRenderer.isRendering()) {
                Log.d(TAG, "🔄 Filament not rendering, starting now...");
                filamentRenderer.startRendering();
            } else {
                Log.d(TAG, "✅ Filament already rendering");
            }
        }
    }
    
    private void checkGLBFiles() {
        try {
            String[] glbFiles = glbReader.listAvailableGLBFiles();
            Log.d(TAG, "📁 Available GLB files: " + java.util.Arrays.toString(glbFiles));
            
            GLBReader.GLBFileInfo giraffeInfo = glbReader.getGLBFileInfo("giraffe_voxel.glb");
            Log.d(TAG, "🦒 Giraffe model info: " + giraffeInfo.fileName + 
                      " - Valid: " + giraffeInfo.isValid + 
                      " - Size: " + giraffeInfo.fileSize + " bytes");
        } catch (Exception e) {
            Log.e(TAG, "❌ Error checking GLB files", e);
        }
    }
    
    private void loadGLBModel() {
        try {
            Log.d(TAG, "📦 Loading GLB model...");
            
            GLBReader.GLBFileInfo glbInfo = glbReader.getGLBFileInfo("giraffe_voxel.glb");
            if (glbInfo.isValid) {
                Log.d(TAG, "✅ GLB file valid, loading: " + glbInfo.fileName);
                vuforiaCoreManager.loadGiraffeModel();
            } else {
                Log.e(TAG, "❌ GLB file invalid: " + glbInfo.fileName);
                showError("GLB 檔案無效");
            }
        } catch (Exception e) {
            Log.e(TAG, "❌ Error loading GLB model", e);
            showError("GLB 模型載入錯誤: " + e.getMessage());
        }
    }
    
    private void startARSession() {
        try {
            Log.d(TAG, "🎉 Starting AR session...");
            
            // 啟動目標檢測
            if (vuforiaCoreManager.startTargetDetection()) {
                Log.d(TAG, "🎯 Target detection started successfully");
                
                runOnUiThread(() -> {
                    Toast.makeText(this, "AR 系統就緒！請將相機對準目標圖像", Toast.LENGTH_LONG).show();
                });
                
                Log.d(TAG, "🎉 AR session started successfully!");
            } else {
                Log.e(TAG, "❌ Failed to start target detection");
                showError("目標檢測啟動失敗");
            }
            
        } catch (Exception e) {
            Log.e(TAG, "❌ Error starting AR session", e);
            showError("AR 會話啟動失敗: " + e.getMessage());
        }
    }
    
    private void showError(String message) {
        Log.e(TAG, "🚨 Error: " + message);
        runOnUiThread(() -> {
            Toast.makeText(this, message, Toast.LENGTH_LONG).show();
        });
    }

    // ==================== 權限處理 ====================
    
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
                Log.d(TAG, "✅ Camera permissions granted by user");
                onPermissionsGranted();
            } else {
                Log.e(TAG, "❌ Camera permissions denied by user");
                showPermissionDeniedDialog();
            }
        }
    }
    
    private void showPermissionDeniedDialog() {
        new AlertDialog.Builder(this)
            .setTitle("需要相機權限")
            .setMessage("此應用需要相機權限才能使用 AR 功能。請授予相機權限。")
            .setPositiveButton("前往設置", (dialog, which) -> {
                try {
                    Intent intent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
                    Uri uri = Uri.fromParts("package", getPackageName(), null);
                    intent.setData(uri);
                    startActivity(intent);
                } catch (Exception e) {
                    Log.e(TAG, "無法打開設置頁面", e);
                    showError("請手動前往設置授予相機權限");
                }
            })
            .setNegativeButton("退出應用", (dialog, which) -> {
                finish();
            })
            .setCancelable(false)
            .show();
    }

    // ==================== 生命週期管理 ====================
    
    @Override
    protected void onResume() {
        super.onResume();
        Log.d(TAG, "▶️ onResume - Resuming AR components");
        
        // 恢復 Filament 渲染
        if (filamentRenderer != null && filamentInitialized) {
            Log.d(TAG, "▶️ Resuming Filament renderer");
            filamentRenderer.resume();
        }
        
        // 恢復 Vuforia
        if (vuforiaCoreManager != null && vuforiaInitialized) {
            Log.d(TAG, "▶️ Resuming Vuforia");
            try {
                vuforiaCoreManager.resumeVuforia();
            } catch (Exception e) {
                Log.e(TAG, "❌ Error resuming Vuforia", e);
            }
        }
        
        // 如果權限已授予但還沒初始化，現在初始化
        if (allPermissionsGranted() && !vuforiaInitialized && !arInitializationRequested) {
            Log.d(TAG, "🔄 Permissions granted but not initialized, starting initialization");
            onPermissionsGranted();
        }
    }
    
    @Override
    protected void onPause() {
        super.onPause();
        Log.d(TAG, "⏸️ onPause - Pausing AR components");
        
        // 暫停 Filament 渲染
        if (filamentRenderer != null) {
            Log.d(TAG, "⏸️ Pausing Filament renderer");
            filamentRenderer.pause();
        }
        
        // 暫停 Vuforia
        if (vuforiaCoreManager != null && vuforiaInitialized) {
            Log.d(TAG, "⏸️ Pausing Vuforia");
            try {
                vuforiaCoreManager.pauseVuforia();
            } catch (Exception e) {
                Log.e(TAG, "❌ Error pausing Vuforia", e);
            }
        }
    }
    
    @Override
    protected void onDestroy() {
        super.onDestroy();
        Log.d(TAG, "🗑️ onDestroy - Cleaning up AR resources");
        
        // 清理 Vuforia
        if (vuforiaCoreManager != null) {
            Log.d(TAG, "🗑️ Cleaning up VuforiaCoreManager");
            try {
                vuforiaCoreManager.cleanupManager();
            } catch (Exception e) {
                Log.e(TAG, "❌ Error cleaning up Vuforia", e);
            }
            vuforiaCoreManager = null;
        }
        
        // 清理 Filament
        if (filamentRenderer != null) {
            Log.d(TAG, "🗑️ Destroying FilamentRenderer");
            try {
                filamentRenderer.destroy();
            } catch (Exception e) {
                Log.e(TAG, "❌ Error destroying Filament", e);
            }
            filamentRenderer = null;
        }
        
        // 清理其他組件
        glbReader = null;
        
        Log.d(TAG, "✅ MainActivity destroyed completely");
    }
    
    // ==================== 狀態檢查輔助方法 ====================
    
    private boolean isVuforiaInitialized() {
        return vuforiaInitialized && vuforiaCoreManager != null && vuforiaCoreManager.isVuforiaInitialized();
    }
    
    private boolean isFilamentInitialized() {
        return filamentInitialized && filamentRenderer != null;
    }
    
    private boolean isSystemReady() {
        return isVuforiaInitialized() && isFilamentInitialized() && allPermissionsGranted();
    }
}