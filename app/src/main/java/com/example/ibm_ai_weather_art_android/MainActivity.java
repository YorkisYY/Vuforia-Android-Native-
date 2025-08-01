package com.example.ibm_ai_weather_art_android;

import android.Manifest;
import android.content.pm.PackageManager;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.util.Log;
import android.widget.Toast;
import android.widget.FrameLayout;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import android.app.AlertDialog;
import android.content.Intent;
import android.net.Uri;
import android.provider.Settings;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public class MainActivity extends AppCompatActivity implements GLSurfaceView.Renderer {
    private static final String TAG = "MainActivity";
    private static final int REQUEST_CODE_PERMISSIONS = 10;
    private static final String[] REQUIRED_PERMISSIONS = new String[]{Manifest.permission.CAMERA};
    
    // 狀態管理
    private volatile boolean arInitializationRequested = false;
    private volatile boolean vuforiaInitialized = false;
    private volatile boolean openglInitialized = false;
    

    private VuforiaCoreManager vuforiaCoreManager;

    private FrameLayout cameraContainer;
    private GLSurfaceView glSurfaceView;  // 替代 SurfaceView
    
    static {
        // 移除 Filament 初始化
        // Filament.init();
        
        // 只載入 Vuforia 庫
        try {
            System.loadLibrary("Vuforia");
            Log.d("MainActivity", "✅ libVuforia.so loaded successfully");
        } catch (UnsatisfiedLinkError e) {
            Log.e("MainActivity", "❌ Failed to load libVuforia.so: " + e.getMessage());
        }
        
        // 載入您的 native wrapper
        try {
            System.loadLibrary("vuforia_wrapper");
            Log.d("MainActivity", "✅ vuforia_wrapper loaded successfully");
        } catch (UnsatisfiedLinkError e) {
            Log.e("MainActivity", "❌ Failed to load vuforia_wrapper: " + e.getMessage());
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        
        Log.d(TAG, "🚀 MainActivity onCreate started");
        
        // 初始化核心組件
        initializeCoreComponents();
        
        // 初始化視圖 - 使用 GLSurfaceView
        initGLViews();
        
        // 設置回調
        setupCallbacks();
        
        // 請求權限
        requestCameraPermissions();
        
        Log.d(TAG, "✅ MainActivity onCreate completed");
    }

    private void initializeCoreComponents() {
        Log.d(TAG, "Initializing core components...");
        
        try {
            // 只創建 VuforiaCoreManager
            vuforiaCoreManager = new VuforiaCoreManager(this);
            Log.d(TAG, "✅ VuforiaCoreManager created");
            
            // 移除 Filament 和 GLBReader
            // filamentRenderer = new FilamentRenderer(this);
            // glbReader = new GLBReader(this);
            
        } catch (Exception e) {
            Log.e(TAG, "❌ Error initializing core components", e);
            showError("核心組件初始化失敗");
        }
    }
    
    private void initGLViews() {
        Log.d(TAG, "Initializing OpenGL views...");
        
        try {
            cameraContainer = findViewById(R.id.cameraContainer);
            if (cameraContainer == null) {
                Log.e(TAG, "❌ Camera container not found in layout");
                showError("UI 初始化失敗：找不到相機容器");
                return;
            }
            
            // 🔥 關鍵變更：創建 GLSurfaceView
            glSurfaceView = new GLSurfaceView(this);
            glSurfaceView.setEGLContextClientVersion(3); // OpenGL ES 3.0
            glSurfaceView.setRenderer(this); // MainActivity 實現 GLSurfaceView.Renderer
            glSurfaceView.setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);
            
            // 將 GLSurfaceView 添加到容器
            cameraContainer.removeAllViews();
            cameraContainer.addView(glSurfaceView);
            
            Log.d(TAG, "✅ GLSurfaceView created and added to container");
            
        } catch (Exception e) {
            Log.e(TAG, "❌ Error initializing GL views", e);
            showError("OpenGL 視圖初始化失敗");
        }
    }
    
    // ==================== GLSurfaceView.Renderer 實現 ====================
    
    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        Log.d(TAG, "🎨 OpenGL Surface Created");
        
        // 如果 Vuforia 已經初始化，設置 OpenGL
        if (vuforiaInitialized && vuforiaCoreManager != null) {
            setupVuforiaOpenGL();
        }
    }
    
    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height) {
        Log.d(TAG, "🎨 OpenGL Surface Changed: " + width + "x" + height);
        
        // 通知 Vuforia surface 變化
        if (vuforiaCoreManager != null) {
            try {
                vuforiaCoreManager.handleSurfaceChanged(width, height);
                
                // 如果還沒設置 OpenGL，現在設置
                if (!openglInitialized) {
                    setupVuforiaOpenGL();
                }
            } catch (Exception e) {
                Log.e(TAG, "Error handling surface change", e);
            }
        }
    }
    
    @Override
    public void onDrawFrame(GL10 gl) {
        // 🔥 關鍵：每一幀都調用 Vuforia 渲染
        if (vuforiaCoreManager != null && vuforiaInitialized && openglInitialized) {
            try {
                // 這會渲染相機背景 + AR 內容
                vuforiaCoreManager.renderFrameSafely();
            } catch (Exception e) {
                Log.e(TAG, "Rendering error: " + e.getMessage());
            }
        }
    }
    
    // ==================== Vuforia OpenGL 設置 ====================
    
    private void setupVuforiaOpenGL() {
        Log.d(TAG, "🎨 Setting up Vuforia OpenGL rendering...");
        
        try {
                    if (vuforiaCoreManager.isOpenGLInitialized()) {
                        Log.d(TAG, "✅ OpenGL already initialized");
                        openglInitialized = true;
                        
                        Log.d(TAG, "🎉 Vuforia OpenGL setup completed successfully!");
                        
                        // 開始目標檢測
                        startARSession();
                        
                    } else {
                        Log.d(TAG, "⏳ OpenGL not ready yet, will initialize when surface is ready");
                        // 不需要做任何事，OpenGL 會在 GLSurfaceView 準備好時自動初始化
                    }
                } catch (Exception e) {
                    Log.e(TAG, "❌ Error checking Vuforia OpenGL status", e);
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
                    
                    // 启动 Vuforia 引擎
                    if (vuforiaCoreManager.startVuforiaEngine()) {
                        Log.d(TAG, "✅ Vuforia Engine started");
                        
                        // 加载模型
                        loadGLBModel();
                        
                        // OpenGL 設置會在 onSurfaceChanged 中進行
                        
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
                if (!success) {
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
            }
            
            @Override
            public void onTargetLost(String targetName) {
                Log.d(TAG, "❌ Target lost: " + targetName);
                runOnUiThread(() -> {
                    Toast.makeText(MainActivity.this, "目標丟失: " + targetName, Toast.LENGTH_SHORT).show();
                });
            }
            
            @Override
            public void onTargetTracking(String targetName, float[] modelViewMatrix) {
                // 目標追蹤更新會在 native 層處理
                // Log.d(TAG, "📡 Target tracking: " + targetName);
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
            
            // 延遲初始化
            new android.os.Handler(android.os.Looper.getMainLooper()).postDelayed(() -> {
                if (!vuforiaInitialized) {
                    Log.d(TAG, "🚀 Starting AR initialization");
                    initializeAR();
                } else {
                    Log.d(TAG, "✅ Vuforia initialized while waiting");
                }
            }, 1000); // 減少延遲時間
        }
    }
    
    private void initializeAR() {
        Log.d(TAG, "🔄 Starting AR system initialization...");
        
        try {
            // 直接初始化 Vuforia，OpenGL 會在 Surface 準備好時自動設置
            Log.d(TAG, "🎯 Starting Vuforia initialization...");
            vuforiaCoreManager.setupVuforia();
            
        } catch (Exception e) {
            Log.e(TAG, "❌ Error during AR initialization", e);
            showError("AR 系統初始化失敗: " + e.getMessage());
        }
    }
    
    private void loadGLBModel() {
        try {
            Log.d(TAG, "📦 Loading GLB model...");
            vuforiaCoreManager.loadGiraffeModel();
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
        
        // 恢復 GLSurfaceView
        if (glSurfaceView != null) {
            glSurfaceView.onResume();
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
        
        // 暫停 GLSurfaceView
        if (glSurfaceView != null) {
            glSurfaceView.onPause();
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
        
        // 清理 GLSurfaceView
        glSurfaceView = null;
        
        Log.d(TAG, "✅ MainActivity destroyed completely");
    }
}