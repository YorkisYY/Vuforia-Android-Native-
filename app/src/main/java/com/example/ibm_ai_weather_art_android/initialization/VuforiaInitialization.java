package com.example.ibm_ai_weather_art_android.initialization;

import android.app.Activity;
import android.content.Context;
import android.os.AsyncTask;
import android.util.Log;

/**
 * Vuforia 初始化管理類
 * 基於 Vuforia Engine 10+ 純 Native C API 模式
 * 移除了所有舊版 Java API 調用
 */
public class VuforiaInitialization {
    
    private static final String TAG = "VuforiaInitialization";
    
    // Vuforia 許可證密鑰 - 需要從 Vuforia 開發者門戶獲取
    private static final String VUFORIA_LICENSE_KEY = // 在 Java 代碼中使用你的真正 License Key
    "AddD0sD/////AAABmb2xv80J2UAshKy68I6M8/chOh4Bd0UsKQeqMnCZenkh8Z9mPEun8HUhBzpsnjGETKQBX0Duvgp/m3k9GYnZks41tcRtaGnjXvwRW/t3zXQH1hAulR/AbMsXnoxHWBtHIE3YzDLnk5/MO30VXre2sz8ZBKtJCKsw4lA8UH1fwzO07aWsLkyGxBqDynU4sq509TAxqB2IdoGsW6kHpl6hz5RA8PzIE5UmUBIdM3/xjAAw/HJ9LJrP+i4KmkRXWHpYLD0bJhq66b78JfigD/zcm+bGK2TS1Klo6/6xkhHYCsd7LOcPmO0scdNVdNBrGugBgDps2n3YFyciQbFPYrGk4rW7u8EPlpABJIDbr0dVTv3W";
    
    private InitializationCallback mCallback;
    private Context mContext;
    private static boolean libraryLoaded = false;
    
    public interface InitializationCallback {
        void onInitializationComplete(boolean success, String errorMessage);
        void onInitializationProgress(int progressValue);
    }
    
    public VuforiaInitialization(InitializationCallback callback) {
        this.mCallback = callback;
    }
    
    // ==================== Native 方法聲明 ====================
    // 所有 Vuforia 操作現在都通過 JNI 調用 Native C++ 層
    private native boolean initVuforiaEngineNative(String licenseKey);
    private native void deinitVuforiaEngineNative();
    private native void pauseVuforiaEngineNative();
    private native void resumeVuforiaEngineNative();
    private native boolean isVuforiaInitializedNative();
    private native String getVuforiaVersionNative();
    private native void setAndroidContextNative(Object context);
    
    /**
     * 開始 Vuforia 初始化
     * @param context Android 上下文
     */
    public void startInitialization(Context context) {
        Log.d(TAG, "Starting Vuforia initialization...");
        this.mContext = context;
        
        // 加載 Native 庫
        if (!loadNativeLibrary()) {
            if (mCallback != null) {
                mCallback.onInitializationComplete(false, "Failed to load native library");
            }
            return;
        }
        
        // 使用 AsyncTask 進行異步初始化，避免阻塞 UI 線程
        new InitVuforiaTask().execute();
    }
    
    /**
     * 加載 Native 庫
     */
    private boolean loadNativeLibrary() {
        try {
            if (!libraryLoaded) {
                // 加載你的 Native 庫（包含 Vuforia 包裝層）
                System.loadLibrary("vuforia_wrapper");
                libraryLoaded = true;
                Log.d(TAG, "Native library loaded successfully");
                return true;
            }
            return true;
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "Failed to load native library: " + e.getMessage(), e);
            return false;
        }
    }
    
    /**
     * 異步初始化 Vuforia 的 AsyncTask
     */
    private class InitVuforiaTask extends AsyncTask<Void, Integer, Boolean> {
        
        private String mErrorMessage = "";
        
        @Override
        protected Boolean doInBackground(Void... params) {
            Log.d(TAG, "Initializing Vuforia in background thread...");
            
            try {
                // 設置初始化進度
                publishProgress(10);
                
                // 設置 Android 上下文到 Native 層
                if (mContext != null) {
                    setAndroidContextNative(mContext);
                    publishProgress(25);
                }
                
                // 檢查許可證密鑰
                if (VUFORIA_LICENSE_KEY.equals("YOUR_VUFORIA_LICENSE_KEY_HERE")) {
                    Log.w(TAG, "Warning: Using placeholder license key. Please set your actual Vuforia license key.");
                    // 在開發階段可以繼續，但要提醒用戶
                }
                
                publishProgress(40);
                
                // 調用 Native 層初始化 Vuforia Engine
                boolean initResult = initVuforiaEngineNative(VUFORIA_LICENSE_KEY);
                
                publishProgress(70);
                
                if (initResult) {
                    Log.d(TAG, "Vuforia Engine initialized successfully");
                    
                    // 獲取版本信息
                    try {
                        String version = getVuforiaVersionNative();
                        Log.d(TAG, "Vuforia Engine version: " + version);
                    } catch (Exception e) {
                        Log.w(TAG, "Could not get Vuforia version", e);
                    }
                    
                    publishProgress(100);
                    return true;
                    
                } else {
                    mErrorMessage = "Failed to initialize Vuforia Engine";
                    Log.e(TAG, mErrorMessage);
                    return false;
                }
                
            } catch (Exception e) {
                mErrorMessage = "Exception during Vuforia initialization: " + e.getMessage();
                Log.e(TAG, mErrorMessage, e);
                return false;
            }
        }
        
        @Override
        protected void onProgressUpdate(Integer... progress) {
            if (mCallback != null) {
                mCallback.onInitializationProgress(progress[0]);
            }
        }
        
        @Override
        protected void onPostExecute(Boolean success) {
            Log.d(TAG, "Vuforia initialization completed. Success: " + success);
            
            if (mCallback != null) {
                mCallback.onInitializationComplete(success, mErrorMessage);
            }
        }
    }
    
    /**
     * 停止並清理 Vuforia
     */
    public void deinitialize() {
        Log.d(TAG, "Deinitializing Vuforia...");
        try {
            deinitVuforiaEngineNative();
            Log.d(TAG, "Vuforia deinitialized successfully");
        } catch (Exception e) {
            Log.e(TAG, "Error during Vuforia deinitialization", e);
        }
    }
    
    /**
     * 暫停 Vuforia
     */
    public void pauseVuforia() {
        Log.d(TAG, "Pausing Vuforia...");
        try {
            pauseVuforiaEngineNative();
        } catch (Exception e) {
            Log.e(TAG, "Error pausing Vuforia", e);
        }
    }
    
    /**
     * 恢復 Vuforia
     */
    public void resumeVuforia() {
        Log.d(TAG, "Resuming Vuforia...");
        try {
            resumeVuforiaEngineNative();
        } catch (Exception e) {
            Log.e(TAG, "Error resuming Vuforia", e);
        }
    }
    
    /**
     * 檢查 Vuforia 是否已初始化
     */
    public boolean isInitialized() {
        try {
            return isVuforiaInitializedNative();
        } catch (Exception e) {
            Log.e(TAG, "Error checking Vuforia initialization status", e);
            return false;
        }
    }
    
    /**
     * 獲取 Vuforia 版本信息
     */
    public String getVuforiaVersion() {
        try {
            return getVuforiaVersionNative();
        } catch (Exception e) {
            Log.e(TAG, "Error getting Vuforia version", e);
            return "Unknown";
        }
    }
}