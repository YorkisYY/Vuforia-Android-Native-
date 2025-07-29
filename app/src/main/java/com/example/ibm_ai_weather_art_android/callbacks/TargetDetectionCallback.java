package com.example.ibm_ai_weather_art_android.callbacks;

/**
 * 統一的目標檢測回調接口
 */
public interface TargetDetectionCallback {
    void onTargetFound(String targetName);
    void onTargetLost(String targetName);
    void onTargetTracking(String targetName, float[] modelViewMatrix);
} 