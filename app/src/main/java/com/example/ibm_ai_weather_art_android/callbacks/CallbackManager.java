package com.example.ibm_ai_weather_art_android.callbacks;

import android.content.Context;
import android.util.Log;

public class CallbackManager {
    private static final String TAG = "CallbackManager";
    
    private Context context;
    private boolean vuforiaReady = false;
    
    public CallbackManager(Context context) {
        this.context = context;
    }
    
    // 回调接口
    public interface InitializationCallback {
        void onVuforiaInitialized(boolean success);
    }
    
    public interface ModelLoadingCallback {
        void onModelLoaded(boolean success);
    }
    
    private InitializationCallback initializationCallback;
    private ModelLoadingCallback modelLoadingCallback;
    
    public void setInitializationCallback(InitializationCallback callback) {
        this.initializationCallback = callback;
    }
    
    public void setModelLoadingCallback(ModelLoadingCallback callback) {
        this.modelLoadingCallback = callback;
    }
    
    public void notifyVuforiaInitialized(boolean success) {
        vuforiaReady = success;
        if (initializationCallback != null) {
            initializationCallback.onVuforiaInitialized(success);
        }
    }
    
    public void notifyModelLoaded(boolean success) {
        if (modelLoadingCallback != null) {
            modelLoadingCallback.onModelLoaded(success);
        }
    }
    
    public boolean isVuforiaReady() {
        return vuforiaReady;
    }
    
    private void runOnUiThread(Runnable runnable) {
        try {
            if (context instanceof android.app.Activity) {
                ((android.app.Activity) context).runOnUiThread(runnable);
            }
        } catch (Exception e) {
            Log.e(TAG, "Error running on UI thread", e);
        }
    }
} 