package com.example.ibm_ai_weather_art_android.model;

import android.content.Context;
import android.net.Uri;
import com.example.ibm_ai_weather_art_android.VuforiaManager;

/**
 * Vuforia Model Renderer
 * Converts GLB files to renderable Vuforia objects
 */
public class ModelRenderer {
    private Context context;
    private VuforiaManager vuforiaManager;
    private RenderCallback callback;

    public ModelRenderer(Context context, VuforiaManager vuforiaManager, RenderCallback callback) {
        this.context = context;
        this.vuforiaManager = vuforiaManager;
        this.callback = callback;
    }

    /**
     * Load and render GLB model with Vuforia
     */
    public void loadAndRenderGLB(String fileName) {
        callback.onRenderStarted(fileName);
        
        // First check if file exists
        GLBReader glbReader = new GLBReader(context);
        if (!glbReader.checkGLBExists(fileName)) {
            callback.onRenderFailed(fileName, "GLB file does not exist: " + fileName);
            return;
        }

        // Get file URI
        Uri modelUri = glbReader.getGLBUri(fileName);
        
        // Load model with Vuforia - 使用正確的方法名
        if (vuforiaManager.loadGiraffeModel()) {
            callback.onRenderSuccess(fileName, fileName);
        } else {
            callback.onRenderFailed(fileName, "Failed to load model with Vuforia");
        }
    }

    /**
     * Load multiple models in batch
     */
    public void loadMultipleGLB(String[] fileNames) {
        callback.onBatchRenderStarted(fileNames.length);
        
        for (int i = 0; i < fileNames.length; i++) {
            final int index = i;
            final String fileName = fileNames[i];
            
            loadAndRenderGLB(fileName);
            
            // Update progress after each model
            callback.onBatchRenderProgress(index + 1, fileNames.length);
        }
        
        callback.onBatchRenderCompleted();
    }

    /**
     * Render callback interface
     */
    public interface RenderCallback {
        void onRenderStarted(String fileName);
        void onRenderSuccess(String fileName, String modelPath);
        void onRenderFailed(String fileName, String error);
        void onBatchRenderStarted(int totalCount);
        void onBatchRenderProgress(int completed, int total);
        void onBatchRenderCompleted();
    }
} 