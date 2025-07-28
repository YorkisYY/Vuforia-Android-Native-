package com.example.ibm_ai_weather_art_android;

import android.content.Context;
import android.view.Surface;
import android.view.SurfaceView;

import com.google.android.filament.Engine;
import com.google.android.filament.EntityManager;
import com.google.android.filament.Renderer;
import com.google.android.filament.Scene;
import com.google.android.filament.View;
import com.google.android.filament.Viewport;
import com.google.android.filament.android.UiHelper;
import com.google.android.filament.gltfio.AssetLoader;
import com.google.android.filament.gltfio.UbershaderProvider;

public class FilamentRenderer {
    private Engine engine;
    private Renderer renderer;
    private Scene scene;
    private View view;
    private UiHelper uiHelper;
    private AssetLoader assetLoader;
    private UbershaderProvider ubershaderProvider;

    public FilamentRenderer(Context context) {
        // 初始化 Filament Engine
        engine = Engine.create();
        renderer = engine.createRenderer();
        scene = engine.createScene();
        view = engine.createView();
        view.setScene(scene);

        // 使用官方提供的 UbershaderProvider
        ubershaderProvider = new UbershaderProvider(engine);
        assetLoader = new AssetLoader(engine, ubershaderProvider, EntityManager.get());
    }

    public void setupSurface(SurfaceView surfaceView) {
        uiHelper = new UiHelper(UiHelper.ContextErrorPolicy.DONT_CHECK);
        uiHelper.setRenderCallback(new SampleRenderCallback());
        uiHelper.attachTo(surfaceView);
    }

    public void destroy() {
        // 清理資源
        if (uiHelper != null) {
            uiHelper.detach();
        }
        
        if (assetLoader != null) {
            assetLoader.destroy();
        }
        
        if (ubershaderProvider != null) {
            ubershaderProvider.destroyMaterials();
        }

        if (engine != null) {
            engine.destroyRenderer(renderer);
            engine.destroyView(view);
            engine.destroyScene(scene);
            engine.destroy();
        }
    }

    private class SampleRenderCallback implements UiHelper.RendererCallback {
        @Override
        public void onNativeWindowChanged(Surface surface) {
            // 處理 surface 變化
        }

        @Override
        public void onDetachedFromSurface() {
            // 處理從 surface 分離
        }

        @Override
        public void onResized(int width, int height) {
            // 設置視口
            view.setViewport(new Viewport(0, 0, width, height));
        }
    }

    // 獲取相關對象的方法
    public Engine getEngine() {
        return engine;
    }

    public Scene getScene() {
        return scene;
    }

    public View getView() {
        return view;
    }

    public Renderer getRenderer() {
        return renderer;
    }

    public AssetLoader getAssetLoader() {
        return assetLoader;
    }
} 