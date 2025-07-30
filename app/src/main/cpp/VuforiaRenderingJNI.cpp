// ==================== VuforiaRenderingJNI.cpp ====================
// 专门处理Vuforia渲染相关的JNI方法
// 解决 stopRenderingLoop() 编译错误以及相关渲染功能
// 集成完整的Vuforia 11.3.4渲染实现
//C:\Users\USER\Desktop\IBM-WEATHER-ART-ANDRIOD\app\src\main\cpp\VuforiaRenderingJNI.cpp

#include "VuforiaRenderingJNI.h"
#include "VuforiaWrapper.h"  // 引用主要的Wrapper类
#include <GLES3/gl3.h>       // OpenGL ES 3.0
#include <GLES2/gl2ext.h>    // OpenGL扩展

// ==================== 渲染循环控制实现 ====================

extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_stopRenderingLoopNative(
    JNIEnv* env, jobject thiz) {
    
    LOGI_RENDER("🛑 stopRenderingLoopNative called - SOLVING COMPILATION ERROR");
    
    try {
        // 调用主Wrapper实例的方法
        VuforiaWrapper::getInstance().stopRenderingLoop();
        LOGI_RENDER("✅ Rendering loop stopped successfully via dedicated JNI");
    } catch (const std::exception& e) {
        LOGE_RENDER("❌ Error in stopRenderingLoopNative: %s", e.what());
    } catch (...) {
        LOGE_RENDER("❌ Unknown error in stopRenderingLoopNative");
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_startRenderingLoopNative(
    JNIEnv* env, jobject thiz) {
    
    LOGI_RENDER("▶️ startRenderingLoopNative called");
    
    try {
        if (VuforiaWrapper::getInstance().startRenderingLoop()) {
            LOGI_RENDER("✅ Rendering loop started successfully via dedicated JNI");
        } else {
            LOGE_RENDER("❌ Failed to start rendering loop via dedicated JNI");
        }
    } catch (const std::exception& e) {
        LOGE_RENDER("❌ Error in startRenderingLoopNative: %s", e.what());
    } catch (...) {
        LOGE_RENDER("❌ Unknown error in startRenderingLoopNative");
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_isRenderingActiveNative(
    JNIEnv* env, jobject thiz) {
    
    LOGD_RENDER("📊 isRenderingActiveNative called");
    
    try {
        bool isActive = VuforiaWrapper::getInstance().isRenderingLoopActive();
        LOGD_RENDER("📊 Rendering active status: %s", isActive ? "true" : "false");
        return isActive ? JNI_TRUE : JNI_FALSE;
    } catch (const std::exception& e) {
        LOGE_RENDER("❌ Error in isRenderingActiveNative: %s", e.what());
        return JNI_FALSE;
    } catch (...) {
        LOGE_RENDER("❌ Unknown error in isRenderingActiveNative");
        return JNI_FALSE;
    }
}

// ==================== OpenGL渲染资源管理实现 ====================

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_initializeOpenGLResourcesNative(
    JNIEnv* env, jobject thiz) {
    
    LOGI_RENDER("🎨 initializeOpenGLResourcesNative called - Vuforia 11.3.4");
    
    try {
        bool success = VuforiaWrapper::getInstance().initializeOpenGLResources();
        if (success) {
            LOGI_RENDER("✅ OpenGL resources initialized successfully for rendering");
        } else {
            LOGE_RENDER("❌ Failed to initialize OpenGL resources for rendering");
        }
        return success ? JNI_TRUE : JNI_FALSE;
    } catch (const std::exception& e) {
        LOGE_RENDER("❌ Exception in initializeOpenGLResourcesNative: %s", e.what());
        return JNI_FALSE;
    } catch (...) {
        LOGE_RENDER("❌ Unknown error in initializeOpenGLResourcesNative");
        return JNI_FALSE;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_cleanupOpenGLResourcesNative(
    JNIEnv* env, jobject thiz) {
    
    LOGI_RENDER("🧹 cleanupOpenGLResourcesNative called");
    
    try {
        VuforiaWrapper::getInstance().cleanupOpenGLResources();
        LOGI_RENDER("✅ OpenGL resources cleaned up successfully");
    } catch (const std::exception& e) {
        LOGE_RENDER("❌ Error in cleanupOpenGLResourcesNative: %s", e.what());
    } catch (...) {
        LOGE_RENDER("❌ Unknown error in cleanupOpenGLResourcesNative");
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_getOpenGLInfoNative(
    JNIEnv* env, jobject thiz) {
    
    LOGD_RENDER("📋 getOpenGLInfoNative called");
    
    try {
        std::string info = VuforiaWrapper::getInstance().getOpenGLInfo();
        return env->NewStringUTF(info.c_str());
    } catch (const std::exception& e) {
        LOGE_RENDER("❌ Error in getOpenGLInfoNative: %s", e.what());
        std::string errorMsg = "Error getting OpenGL info: " + std::string(e.what());
        return env->NewStringUTF(errorMsg.c_str());
    } catch (...) {
        LOGE_RENDER("❌ Unknown error in getOpenGLInfoNative");
        return env->NewStringUTF("Unknown error getting OpenGL info");
    }
}

// ==================== 视频背景渲染核心实现 ====================

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_setupVideoBackgroundRenderingNative(
    JNIEnv* env, jobject thiz) {
    
    LOGI_RENDER("📷 setupVideoBackgroundRenderingNative called - Vuforia 11.3.4");
    
    try {
        // 设置视频背景渲染 - 这会创建必要的着色器和纹理
        bool success = VuforiaWrapper::getInstance().setupVideoBackgroundRendering();
        
        if (success) {
            LOGI_RENDER("✅ Video background rendering setup completed");
        } else {
            LOGE_RENDER("❌ Failed to setup video background rendering");
        }
        
        return success ? JNI_TRUE : JNI_FALSE;
    } catch (const std::exception& e) {
        LOGE_RENDER("❌ Exception in setupVideoBackgroundRenderingNative: %s", e.what());
        return JNI_FALSE;
    } catch (...) {
        LOGE_RENDER("❌ Unknown error in setupVideoBackgroundRenderingNative");
        return JNI_FALSE;
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_validateRenderingSetupNative(
    JNIEnv* env, jobject thiz) {
    
    LOGD_RENDER("🔍 validateRenderingSetupNative called");
    
    try {
        bool isValid = VuforiaWrapper::getInstance().validateOpenGLSetup();
        LOGD_RENDER("🔍 Rendering setup validation: %s", isValid ? "PASSED" : "FAILED");
        return isValid ? JNI_TRUE : JNI_FALSE;
    } catch (const std::exception& e) {
        LOGE_RENDER("❌ Error in validateRenderingSetupNative: %s", e.what());
        return JNI_FALSE;
    } catch (...) {
        LOGE_RENDER("❌ Unknown error in validateRenderingSetupNative");
        return JNI_FALSE;
    }
}

// ==================== 帧渲染实现 ====================

extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_renderFrameWithVideoBackgroundNative(
    JNIEnv* env, jobject thiz) {
    
    LOGD_RENDER("🎬 renderFrameWithVideoBackgroundNative called");
    
    try {
        // 调用增强版的渲染帧方法
        VuforiaWrapper::getInstance().renderFrameWithVideoBackground(env);
        
    } catch (const std::exception& e) {
        LOGE_RENDER("❌ Exception in renderFrameWithVideoBackgroundNative: %s", e.what());
    } catch (...) {
        LOGE_RENDER("❌ Unknown error in renderFrameWithVideoBackgroundNative");
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_debugRenderStateNative(
    JNIEnv* env, jobject thiz) {
    
    LOGD_RENDER("🔍 debugRenderStateNative called");
    
    try {
        VuforiaWrapper::getInstance().debugCurrentRenderState();
        LOGD_RENDER("✅ Render state debug information logged");
    } catch (const std::exception& e) {
        LOGE_RENDER("❌ Error in debugRenderStateNative: %s", e.what());
    } catch (...) {
        LOGE_RENDER("❌ Unknown error in debugRenderStateNative");
    }
}

// ==================== 相机控制实现 ====================

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_startCameraNative(
    JNIEnv* env, jobject thiz) {
    
    LOGI_RENDER("📷 startCameraNative called");
    
    try {
        // 在Vuforia 11.x中，相机与引擎生命周期绑定
        bool success = VuforiaWrapper::getInstance().start();
        
        if (success) {
            LOGI_RENDER("✅ Camera started successfully (engine started)");
        } else {
            LOGE_RENDER("❌ Failed to start camera (engine start failed)");
        }
        
        return success ? JNI_TRUE : JNI_FALSE;
    } catch (const std::exception& e) {
        LOGE_RENDER("❌ Error in startCameraNative: %s", e.what());
        return JNI_FALSE;
    } catch (...) {
        LOGE_RENDER("❌ Unknown error in startCameraNative");
        return JNI_FALSE;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_stopCameraNative(
    JNIEnv* env, jobject thiz) {
    
    LOGI_RENDER("📷 stopCameraNative called");
    
    try {
        // 暂停引擎即可停止相机
        VuforiaWrapper::getInstance().pause();
        LOGI_RENDER("✅ Camera stopped successfully (engine paused)");
    } catch (const std::exception& e) {
        LOGE_RENDER("❌ Error in stopCameraNative: %s", e.what());
    } catch (...) {
        LOGE_RENDER("❌ Unknown error in stopCameraNative");
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_isCameraActiveNative(
    JNIEnv* env, jobject thiz) {
    
    LOGD_RENDER("📊 isCameraActiveNative called");
    
    try {
        bool isActive = VuforiaWrapper::getInstance().isCameraActive();
        LOGD_RENDER("📊 Camera active status: %s", isActive ? "true" : "false");
        return isActive ? JNI_TRUE : JNI_FALSE;
    } catch (const std::exception& e) {
        LOGE_RENDER("❌ Error in isCameraActiveNative: %s", e.what());
        return JNI_FALSE;
    } catch (...) {
        LOGE_RENDER("❌ Unknown error in isCameraActiveNative");
        return JNI_FALSE;
    }
}

// ==================== Surface管理实现 ====================

extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_setSurfaceNative(
    JNIEnv* env, jobject thiz, jobject surface) {
    
    LOGI_RENDER("🖼️ setSurfaceNative called");
    
    try {
        if (surface != nullptr) {
            VuforiaWrapper::getInstance().setRenderingSurface(reinterpret_cast<void*>(surface));
            LOGI_RENDER("✅ Surface set successfully (non-null)");
        } else {
            VuforiaWrapper::getInstance().setRenderingSurface(nullptr);
            LOGW_RENDER("⚠️ Surface set to null");
        }
    } catch (const std::exception& e) {
        LOGE_RENDER("❌ Error in setSurfaceNative: %s", e.what());
    } catch (...) {
        LOGE_RENDER("❌ Unknown error in setSurfaceNative");
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_onSurfaceCreatedNative(
    JNIEnv* env, jobject thiz, jint width, jint height) {
    
    LOGI_RENDER("🖼️ onSurfaceCreatedNative called: %dx%d", width, height);
    
    try {
        // 首先处理surface创建
        VuforiaWrapper::getInstance().onSurfaceCreated(static_cast<int>(width), static_cast<int>(height));
        LOGI_RENDER("✅ Surface creation processed: %dx%d", width, height);
        
        // 然后初始化OpenGL资源（如果还没有初始化）
        if (!VuforiaWrapper::getInstance().isOpenGLInitialized()) {
            LOGI_RENDER("🎨 Auto-initializing OpenGL resources after surface creation");
            if (VuforiaWrapper::getInstance().initializeOpenGLResources()) {
                LOGI_RENDER("✅ OpenGL resources auto-initialized successfully");
            } else {
                LOGE_RENDER("❌ Failed to auto-initialize OpenGL resources");
            }
        }
        
        // 自动启动渲染循环（如果引擎已准备好）
        if (VuforiaWrapper::getInstance().isEngineRunning()) {
            LOGI_RENDER("🚀 Auto-starting rendering loop after surface creation");
            VuforiaWrapper::getInstance().startRenderingLoop();
        }
        
    } catch (const std::exception& e) {
        LOGE_RENDER("❌ Error in onSurfaceCreatedNative: %s", e.what());
    } catch (...) {
        LOGE_RENDER("❌ Unknown error in onSurfaceCreatedNative");
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_onSurfaceDestroyedNative(
    JNIEnv* env, jobject thiz) {
    
    LOGI_RENDER("🖼️ onSurfaceDestroyedNative called");
    
    try {
        // 先停止渲染循环
        VuforiaWrapper::getInstance().stopRenderingLoop();
        
        // 清理OpenGL资源
        VuforiaWrapper::getInstance().cleanupOpenGLResources();
        
        // 然后处理surface销毁
        VuforiaWrapper::getInstance().onSurfaceDestroyed();
        
        LOGI_RENDER("✅ Surface destruction processed with cleanup");
    } catch (const std::exception& e) {
        LOGE_RENDER("❌ Error in onSurfaceDestroyedNative: %s", e.what());
    } catch (...) {
        LOGE_RENDER("❌ Unknown error in onSurfaceDestroyedNative");
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_onSurfaceChangedNative(
    JNIEnv* env, jobject thiz, jint width, jint height) {
    
    LOGI_RENDER("🖼️ onSurfaceChangedNative called: %dx%d", width, height);
    
    try {
        // 更新视口设置
        glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
        
        // 通知Wrapper surface尺寸变化
        VuforiaWrapper::getInstance().onSurfaceChanged(static_cast<int>(width), static_cast<int>(height));
        
        LOGI_RENDER("✅ Surface change processed: %dx%d", width, height);
    } catch (const std::exception& e) {
        LOGE_RENDER("❌ Error in onSurfaceChangedNative: %s", e.what());
    } catch (...) {
        LOGE_RENDER("❌ Unknown error in onSurfaceChangedNative");
    }
}

// ==================== 引擎状态查询实现 ====================

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_isVuforiaEngineRunningNative(
    JNIEnv* env, jobject thiz) {
    
    LOGD_RENDER("🔍 isVuforiaEngineRunningNative called");
    
    try {
        bool isRunning = VuforiaWrapper::getInstance().isEngineRunning();
        LOGD_RENDER("🔍 Engine running status: %s", isRunning ? "true" : "false");
        return isRunning ? JNI_TRUE : JNI_FALSE;
    } catch (const std::exception& e) {
        LOGE_RENDER("❌ Error in isVuforiaEngineRunningNative: %s", e.what());
        return JNI_FALSE;
    } catch (...) {
        LOGE_RENDER("❌ Unknown error in isVuforiaEngineRunningNative");
        return JNI_FALSE;
    }
}

// ==================== 引擎生命周期控制实现 ====================

extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_pauseVuforiaEngineNative(
    JNIEnv* env, jobject thiz) {
    
    LOGI_RENDER("⏸️ pauseVuforiaEngineNative called");
    
    try {
        VuforiaWrapper::getInstance().pause();
        LOGI_RENDER("✅ Vuforia engine paused successfully");
    } catch (const std::exception& e) {
        LOGE_RENDER("❌ Error in pauseVuforiaEngineNative: %s", e.what());
    } catch (...) {
        LOGE_RENDER("❌ Unknown error in pauseVuforiaEngineNative");
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_resumeVuforiaEngineNative(
    JNIEnv* env, jobject thiz) {
    
    LOGI_RENDER("▶️ resumeVuforiaEngineNative called");
    
    try {
        VuforiaWrapper::getInstance().resume();
        LOGI_RENDER("✅ Vuforia engine resumed successfully");
    } catch (const std::exception& e) {
        LOGE_RENDER("❌ Error in resumeVuforiaEngineNative: %s", e.what());
    } catch (...) {
        LOGE_RENDER("❌ Unknown error in resumeVuforiaEngineNative");
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_startVuforiaEngineNative(
    JNIEnv* env, jobject thiz) {
    
    LOGI_RENDER("🚀 startVuforiaEngineNative called");
    
    try {
        bool success = VuforiaWrapper::getInstance().start();
        if (success) {
            LOGI_RENDER("✅ Vuforia engine started successfully");
        } else {
            LOGE_RENDER("❌ Failed to start Vuforia engine");
        }
        return success ? JNI_TRUE : JNI_FALSE;
    } catch (const std::exception& e) {
        LOGE_RENDER("❌ Error in startVuforiaEngineNative: %s", e.what());
        return JNI_FALSE;
    } catch (...) {
        LOGE_RENDER("❌ Unknown error in startVuforiaEngineNative");
        return JNI_FALSE;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_stopVuforiaEngineNative(
    JNIEnv* env, jobject thiz) {
    
    LOGI_RENDER("🛑 stopVuforiaEngineNative called");
    
    try {
        VuforiaWrapper::getInstance().stop();
        LOGI_RENDER("✅ Vuforia engine stopped successfully");
    } catch (const std::exception& e) {
        LOGE_RENDER("❌ Error in stopVuforiaEngineNative: %s", e.what());
    } catch (...) {
        LOGE_RENDER("❌ Unknown error in stopVuforiaEngineNative");
    }
}

// ==================== 诊断和调试实现 ====================

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_getEngineStatusDetailNative(
    JNIEnv* env, jobject thiz) {
    
    LOGD_RENDER("📋 getEngineStatusDetailNative called");
    
    try {
        std::string statusDetail = VuforiaWrapper::getInstance().getEngineStatusDetail();
        return env->NewStringUTF(statusDetail.c_str());
    } catch (const std::exception& e) {
        LOGE_RENDER("❌ Error in getEngineStatusDetailNative: %s", e.what());
        std::string errorMsg = "Error getting engine status: " + std::string(e.what());
        return env->NewStringUTF(errorMsg.c_str());
    } catch (...) {
        LOGE_RENDER("❌ Unknown error in getEngineStatusDetailNative");
        return env->NewStringUTF("Unknown error getting engine status");
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_getMemoryUsageNative(
    JNIEnv* env, jobject thiz) {
    
    LOGD_RENDER("🧠 getMemoryUsageNative called");
    
    try {
        std::string memoryInfo = VuforiaWrapper::getInstance().getMemoryUsageInfo();
        return env->NewStringUTF(memoryInfo.c_str());
    } catch (const std::exception& e) {
        LOGE_RENDER("❌ Error in getMemoryUsageNative: %s", e.what());
        std::string errorMsg = "Error getting memory usage: " + std::string(e.what());
        return env->NewStringUTF(errorMsg.c_str());
    } catch (...) {
        LOGE_RENDER("❌ Unknown error in getMemoryUsageNative");
        return env->NewStringUTF("Unknown error getting memory usage");
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_getRenderingStatusNative(
    JNIEnv* env, jobject thiz) {
    
    LOGD_RENDER("🖼️ getRenderingStatusNative called");
    
    try {
        std::string renderingStatus = VuforiaWrapper::getInstance().getRenderingStatusDetail();
        return env->NewStringUTF(renderingStatus.c_str());
    } catch (const std::exception& e) {
        LOGE_RENDER("❌ Error in getRenderingStatusNative: %s", e.what());
        std::string errorMsg = "Error getting rendering status: " + std::string(e.what());
        return env->NewStringUTF(errorMsg.c_str());
    } catch (...) {
        LOGE_RENDER("❌ Unknown error in getRenderingStatusNative");
        return env->NewStringUTF("Unknown error getting rendering status");
    }
}

// ==================== 安全的图像追踪控制实现 ====================

extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_stopImageTrackingNativeSafe(
    JNIEnv* env, jobject thiz) {
    
    LOGI_RENDER("🎯 stopImageTrackingNativeSafe called (Safe version)");
    
    try {
        VuforiaWrapper::getInstance().stopImageTrackingSafe();
        LOGI_RENDER("✅ Image tracking stopped safely via dedicated JNI");
    } catch (const std::exception& e) {
        LOGE_RENDER("❌ Error in stopImageTrackingNativeSafe: %s", e.what());
    } catch (...) {
        LOGE_RENDER("❌ Unknown error in stopImageTrackingNativeSafe");
    }
}

// ==================== 渲染性能监控实现 ====================

extern "C" JNIEXPORT jfloat JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_getCurrentFPSNative(
    JNIEnv* env, jobject thiz) {
    
    LOGD_RENDER("📊 getCurrentFPSNative called");
    
    try {
        float fps = VuforiaWrapper::getInstance().getCurrentRenderingFPS();
        LOGD_RENDER("📊 Current FPS: %.2f", fps);
        return fps;
    } catch (const std::exception& e) {
        LOGE_RENDER("❌ Error in getCurrentFPSNative: %s", e.what());
        return 0.0f;
    } catch (...) {
        LOGE_RENDER("❌ Unknown error in getCurrentFPSNative");
        return 0.0f;
    }
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_getFrameCountNative(
    JNIEnv* env, jobject thiz) {
    
    LOGD_RENDER("📊 getFrameCountNative called");
    
    try {
        long frameCount = VuforiaWrapper::getInstance().getTotalFrameCount();
        LOGD_RENDER("📊 Total frame count: %ld", frameCount);
        return static_cast<jlong>(frameCount);
    } catch (const std::exception& e) {
        LOGE_RENDER("❌ Error in getFrameCountNative: %s", e.what());
        return 0L;
    } catch (...) {
        LOGE_RENDER("❌ Unknown error in getFrameCountNative");
        return 0L;
    }
}

// ==================== 高级渲染配置实现 ====================

extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_setVideoBackgroundRenderingEnabledNative(
    JNIEnv* env, jobject thiz, jboolean enabled) {
    
    LOGI_RENDER("📷 setVideoBackgroundRenderingEnabledNative called: %s", enabled ? "enabled" : "disabled");
    
    try {
        VuforiaWrapper::getInstance().setVideoBackgroundRenderingEnabled(enabled == JNI_TRUE);
        LOGI_RENDER("✅ Video background rendering %s", enabled ? "enabled" : "disabled");
    } catch (const std::exception& e) {
        LOGE_RENDER("❌ Error in setVideoBackgroundRenderingEnabledNative: %s", e.what());
    } catch (...) {
        LOGE_RENDER("❌ Unknown error in setVideoBackgroundRenderingEnabledNative");
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_setRenderingQualityNative(
    JNIEnv* env, jobject thiz, jint quality) {
    
    LOGI_RENDER("🎨 setRenderingQualityNative called: quality=%d", quality);
    
    try {
        VuforiaWrapper::getInstance().setRenderingQuality(static_cast<int>(quality));
        LOGI_RENDER("✅ Rendering quality set to: %d", quality);
    } catch (const std::exception& e) {
        LOGE_RENDER("❌ Error in setRenderingQualityNative: %s", e.what());
    } catch (...) {
        LOGE_RENDER("❌ Unknown error in setRenderingQualityNative");
    }
}

// ==================== 模块信息和使用说明 ====================
/*
 * 🎯 VuforiaRenderingJNI.cpp 完整渲染模块说明：
 * 
 * === 核心功能 ===
 * 1. 渲染循环控制：启动/停止/状态查询
 * 2. OpenGL资源管理：初始化/清理/验证
 * 3. 视频背景渲染：完整的Vuforia 11.3.4实现
 * 4. 相机控制：启动/停止/状态监控
 * 5. Surface管理：创建/销毁/变化处理
 * 6. 性能监控：FPS监控/帧计数
 * 7. 调试工具：状态报告/内存使用/渲染诊断
 * 
 * === 新增渲染方法 ===
 * - initializeOpenGLResourcesNative(): 初始化OpenGL渲染资源
 * - setupVideoBackgroundRenderingNative(): 设置视频背景渲染
 * - renderFrameWithVideoBackgroundNative(): 渲染带视频背景的帧
 * - validateRenderingSetupNative(): 验证渲染设置
 * - debugRenderStateNative(): 调试渲染状态
 * - getCurrentFPSNative(): 获取当前FPS
 * - setVideoBackgroundRenderingEnabledNative(): 启用/禁用视频背景
 * 
 * === Android使用方式 ===
 * 1. 在GLSurfaceView.Renderer的onSurfaceCreated中调用initializeOpenGLResourcesNative()
 * 2. 在onDrawFrame中调用renderFrameWithVideoBackgroundNative()
 * 3. 在onSurfaceDestroyed中调用cleanupOpenGLResourcesNative()
 * 4. 使用各种状态查询方法进行调试和监控
 * 
 * === 编译要求 ===
 * - 需要在CMakeLists.txt中包含此文件
 * - 需要链接OpenGL ES 3.0库
 * - 需要VuforiaWrapper.h中实现对应的方法
 * 
 * === 故障排除 ===
 * - 使用getRenderingStatusNative()查看详细状态
 * - 使用debugRenderStateNative()调试渲染问题
 * - 使用validateRenderingSetupNative()验证设置
 */