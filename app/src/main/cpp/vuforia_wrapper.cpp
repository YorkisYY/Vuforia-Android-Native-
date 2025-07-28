#include <jni.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <string>
#include <vector>
#include <memory>
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <ctime>
#include <cstdlib>

// 模拟 Vuforia 10 API
#define VU_SUCCESS 0
#define VU_FAILURE -1

// 模拟 Vuforia 10 结构体
typedef struct {
    float data[2];
} VuVec2F;

typedef struct {
    int data[2];
} VuVec2I;

typedef struct {
    VuVec2I pos;
    VuVec2I size;
} VuViewport;

typedef struct {
    VuVec2I resolution;
} VuRenderViewConfig;

typedef struct {
    void* vbMesh;
    VuViewport vbViewport;
} VuRenderState;

typedef struct {
    float data[16];
} VuMatrix44F;

typedef struct {
    VuMatrix44F pose;
    int poseStatus;
} VuPoseInfo;

typedef struct {
    char name[64];
} VuImageTargetObservationTargetInfo;

// 前向声明
void renderVideoBackground(const VuRenderState& renderState);
void renderSimpleBackground();

// 模拟 Vuforia 10 类
class VuRenderController {
public:
    int setRenderViewConfig(const VuRenderViewConfig* config) { 
        if (config) {
            return VU_SUCCESS; 
        }
        return VU_FAILURE;
    }
};

class VuState {
public:
    int getRenderState(VuRenderState* renderState) {
        if (renderState) {
            renderState->vbMesh = nullptr;
            renderState->vbViewport.pos.data[0] = 0;
            renderState->vbViewport.pos.data[1] = 0;
            renderState->vbViewport.size.data[0] = 1280;
            renderState->vbViewport.size.data[1] = 720;
            return VU_SUCCESS;
        }
        return VU_FAILURE;
    }
    
    void release() { 
        delete this; 
    }
};

class VuEngine {
public:
    static VuEngine* create() { 
        return new VuEngine(); 
    }
    
    void destroy() { 
        delete this; 
    }
    
    int start() { 
        return VU_SUCCESS; 
    }
    
    int stop() { 
        return VU_SUCCESS; 
    }
    
    int acquireLatestState(VuState** state) {
        if (state) {
            *state = new VuState(); 
            return VU_SUCCESS;
        }
        return VU_FAILURE;
    }
    
    VuRenderController* getRenderController() { 
        return &renderController; 
    }
    
private:
    VuRenderController renderController;
};

// 默认配置函数
VuRenderViewConfig vuRenderViewConfigDefault() {
    VuRenderViewConfig config;
    config.resolution.data[0] = 1280;
    config.resolution.data[1] = 720;
    return config;
}

#define LOG_TAG "VuforiaWrapper"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

// 全局变量
static VuEngine* gEngine = nullptr;
static VuRenderController* gRenderController = nullptr;
static AAssetManager* gAssetManager = nullptr;
static bool gVuforiaInitialized = false;
static bool gModelLoaded = false;
static bool gARRendering = false;
static bool gTargetDetectionActive = false;
static bool gCameraStarted = false;
static std::vector<std::string> gTargets;
static JavaVM* gJVM = nullptr;
static jobject gTargetCallback = nullptr;

// 简化的目标检测实现
class SimpleTargetDetector {
private:
    bool mIsActive;
    time_t mLastDetectionTime;
    int mDetectionCounter;
    
public:
    SimpleTargetDetector() : mIsActive(false), mLastDetectionTime(0), mDetectionCounter(0) {}
    
    bool startDetection() {
        LOGI("Starting simple target detection");
        mIsActive = true;
        mDetectionCounter = 0;
        mLastDetectionTime = time(nullptr);
        return true;
    }
    
    void stopDetection() {
        LOGI("Stopping simple target detection");
        mIsActive = false;
    }
    
