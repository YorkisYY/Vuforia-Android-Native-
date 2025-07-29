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
    private volatile boolean arInitializationRequested = false;
    private static final String TAG = "MainActivity";
    private static final int REQUEST_CODE_PERMISSIONS = 10;
    private static final String[] REQUIRED_PERMISSIONS = new String[]{Manifest.permission.CAMERA};
    private boolean isVuforiaInitialized() {
    return vuforiaCoreManager != null && vuforiaCoreManager.isVuforiaInitialized();
}

    
    static {
        Filament.init();
        try {
            System.loadLibrary("gltfio-jni");
        } catch (UnsatisfiedLinkError e) {
            Log.e("MainActivity", "gltfio library not found", e);
        }
    }
    
    static {
        try {
            System.loadLibrary("Vuforia");
            Log.d("MainActivity", "✅ libVuforia.so loaded successfully");
        } catch (UnsatisfiedLinkError e) {
            Log.e("MainActivity", "❌ Failed to load libVuforia.so: " + e.getMessage());
        }
    }

    // ✅ 修正：統一使用一個 VuforiaCoreManager 實例
    private VuforiaCoreManager vuforiaCoreManager;
    private FilamentRenderer filamentRenderer;
    private GLBReader glbReader;
    
    // UI 組件
    private FrameLayout cameraContainer;
    private SurfaceView filamentSurface;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        
        // ✅ 修正：只創建一次，傳入 Activity 實例
        vuforiaCoreManager = new VuforiaCoreManager(this);
        
        // 初始化視圖
        initViews();
        
        // 初始化核心組件
        initializeComponents();
        
        // ✅ 強制每次都重新請求相機權限（解決 Vuforia 11.3 Bug）
        ActivityCompat.requestPermissions(this, REQUIRED_PERMISSIONS, REQUEST_CODE_PERMISSIONS);
    }

    private void initializeComponents() {
        try {
            Log.d(TAG, "Initializing core components...");
            
            // ✅ 修正：不重複創建，直接使用已有的實例
            // vuforiaCoreManager 已經在 onCreate 中創建了
            
            // 初始化 Filament 渲染器
            filamentRenderer = new FilamentRenderer(this);
            
            // 初始化 GLB 讀取器
            glbReader = new GLBReader(this);
            
            // 設定回調
            setupVuforiaCallbacks();
            
            Log.d(TAG, "Core components initialized successfully");
        } catch (Exception e) {
            Log.e(TAG, "Error initializing components", e);
        }
    }
    
    private void setupVuforiaCallbacks() {
        // 初始化回調
        vuforiaCoreManager.setInitializationCallback(new VuforiaCoreManager.InitializationCallback() {
            @Override
            public void onVuforiaInitialized(boolean success) {
                if (success) {
                    Log.d(TAG, "✅ Vuforia initialized successfully");
                    loadGLBModel();
                } else {
                    Log.e(TAG, "❌ Vuforia initialization failed");
                    showError("Vuforia 初始化失敗");
                }
            }
        });
        
        // 模型載入回調
        vuforiaCoreManager.setModelLoadingCallback(new VuforiaCoreManager.ModelLoadingCallback() {
            @Override
            public void onModelLoaded(boolean success) {
                Log.d(TAG, "📦 GLB model loaded: " + (success ? "成功" : "失敗"));
                if (success) {
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
                
                // 觸發 Filament 渲染
                if (filamentRenderer != null) {
                    // 顯示 3D 模型
                }
            }
            
            @Override
            public void onTargetLost(String targetName) {
                Log.d(TAG, "❌ Target lost: " + targetName);
                // 隱藏 3D 模型
            }
            
            @Override
            public void onTargetTracking(String targetName, float[] modelViewMatrix) {
                Log.d(TAG, "📡 Target tracking: " + targetName);
                // 更新 3D 模型位置
                if (filamentRenderer != null) {
                    // 更新變換矩陣
                }
            }
        });
    }
    
    private void initViews() {
        Log.d(TAG, "初始化視圖組件");
        
        cameraContainer = findViewById(R.id.cameraContainer);
        if (cameraContainer == null) {
            Log.e(TAG, "Camera container not found");
            return;
        }
        
        filamentSurface = findViewById(R.id.filamentSurface);
        if (filamentSurface == null) {
            Log.e(TAG, "Filament SurfaceView not found");
            return;
        }
        
        Log.d(TAG, "視圖組件初始化完成");
    }
    
    private void initializeAR() {
        Log.d(TAG, "開始初始化 AR 系統");
        
        try {
            // 1. 設定 Filament Surface
            if (filamentRenderer != null && filamentSurface != null) {
                filamentRenderer.setupSurface(filamentSurface);
            }
            
            // 2. 檢查 GLB 檔案
            checkGLBFiles();
            
            // 3. 初始化 Vuforia (會觸發 onVuforiaInitialized 回調)
            vuforiaCoreManager.setupVuforia();
            
        } catch (Exception e) {
            Log.e(TAG, "AR 初始化錯誤: " + e.getMessage());
            showError("AR 初始化失敗");
        }
    }
    
    private void checkGLBFiles() {
        String[] glbFiles = glbReader.listAvailableGLBFiles();
        Log.d(TAG, "可用的 GLB 檔案: " + java.util.Arrays.toString(glbFiles));
        
        GLBReader.GLBFileInfo giraffeInfo = glbReader.getGLBFileInfo("giraffe_voxel.glb");
        Log.d(TAG, "長頸鹿模型資訊: " + giraffeInfo.fileName + " - 有效: " + giraffeInfo.isValid + " - 大小: " + giraffeInfo.fileSize);
    }
    
    private void loadGLBModel() {
        try {
            GLBReader.GLBFileInfo glbInfo = glbReader.getGLBFileInfo("giraffe_voxel.glb");
            if (glbInfo.isValid) {
                Log.d(TAG, "Loading GLB model: " + glbInfo.fileName);
                vuforiaCoreManager.loadGiraffeModel(); // 會觸發 onModelLoaded 回調
            } else {
                showError("GLB 檔案無效: " + glbInfo.fileName);
            }
        } catch (Exception e) {
            Log.e(TAG, "Error loading GLB model", e);
            showError("GLB 模型載入錯誤");
        }
    }
    
    private void startARSession() {
        try {
            Log.d(TAG, "Starting AR session...");
            
            // 啟動目標檢測
            if (vuforiaCoreManager.startTargetDetection()) {
                Log.d(TAG, "🎉 AR 會話啟動成功！");
                Toast.makeText(this, "AR 系統就緒", Toast.LENGTH_SHORT).show();
            } else {
                showError("目標檢測啟動失敗");
            }
            
        } catch (Exception e) {
            Log.e(TAG, "Error starting AR session", e);
            showError("AR 會話啟動失敗");
        }
    }
    
    private void showError(String message) {
        runOnUiThread(() -> {
            Toast.makeText(this, message, Toast.LENGTH_LONG).show();
        });
    }
    
    // ==================== 生命週期管理 ====================
    
    @Override
    protected void onPause() {
        super.onPause();
        Log.d(TAG, "onPause - 暫停 AR 組件");
        
        // ✅ 修正：統一使用 vuforiaCoreManager
        try {
            if (vuforiaCoreManager != null) {
                vuforiaCoreManager.pauseVuforia();
            }
        } catch (Exception e) {
            Log.e(TAG, "Error during Vuforia pause", e);
        }
    }
    // 替换您的 onResume 方法为以下代码：

    @Override
    protected void onResume() {
        super.onResume();
        Log.d(TAG, "onResume - 恢復 AR 組件");
        
        // ✅ 如果已經成功初始化，只需要恢復
        if (isVuforiaInitialized()) {
            Log.d(TAG, "✅ Vuforia already initialized, just resuming...");
            try {
                if (vuforiaCoreManager != null) {
                    vuforiaCoreManager.resumeVuforia();
                }
            } catch (Exception e) {
                Log.e(TAG, "Error during Vuforia resume", e);
            }
            return;
        }
        
        // ✅ 如果有權限但還沒初始化，且沒有請求過初始化
        if (allPermissionsGranted() && !arInitializationRequested) {
            Log.d(TAG, "🔄 Permissions granted, requesting AR initialization");
            arInitializationRequested = true;
            
            new android.os.Handler(android.os.Looper.getMainLooper()).postDelayed(() -> {
                if (!isVuforiaInitialized()) {  // 雙重檢查
                    Log.d(TAG, "🚀 Starting delayed AR initialization from onResume");
                    initializeAR();
                } else {
                    Log.d(TAG, "✅ Vuforia initialized while waiting");
                }
            }, 500);
        }
    }
    
    @Override
    protected void onDestroy() {
        super.onDestroy();
        Log.d(TAG, "onDestroy - 清理 AR 資源");
        
        // ✅ 修正：統一使用 vuforiaCoreManager
        if (vuforiaCoreManager != null) {
            vuforiaCoreManager.cleanupManager();
        }
        
        if (filamentRenderer != null) {
            filamentRenderer.destroy();
        }
    }

    // ==================== 權限檢查 ====================
    
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
                Log.d(TAG, "🔄 Camera permission granted");
                
                // ✅ 如果已經初始化成功，不需要再初始化
                if (isVuforiaInitialized()) {
                    Log.d(TAG, "✅ Vuforia already initialized, no need to initialize again");
                    return;
                }
                
                // ✅ 如果還沒請求過初始化，現在請求
                if (!arInitializationRequested) {
                    arInitializationRequested = true;
                    
                    new android.os.Handler(android.os.Looper.getMainLooper()).postDelayed(() -> {
                        if (!isVuforiaInitialized()) {
                            Log.d(TAG, "🚀 Starting delayed AR initialization (3s delay for Vuforia 11.x)");
                            initializeAR();
                        } else {
                            Log.d(TAG, "✅ Vuforia initialized while waiting");
                        }
                    }, 3000); // 3秒延遲
                } else {
                    Log.d(TAG, "⚠️ AR initialization already requested");
                }
            } else {
                Toast.makeText(this, "需要相機權限", Toast.LENGTH_SHORT).show();
                finish();
            }
        }
    }
    private void showPermissionResetDialog() {
        new AlertDialog.Builder(this)
            .setTitle("Vuforia 初始化失敗")
            .setMessage("這是 Vuforia 11.x 的已知問題。\n\n解決方法：\n1. 點擊'前往設置'\n2. 關閉相機權限\n3. 重新啟動應用\n4. 重新授予相機權限")
            .setPositiveButton("前往設置", (dialog, which) -> {
                try {
                    Intent intent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
                    Uri uri = Uri.fromParts("package", getPackageName(), null);
                    intent.setData(uri);
                    startActivity(intent);
                } catch (Exception e) {
                    Log.e(TAG, "無法打開設置頁面", e);
                    Toast.makeText(this, "請手動前往設置關閉相機權限", Toast.LENGTH_LONG).show();
                }
            })
            .setNegativeButton("重試", (dialog, which) -> {
                // ✅ 重置請求狀態並重試
                arInitializationRequested = false;
                ActivityCompat.requestPermissions(this, REQUIRED_PERMISSIONS, REQUEST_CODE_PERMISSIONS);
            })
            .setCancelable(false)
            .show();
    }
    }