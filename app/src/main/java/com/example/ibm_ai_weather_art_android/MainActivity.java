package com.example.ibm_ai_weather_art_android;

import android.os.Bundle;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;
import androidx.appcompat.app.AppCompatActivity;
import android.util.Log;
import android.widget.FrameLayout;

// Filament imports
import com.google.android.filament.Engine;

// Import our custom classes
import com.example.ibm_ai_weather_art_android.camera.CameraPermissionManager;
import com.example.ibm_ai_weather_art_android.camera.ARCameraController;
import com.example.ibm_ai_weather_art_android.model.GLBReader;
import com.example.ibm_ai_weather_art_android.model.ModelRenderer;
import com.example.ibm_ai_weather_art_android.model.ARObjectPlacer;
import com.example.ibm_ai_weather_art_android.ui.ARMainView;
import com.example.ibm_ai_weather_art_android.ui.ControlPanel;

/**
 * Main activity - integrates all components
 * This class only coordinates components, no business logic inside
 */
public class MainActivity extends AppCompatActivity {

    private static final String TAG = "MainActivity";
    
    // 加載 Filament JNI - 使用正確的初始化方式
    static {
        if (!loadFilamentLibraries()) {
            Log.e(TAG, "Filament not available on this device");
        }
    }
    
    private static boolean loadFilamentLibraries() {
        try {
            // 1. 首先載入 filament-jni
            System.loadLibrary("filament-jni");
            Log.d(TAG, "filament-jni loaded successfully");
            
            // 2. 然後載入 gltfio-jni (這個是關鍵！)
            System.loadLibrary("gltfio-jni");
            Log.d(TAG, "gltfio-jni loaded successfully");
            
            Log.d(TAG, "All Filament libraries loaded successfully");
            return true;
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "Failed to load Filament libraries", e);
            return false;
        }
    }

    // UI components
    private TextView statusText;
    private Button btnLoadModel, btnPlaceObject;

    // Vuforia component
    private VuforiaManager vuforiaManager;

    // Functional components
    private CameraPermissionManager permissionManager;
    private ARCameraController cameraController;
    private GLBReader glbReader;
    private ModelRenderer modelRenderer;
    private ARObjectPlacer objectPlacer;
    private ARMainView mainView;
    private ControlPanel controlPanel;

    // Status variables
    private static final String DEFAULT_MODEL = "giraffe_voxel.glb";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        
        try {
            // 檢查 Filament 是否可用再進行測試
            if (isFilamentLoaded()) {
                testFilament();
                initializeViews();
                initializeComponents();
                startApplication();
            } else {
                // 顯示錯誤訊息或使用替代方案
                Log.e(TAG, "Cannot proceed - Filament libraries not loaded");
                Toast.makeText(this, "Filament not available on this device", Toast.LENGTH_LONG).show();
                // 仍然初始化基本功能
                initializeViews();
                initializeComponents();
                startApplication();
            }
        } catch (Exception e) {
            Log.e(TAG, "Error in onCreate", e);
            Toast.makeText(this, "Error initializing app: " + e.getMessage(), Toast.LENGTH_LONG).show();
        }
    }
    
    private boolean isFilamentLoaded() {
        try {
            // 簡單測試是否可以創建 Engine
            Engine testEngine = Engine.create();
            testEngine.destroy();
            return true;
        } catch (Exception e) {
            Log.e(TAG, "Filament not available", e);
            return false;
        }
    }
    
    private void testFilament() {
        try {
            Log.d(TAG, "Testing Filament...");
            // 測試 Filament 類是否可用
            Class<?> engineClass = Class.forName("com.google.android.filament.Engine");
            Log.d(TAG, "Filament Engine class found: " + engineClass.getName());
            
            // 測試 FilamentRenderer 是否可用
            FilamentRenderer filamentRenderer = new FilamentRenderer(this);
            if (filamentRenderer.isInitialized()) {
                Log.d(TAG, "FilamentRenderer initialized successfully");
            } else {
                Log.e(TAG, "FilamentRenderer initialization failed");
            }
        } catch (Exception e) {
            Log.e(TAG, "Filament test failed", e);
        }
    }

    /**
     * Initialize UI views
     */
    private void initializeViews() {
        statusText = findViewById(R.id.tv_status);
        btnLoadModel = findViewById(R.id.btn_load_model);
        btnPlaceObject = findViewById(R.id.btn_place_object);
    }

    /**
     * Initialize functional components
     */
    private void initializeComponents() {
        // Initialize Vuforia manager
        vuforiaManager = new VuforiaManager(this);

        // Initialize permission manager
        permissionManager = new CameraPermissionManager(this, new CameraPermissionManager.PermissionCallback() {
            @Override
            public void onPermissionGranted() {
                mainView.showSuccess("Camera permission granted");
                initializeVuforia();
            }

            @Override
            public void onPermissionDenied() {
                mainView.showError("Camera permission required for AR functionality");
                Toast.makeText(MainActivity.this, "Please grant camera permission", Toast.LENGTH_LONG).show();
            }
        });

        // Initialize main view
        mainView = new ARMainView(this, statusText, vuforiaManager, new ARMainView.ViewCallback() {
            @Override
            public void onViewInitialized() {
                // View initialization complete
            }

            @Override
            public void onViewReady() {
                mainView.showSuccess("Vuforia view ready");
            }

            @Override
            public void onViewError(String error) {
                mainView.showError(error);
            }
        });

        // Initialize control panel
        controlPanel = new ControlPanel(btnLoadModel, btnPlaceObject, new ControlPanel.ControlCallback() {
            @Override
            public void onLoadModelClicked() {
                loadModel();
            }

            @Override
            public void onPlaceObjectClicked() {
                enableObjectPlacement();
            }

            @Override
            public void onClearObjectsClicked() {
                clearAllObjects();
            }
        });

        // Initialize GLB reader
        glbReader = new GLBReader(this);

        // Initialize model renderer
        modelRenderer = new ModelRenderer(this, vuforiaManager, new ModelRenderer.RenderCallback() {
            @Override
            public void onRenderStarted(String fileName) {
                controlPanel.onModelLoadingStarted();
                mainView.showLoading("Loading model: " + fileName);
            }

            @Override
            public void onRenderSuccess(String fileName, String modelPath) {
                controlPanel.onModelLoadingSuccess();
                mainView.showSuccess("Model loaded successfully: " + fileName);
                setupObjectPlacer(modelPath);
            }

            @Override
            public void onRenderFailed(String fileName, String error) {
                controlPanel.onModelLoadingFailed();
                mainView.showError("Model loading failed: " + error);
            }

            @Override
            public void onBatchRenderStarted(int totalCount) {
                mainView.showLoading("Starting batch load of " + totalCount + " models");
            }

            @Override
            public void onBatchRenderProgress(int completed, int total) {
                mainView.updateStatus("Loading progress: " + completed + "/" + total);
            }

            @Override
            public void onBatchRenderCompleted() {
                mainView.showSuccess("All models loaded successfully");
            }
        });
    }

    /**
     * Start application
     */
    private void startApplication() {
        mainView.updateStatus("Checking camera permissions...");
        
        // Set up camera container immediately
        FrameLayout cameraContainer = findViewById(R.id.vuforia_camera_view);
        vuforiaManager.setCameraContainer(cameraContainer);
        
        permissionManager.checkAndRequestPermission();
    }

    /**
     * Initialize Vuforia functionality
     */
    private void initializeVuforia() {
        // Initialize Vuforia camera controller
        cameraController = new ARCameraController(this, new ARCameraController.ARCallback() {
            @Override
            public void onVuforiaInitialized() {
                mainView.showSuccess("Vuforia initialized successfully");
                controlPanel.onARInitialized();
            }

            @Override
            public void onVuforiaError(String error) {
                mainView.showError("Vuforia error: " + error);
                controlPanel.onARInitializationFailed();
            }

            @Override
            public void onVuforiaSessionStarted() {
                mainView.showSuccess("Vuforia session started");
            }

            @Override
            public void onVuforiaSessionPaused() {
                mainView.updateStatus("Vuforia session paused");
            }

            @Override
            public void onVuforiaSessionStopped() {
                mainView.updateStatus("Vuforia session stopped");
            }
        });

        // Start Vuforia initialization
        cameraController.initializeVuforia();
    }

    /**
     * Load 3D model
     */
    private void loadModel() {
        // Check if default model exists
        if (!glbReader.checkGLBExists(DEFAULT_MODEL)) {
            mainView.showError("Model file not found: " + DEFAULT_MODEL);
            mainView.updateStatus("Please place " + DEFAULT_MODEL + " in assets/models/ folder");
            return;
        }

        // Check if Filament is initialized
        if (!vuforiaManager.isFilamentInitialized()) {
            mainView.showError("Filament renderer not initialized");
            mainView.updateStatus("3D rendering engine not ready");
            return;
        }

        // Load model with Filament
        mainView.showLoading("Loading giraffe model with Filament...");
        if (vuforiaManager.loadGiraffeModel()) {
            mainView.showSuccess("Giraffe model loaded successfully with Filament");
            btnPlaceObject.setEnabled(true);
            mainView.updateStatus("Model ready! Click 'Start AR' to see 3D giraffe");
        } else {
            mainView.showError("Failed to load giraffe model with Filament");
            btnPlaceObject.setEnabled(false);
            mainView.updateStatus("Model loading failed. Please try again.");
        }
    }

    /**
     * Setup object placer
     */
    private void setupObjectPlacer(String modelPath) {
        objectPlacer = new ARObjectPlacer(this, vuforiaManager, new ARObjectPlacer.PlacementCallback() {
            @Override
            public void onPlacementReady() {
                mainView.updateStatus("Click on screen plane to place object");
            }

            @Override
            public void onObjectPlaced(Object node, Object anchor) {
                mainView.showSuccess("Object placed! You can move, rotate and scale");
                controlPanel.onObjectPlaced();
            }

            @Override
            public void onPlacementFailed(String error) {
                mainView.showError("Placement failed: " + error);
            }

            @Override
            public void onAllObjectsRemoved() {
                mainView.showSuccess("All objects cleared");
                controlPanel.onAllObjectsCleared();
            }
        });
        objectPlacer.setModel(modelPath);
    }

    /**
     * Enable object placement
     */
    private void enableObjectPlacement() {
        if (!vuforiaManager.isModelLoaded()) {
            mainView.showError("Please load the model first");
            mainView.updateStatus("Click 'Load Giraffe Model' before starting AR");
            return;
        }

        mainView.showLoading("Starting Vuforia + Filament AR rendering...");
        
        if (vuforiaManager.startAR()) {
            mainView.showSuccess("AR started! Vuforia tracking + Filament rendering active");
            mainView.updateStatus("AR active! You should see the 3D giraffe model");
            btnPlaceObject.setText("AR Active");
            btnPlaceObject.setEnabled(false);
        } else {
            mainView.showError("Failed to start AR rendering");
            mainView.updateStatus("Make sure model is loaded first");
        }
    }

    /**
     * Clear all objects
     */
    private void clearAllObjects() {
        if (objectPlacer != null) {
            objectPlacer.removeAllObjects();
        }
    }

    /**
     * Handle permission request results
     */
    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        permissionManager.handlePermissionResult(requestCode, grantResults);
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (cameraController != null) {
            cameraController.startVuforiaSession();
        }
        // Update camera orientation when resuming
        if (vuforiaManager != null) {
            // Force camera orientation update
            mainView.updateStatus("Updating camera orientation...");
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (cameraController != null) {
            cameraController.pauseVuforiaSession();
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (cameraController != null) {
            cameraController.stopVuforiaSession();
        }
        if (vuforiaManager != null) {
            vuforiaManager.onDestroy();
        }
    }

    @Override
    public void onConfigurationChanged(android.content.res.Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        // Handle screen rotation
        mainView.updateStatus("Screen rotated, updating camera...");
        // Camera orientation will be updated automatically in VuforiaManager
    }
}