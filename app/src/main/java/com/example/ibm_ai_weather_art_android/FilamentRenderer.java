package com.example.ibm_ai_weather_art_android;

import android.content.Context;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceView;
import android.view.Choreographer;
import android.opengl.Matrix;
import java.nio.ByteBuffer;

import com.google.android.filament.Engine;
import com.google.android.filament.Renderer;
import com.google.android.filament.Scene;
import com.google.android.filament.View;
import com.google.android.filament.Camera;
import com.google.android.filament.SwapChain;
import com.google.android.filament.Viewport;
import com.google.android.filament.LightManager;
import com.google.android.filament.EntityManager;
import com.google.android.filament.RenderableManager;
import com.google.android.filament.TransformManager;

import com.google.android.filament.gltfio.AssetLoader;
import com.google.android.filament.gltfio.FilamentAsset;
import com.google.android.filament.gltfio.UbershaderProvider;

public class FilamentRenderer implements Choreographer.FrameCallback {
    private static final String TAG = "FilamentRenderer";
    
    private Context context;
    private Engine engine;
    private Renderer renderer;
    private Scene scene;
    private View view;
    private Camera camera;
    private SwapChain swapChain;
    private AssetLoader assetLoader;
    private UbershaderProvider materialProvider;
    private FilamentAsset asset;
    
    private SurfaceView surfaceView;
    private boolean isInitialized = false;
    private boolean isModelLoaded = false;
    
    public FilamentRenderer(Context context) {
        this.context = context;
        initialize();
    }
    
    private void initialize() {
        try {
            // 初始化 Filament
            engine = Engine.create();
            renderer = engine.createRenderer();
            scene = engine.createScene();
            view = engine.createView();
            
            // 建立相機
            int cameraEntity = EntityManager.get().create();
            camera = engine.createCamera(cameraEntity);
            
            // 設定相機位置和方向 - 使用 1.45.0 兼容的 API
            double aspectRatio = 1.0; // 預設比例，會在 setSurface 時更新
            camera.setProjection(45.0, aspectRatio, 0.1, 20.0, Camera.Fov.VERTICAL);
            camera.lookAt(0, 0, 4, 0, 0, 0, 0, 1, 0);
            
            view.setCamera(camera);
            view.setScene(scene);
            
            // 建立材質提供者 - 使用 UbershaderProvider
            materialProvider = new UbershaderProvider(engine);
            assetLoader = new AssetLoader(engine, materialProvider, EntityManager.get());
            
            isInitialized = true;
            Log.d(TAG, "Filament initialized successfully");
            
        } catch (Exception e) {
            Log.e(TAG, "Failed to initialize Filament", e);
        }
    }
    
    public void setSurface(Surface surface, int width, int height) {
        if (!isInitialized) return;
        
        try {
            // 建立 SwapChain
            swapChain = engine.createSwapChain(surface);
            
            // 設定 Viewport - 使用 Viewport 物件
            Viewport viewport = new Viewport(0, 0, width, height);
            view.setViewport(viewport);
            
            // 更新相機投影比例
            double aspectRatio = (double) width / height;
            camera.setProjection(45.0, aspectRatio, 0.1, 20.0, Camera.Fov.VERTICAL);
            
            // 開始渲染循環
            Choreographer.getInstance().postFrameCallback(this);
            
            Log.d(TAG, "Surface set successfully: " + width + "x" + height);
            
        } catch (Exception e) {
            Log.e(TAG, "Error setting surface", e);
        }
    }
    
    public void onSurfaceChanged(int width, int height) {
        if (view != null) {
            Viewport viewport = new Viewport(0, 0, width, height);
            view.setViewport(viewport);
            
            // 更新相機投影比例
            double aspectRatio = (double) width / height;
            camera.setProjection(45.0, aspectRatio, 0.1, 20.0, Camera.Fov.VERTICAL);
        }
    }
    
    public boolean loadGLBModel(String modelPath) {
        if (!isInitialized || assetLoader == null) {
            Log.e(TAG, "Filament not initialized");
            return false;
        }
        
        try {
            // 從 assets 載入 GLB 檔案
            android.content.res.AssetManager assetManager = context.getAssets();
            android.content.res.AssetFileDescriptor afd = assetManager.openFd(modelPath);
            
            // 讀取檔案到 ByteBuffer
            java.io.FileInputStream fis = new java.io.FileInputStream(afd.getFileDescriptor());
            fis.getChannel().position(afd.getStartOffset());
            
            byte[] data = new byte[(int) afd.getDeclaredLength()];
            fis.read(data);
            fis.close();
            afd.close();
            
            ByteBuffer buffer = ByteBuffer.allocateDirect(data.length);
            buffer.put(data);
            buffer.flip();
            
            // 使用 1.45.0 兼容的 API 載入 GLB - 修正方法名
            asset = assetLoader.createAsset(buffer);
            
            if (asset != null) {
                // 將實體添加到場景 - 修正方法名
                scene.addEntities(asset.getEntities());
                
                // 釋放資源（由 AssetLoader 管理）
                asset.releaseSourceData();
                
                isModelLoaded = true;
                Log.d(TAG, "GLB model loaded successfully");
                return true;
            } else {
                Log.e(TAG, "Failed to load GLB asset");
                return false;
            }
            
        } catch (Exception e) {
            Log.e(TAG, "Failed to load GLB model", e);
            return false;
        }
    }
    
    public void render(Surface surface) {
        if (renderer != null && swapChain != null) {
            // 開始渲染幀
            if (renderer.beginFrame(swapChain, System.nanoTime())) {
                renderer.render(view);
                renderer.endFrame();
            }
        }
    }
    
    @Override
    public void doFrame(long frameTimeNanos) {
        if (!isInitialized || swapChain == null) {
            Choreographer.getInstance().postFrameCallback(this);
            return;
        }
        
        try {
            // 開始幀渲染 - 1.45.0 兼容的 API
            if (renderer.beginFrame(swapChain, frameTimeNanos)) {
                renderer.render(view);
                renderer.endFrame();
            }
            
        } catch (Exception e) {
            Log.e(TAG, "Render error", e);
        }
        
        // 繼續下一幀
        Choreographer.getInstance().postFrameCallback(this);
    }
    
    public void cleanup() {
        if (engine != null && engine.isValid()) {
            try {
                // 銷毀 AssetLoader (正確的方式)
                if (assetLoader != null) {
                    assetLoader.destroy();
                    assetLoader = null;
                }
                
                // 銷毀其他 Filament 對象
                if (swapChain != null) {
                    engine.destroySwapChain(swapChain);
                    swapChain = null;
                }
                
                if (view != null) {
                    engine.destroyView(view);
                    view = null;
                }
                
                if (scene != null) {
                    engine.destroyScene(scene);
                    scene = null;
                }
                
                // 暫時註釋掉 camera 銷毀 - Engine.destroy() 會自動清理
                // if (camera != null) {
                //     engine.destroyCamera(camera);
                //     camera = null;
                // }
                
                if (renderer != null) {
                    engine.destroyRenderer(renderer);
                    renderer = null;
                }
                
                // 最後銷毀 Engine
                engine.destroy();
                engine = null;
                
            } catch (Exception e) {
                Log.e(TAG, "Error during cleanup", e);
            }
        }
        
        isInitialized = false;
        isModelLoaded = false;
        Log.d(TAG, "Filament cleanup completed");
    }
    
    public boolean isInitialized() {
        return isInitialized;
    }
    
    public boolean isModelLoaded() {
        return isModelLoaded;
    }
} 