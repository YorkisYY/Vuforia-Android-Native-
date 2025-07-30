package com.example.ibm_ai_weather_art_android;

import android.content.Context;
import android.view.Surface;
import android.view.SurfaceView;
import android.util.Log;
import android.os.Handler;
import android.os.Looper;

import com.google.android.filament.Engine;
import com.google.android.filament.EntityManager;
import com.google.android.filament.Renderer;
import com.google.android.filament.Scene;
import com.google.android.filament.View;
import com.google.android.filament.Viewport;
import com.google.android.filament.SwapChain;
import com.google.android.filament.Camera;
import com.google.android.filament.android.UiHelper;
import com.google.android.filament.gltfio.AssetLoader;
import com.google.android.filament.gltfio.UbershaderProvider;

public class FilamentRenderer {
    private static final String TAG = "FilamentRenderer";
    
    private Engine engine;
    private Renderer renderer;
    private Scene scene;
    private View view;
    private Camera camera;
    private SwapChain swapChain;
    private UiHelper uiHelper;
    private AssetLoader assetLoader;
    private UbershaderProvider ubershaderProvider;
    
    // 渲染控制
    private boolean isRendering = false;
    private Handler renderHandler;
    private Context context;

    public FilamentRenderer(Context context) {
        this.context = context;
        Log.d(TAG, "Initializing Filament Renderer...");
        
        try {
            // 初始化 Filament Engine
            engine = Engine.create();
            renderer = engine.createRenderer();
            scene = engine.createScene();
            view = engine.createView();
            
            // 創建相機
            camera = engine.createCamera(EntityManager.get().create());
            view.setCamera(camera);
            view.setScene(scene);

            // 使用官方提供的 UbershaderProvider
            ubershaderProvider = new UbershaderProvider(engine);
            assetLoader = new AssetLoader(engine, ubershaderProvider, EntityManager.get());
            
            Log.d(TAG, "✅ Filament Engine initialized successfully");
        } catch (Exception e) {
            Log.e(TAG, "❌ Failed to initialize Filament Engine", e);
        }
    }

    public void setupSurface(SurfaceView surfaceView) {
        Log.d(TAG, "Setting up Filament surface...");
        
        try {
            uiHelper = new UiHelper(UiHelper.ContextErrorPolicy.DONT_CHECK);
            uiHelper.setRenderCallback(new FilamentRenderCallback());
            uiHelper.attachTo(surfaceView);
            
            Log.d(TAG, "✅ Filament surface setup completed");
        } catch (Exception e) {
            Log.e(TAG, "❌ Failed to setup Filament surface", e);
        }
    }

    /**
     * ⭐ 關鍵方法：啟動渲染循環
     */
    public void startRendering() {
        if (isRendering) {
            Log.w(TAG, "Rendering already started");
            return;
        }
        
        Log.d(TAG, "Starting Filament rendering loop...");
        isRendering = true;
        renderHandler = new Handler(Looper.getMainLooper());
        
        Runnable renderRunnable = new Runnable() {
            @Override
            public void run() {
                if (isRendering && renderer != null && view != null) {
                    try {
                        // 檢查是否準備
                        
                        // 60fps (16ms間隔)
                        renderHandler.postDelayed(this, 16);
                    } catch (Exception e) {
                        Log.e(TAG, "Rendering error: " + e.getMessage());
                        // 出錯時稍微延遲重試
                        renderHandler.postDelayed(this, 100);
                    }
                }
            }
        };
        
        renderHandler.post(renderRunnable);
        Log.d(TAG, "✅ Filament rendering loop started (60fps)");
    }
    
    /**
     * 停止渲染循環
     */
    public void stopRendering() {
        Log.d(TAG, "Stopping Filament rendering...");
        isRendering = false;
        
        if (renderHandler != null) {
            renderHandler.removeCallbacksAndMessages(null);
            renderHandler = null;
        }
        
        Log.d(TAG, "⏹️ Filament rendering stopped");
    }
    
    /**
     * 檢查是否準備好渲染
     */
    public boolean isReadyToRender() {
        return uiHelper != null && uiHelper.isReadyToRender() && swapChain != null;
    }
    
    /**
     * 檢查是否正在渲染
     */
    public boolean isRendering() {
        return isRendering;
    }