    bool updateDetection() {
        if (!mIsActive) return false;
        
        // 模拟目标检测 - 每5秒检测一次目标
        time_t currentTime = time(nullptr);
        if (currentTime - mLastDetectionTime >= 5) {
            mLastDetectionTime = currentTime;
            mDetectionCounter++;
            
            // 模拟检测到目标
            if (mDetectionCounter % 2 == 1) {
                LOGI("🎯 Target found: stones");
                notifyTargetFound("stones");
                return true;
            } else {
                LOGI("❌ Target lost: stones");
                notifyTargetLost("stones");
                return false;
            }
        }
        
        return false;
    }
    
private:
    void notifyTargetFound(const std::string& targetName) {
        if (gJVM != nullptr && gTargetCallback != nullptr) {
            JNIEnv* env = nullptr;
            if (gJVM->AttachCurrentThread(&env, nullptr) == JNI_OK && env != nullptr) {
                jclass callbackClass = env->GetObjectClass(gTargetCallback);
                if (callbackClass) {
                    jmethodID methodId = env->GetMethodID(callbackClass, "onTargetFound", "(Ljava/lang/String;)V");
                    if (methodId) {
                        jstring jTargetName = env->NewStringUTF(targetName.c_str());
                        if (jTargetName) {
                            env->CallVoidMethod(gTargetCallback, methodId, jTargetName);
                            env->DeleteLocalRef(jTargetName);
                        }
                    }
                    env->DeleteLocalRef(callbackClass);
                }
            }
        }
    }
    
    void notifyTargetLost(const std::string& targetName) {
        if (gJVM != nullptr && gTargetCallback != nullptr) {
            JNIEnv* env = nullptr;
            if (gJVM->AttachCurrentThread(&env, nullptr) == JNI_OK && env != nullptr) {
                jclass callbackClass = env->GetObjectClass(gTargetCallback);
                if (callbackClass) {
                    jmethodID methodId = env->GetMethodID(callbackClass, "onTargetLost", "(Ljava/lang/String;)V");
                    if (methodId) {
                        jstring jTargetName = env->NewStringUTF(targetName.c_str());
                        if (jTargetName) {
                            env->CallVoidMethod(gTargetCallback, methodId, jTargetName);
                            env->DeleteLocalRef(jTargetName);
                        }
                    }
                    env->DeleteLocalRef(callbackClass);
                }
            }
        }
    }
};

static std::unique_ptr<SimpleTargetDetector> gTargetDetector;

// ===== JNI 方法实现 =====

extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaManager_setAssetManager(
    JNIEnv* env, jobject thiz, jobject asset_manager) {
    
    if (asset_manager) {
        gAssetManager = AAssetManager_fromJava(env, asset_manager);
        LOGI("Asset manager set successfully");
    } else {
        LOGE("Asset manager is null");
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaManager_setLicenseKey(
    JNIEnv* env, jobject thiz, jstring license_key) {
    
    if (license_key) {
        const char* key = env->GetStringUTFChars(license_key, nullptr);
        if (key) {
            LOGI("License key set: %.20s...", key); // 只显示前20个字符
            env->ReleaseStringUTFChars(license_key, key);
        }
    } else {
        LOGE("License key is null");
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaManager_initVuforia(
    JNIEnv* env, jobject thiz) {
    LOGI("Initializing Vuforia...");
    gVuforiaInitialized = true;
    LOGI("Vuforia initialized successfully");
    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaManager_initializeVuforiaNative(
    JNIEnv* env, jobject instance) {
    
    LOGI("Initializing Vuforia 10 engine...");
    
    try {
        // 1. 创建引擎
        gEngine = VuEngine::create();
        if (gEngine == nullptr) {
            LOGE("Failed to create Vuforia engine");
            return JNI_FALSE;
        }
        
        // 2. 获取渲染控制器
        gRenderController = gEngine->getRenderController();
        if (gRenderController == nullptr) {
            LOGE("Failed to get render controller");
            return JNI_FALSE;
        }
        
        // 3. 设置渲染配置
        VuRenderViewConfig renderConfig = vuRenderViewConfigDefault();
        renderConfig.resolution.data[0] = 1280;
        renderConfig.resolution.data[1] = 720;
        
        int result = gRenderController->setRenderViewConfig(&renderConfig);
        if (result != VU_SUCCESS) {
            LOGE("Failed to set render config: %d", result);
            return JNI_FALSE;
        }
        
        gVuforiaInitialized = true;
        LOGI("Vuforia 10 engine created successfully");
        return JNI_TRUE;
        
    } catch (const std::exception& e) {
        LOGE("Exception during Vuforia initialization: %s", e.what());
        return JNI_FALSE;
    } catch (...) {
        LOGE("Unknown exception during Vuforia initialization");
        return JNI_FALSE;
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaManager_startCameraNative(
    JNIEnv* env, jobject instance, jobject surface) {
    
    if (gEngine == nullptr) {
        LOGE("Engine not created");
        return JNI_FALSE;
    }
    
    LOGI("Starting Vuforia camera...");
    
    try {
        // 启动引擎（这会自动启动相机）
        int result = gEngine->start();
        if (result != VU_SUCCESS) {
            LOGE("Failed to start engine: %d", result);
            return JNI_FALSE;
        }
        
        gCameraStarted = true;
        LOGI("Camera started successfully");
        return JNI_TRUE;
        
    } catch (const std::exception& e) {
        LOGE("Exception during camera start: %s", e.what());
        return JNI_FALSE;
    } catch (...) {
        LOGE("Unknown exception during camera start");
        return JNI_FALSE;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaManager_renderFrameNative(
    JNIEnv* env, jobject instance) {
    
    if (gEngine == nullptr) return;
    
    try {
        // 获取最新状态
        VuState* state = nullptr;
        int result = gEngine->acquireLatestState(&state);
        if (result != VU_SUCCESS || state == nullptr) return;
        
        // 获取渲染状态
        VuRenderState renderState;
        result = state->getRenderState(&renderState);
        if (result == VU_SUCCESS) {
            // 渲染视频背景（这是显示相机预览的关键）
            renderVideoBackground(renderState);
        }
        
        // 释放状态
        state->release();
        
    } catch (const std::exception& e) {
        LOGE("Exception during frame rendering: %s", e.what());
    } catch (...) {
        LOGE("Unknown exception during frame rendering");
    }
}

// 渲染视频背景（相机预览的核心）
void renderVideoBackground(const VuRenderState& renderState) {
    try {
        // 设置视口
        glViewport(renderState.vbViewport.pos.data[0], 
                   renderState.vbViewport.pos.data[1],
                   renderState.vbViewport.size.data[0], 
                   renderState.vbViewport.size.data[1]);
        
        // 清除背景
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // 渲染简单的背景（模拟相机预览）
        renderSimpleBackground();
        
    } catch (...) {
        LOGE("Exception in renderVideoBackground");
    }
}

void renderSimpleBackground() {
    try {
        // 渲染简单的背景颜色（模拟相机预览）
        glClearColor(0.2f, 0.3f, 0.8f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        // 可以在这里添加一些简单的图形来模拟相机内容
        
    } catch (...) {
        LOGE("Exception in renderSimpleBackground");
    }
}

// 目标数据库和检测相关方法
extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaManager_initTargetDatabase(
    JNIEnv* env, jobject thiz) {
    
    LOGI("Initializing target database...");
    
    // 检查 Asset Manager
    if (gAssetManager == nullptr) {
        LOGE("Asset manager not set");
        return JNI_FALSE;
    }
    
    // 初始化目标列表
    gTargets.clear();
    gTargets.push_back("stones");
    gTargets.push_back("chips");
    gTargets.push_back("tarmac");
    
    // 创建目标检测器
    gTargetDetector = std::make_unique<SimpleTargetDetector>();
    
    LOGI("Target database initialized successfully with %zu targets", gTargets.size());
    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaManager_startImageTracking(
    JNIEnv* env, jobject thiz) {
    
    LOGI("Starting image tracking...");
    if (gTargetDetector == nullptr) {
        LOGE("Target detector not initialized");
        return JNI_FALSE;
    }
    
    gTargetDetectionActive = gTargetDetector->startDetection();
    LOGI("Image tracking started: %s", gTargetDetectionActive ? "true" : "false");
    return gTargetDetectionActive ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaManager_stopImageTracking(
    JNIEnv* env, jobject thiz) {
    
    LOGI("Stopping image tracking...");
    if (gTargetDetector != nullptr) {
        gTargetDetector->stopDetection();
    }
    gTargetDetectionActive = false;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaManager_updateTargetDetection(
    JNIEnv* env, jobject thiz) {
    
    if (gTargetDetector != nullptr && gTargetDetectionActive) {
        return gTargetDetector->updateDetection() ? JNI_TRUE : JNI_FALSE;
    }
    return JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaManager_setTargetDetectionCallback(
    JNIEnv* env, jobject thiz, jobject callback) {
    
    if (env && callback) {
        env->GetJavaVM(&gJVM);
        if (gTargetCallback != nullptr) {
            env->DeleteGlobalRef(gTargetCallback);
        }
        gTargetCallback = env->NewGlobalRef(callback);
        LOGI("Target detection callback set");
    }
}

// 模型和渲染相关方法
extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaManager_setModelLoaded(
    JNIEnv* env, jobject thiz, jboolean loaded) {
    gModelLoaded = loaded;
    LOGI("Model loaded status set to: %s", loaded ? "true" : "false");
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaManager_isModelLoadedNative(
    JNIEnv* env, jobject thiz) {
    return gModelLoaded ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaManager_loadGLBModel(
    JNIEnv* env, jobject thiz, jstring model_path) {
    
    if (model_path) {
        const char* path = env->GetStringUTFChars(model_path, nullptr);
        if (path) {
            LOGI("Loading GLB model: %s", path);
            env->ReleaseStringUTFChars(model_path, path);
            
            // 模拟模型加载成功
            gModelLoaded = true;
            return JNI_TRUE;
        }
    }
    
    LOGE("Invalid model path");
    return JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaManager_startRendering(
    JNIEnv* env, jobject thiz) {
    LOGI("Starting AR rendering");
    gARRendering = true;
    return JNI_TRUE;
}

// Device Tracking 相关方法
extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaManager_enableDeviceTracking(
    JNIEnv* env, jobject thiz) {
    LOGI("Device tracking enabled");
    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaManager_disableImageTracking(
    JNIEnv* env, jobject thiz) {
    LOGI("Image tracking disabled");
    return JNI_TRUE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaManager_setWorldCenterMode(
    JNIEnv* env, jobject thiz) {
    LOGI("World center mode set");
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaManager_setupCameraBackground(
    JNIEnv* env, jobject thiz) {
    LOGI("Camera background setup completed");
    return JNI_TRUE;
}

// 清理资源
extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaManager_cleanup(
    JNIEnv* env, jobject thiz) {
    
    LOGI("Cleaning up Vuforia resources");
    
    try {
        // 停止目标检测
        if (gTargetDetector) {
            gTargetDetector->stopDetection();
            gTargetDetector.reset();
        }
        
        // 清理引擎
        if (gEngine != nullptr) {
            gEngine->stop();
            gEngine->destroy();
            gEngine = nullptr;
        }
        
        // 清理回调
        if (gTargetCallback != nullptr && env != nullptr) {
            env->DeleteGlobalRef(gTargetCallback);
            gTargetCallback = nullptr;
        }
        
        // 重置状态
        gRenderController = nullptr;
        gVuforiaInitialized = false;
        gModelLoaded = false;
        gARRendering = false;
        gTargetDetectionActive = false;
        gCameraStarted = false;
        gTargets.clear();
        
        LOGI("Cleanup completed successfully");
        
    } catch (const std::exception& e) {
        LOGE("Exception during cleanup: %s", e.what());
    } catch (...) {
        LOGE("Unknown exception during cleanup");
    }
}