    /**
     * ⭐ 用於接收 Vuforia 相機數據的方法
     */
    public void updateCameraTexture(byte[] cameraData, int width, int height) {
        // 這個方法用於接收 Vuforia 的相機數據並作為背景紋理
        // 具體實現需要創建紋理並應用到背景材質上
        Log.d(TAG, "📷 Received camera data: " + width + "x" + height);
    }
    
    /**
     * 設置視頻背景
     */
    public void setupVideoBackground() {
        // 設置相機背景的方法
        Log.d(TAG, "🎬 Setting up video background");
    }

    /**
     * Filament 渲染回調
     */
    private class FilamentRenderCallback implements UiHelper.RendererCallback {
        @Override
        public void onNativeWindowChanged(Surface surface) {
            Log.d(TAG, "📱 Native window changed");
            
            if (surface != null) {
                // 創建或重新創建 SwapChain
                if (swapChain != null) {
                    engine.destroySwapChain(swapChain);
                }
                swapChain = engine.createSwapChain(surface);
                Log.d(TAG, "✅ SwapChain created for surface");
            } else {
                // 銷毀 SwapChain
                if (swapChain != null) {
                    engine.destroySwapChain(swapChain);
                    swapChain = null;
                }
                Log.d(TAG, "🗑️ SwapChain destroyed");
            }
        }

        @Override
        public void onDetachedFromSurface() {
            Log.d(TAG, "📱 Detached from surface");
            
            // 停止渲染
            stopRendering();
            
            // 清理 SwapChain
            if (swapChain != null) {
                engine.destroySwapChain(swapChain);
                swapChain = null;
            }
        }

        @Override
        public void onResized(int width, int height) {
            Log.d(TAG, "📐 Surface resized: " + width + "x" + height);
            
            // 設置視口
            if (view != null) {
                view.setViewport(new Viewport(0, 0, width, height));
            }
            
            // 設置相機投影
            if (camera != null) {
                double aspect = (double) width / (double) height;
                camera.setProjection(45.0, aspect, 0.1, 1000.0, Camera.Fov.VERTICAL);
            }
        }
    }

    /**
     * 暫停渲染（用於 Activity onPause）
     */
    public void pause() {
        Log.d(TAG, "⏸️ Pausing Filament renderer");
        stopRendering();
    }
    
    /**
     * 恢復渲染（用於 Activity onResume）
     */
    public void resume() {
        Log.d(TAG, "▶️ Resuming Filament renderer");
        if (isReadyToRender()) {
            startRendering();
        }
    }

    /**
     * 清理所有資源
     */
    public void destroy() {
        Log.d(TAG, "🗑️ Destroying Filament renderer...");
        
        // 停止渲染
        stopRendering();
        
        // 分離 UiHelper
        if (uiHelper != null) {
            uiHelper.detach();
            uiHelper = null;
        }
        
        // 銷毀 SwapChain
        if (swapChain != null && engine != null) {
            engine.destroySwapChain(swapChain);
            swapChain = null;
        }
        
        // 清理 AssetLoader
        if (assetLoader != null) {
            assetLoader.destroy();
            assetLoader = null;
        }
        
        // 清理 UbershaderProvider
        if (ubershaderProvider != null) {
            ubershaderProvider.destroyMaterials();
            ubershaderProvider = null;
        }

        // 銷毀 Filament 組件
        if (engine != null) {
            if (camera != null) {
                // ⭐ Filament 1.31 正確的相機銷毀方式
                int cameraEntity = camera.getEntity();
                engine.destroyCameraComponent(cameraEntity);
            }
            
            if (renderer != null) {
                engine.destroyRenderer(renderer);
                renderer = null;
            }
            
            if (view != null) {
                engine.destroyView(view);
                view = null;
            }
            
            if (scene != null) {
                engine.destroyScene(scene);
                scene = null;
            }
            
            // 最後銷毀引擎
            engine.destroy();
            engine = null;
        }
        
        Log.d(TAG, "✅ Filament renderer destroyed completely");
    }

    // ==================== Getter 方法 ====================
    
    public Engine getEngine() {
        return engine;
    }

    public Scene getScene() {
        return scene;
    }

    public View getView() {
        return view;
    }

    public Camera getCamera() {
        return camera;
    }

    public Renderer getRenderer() {
        return renderer;
    }

    public AssetLoader getAssetLoader() {
        return assetLoader;
    }
    
    public SwapChain getSwapChain() {
        return swapChain;
    }
}