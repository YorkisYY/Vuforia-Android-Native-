#ifndef VUFORIA_WRAPPER_H
#define VUFORIA_WRAPPER_H
//C:\Users\USER\Desktop\IBM-WEATHER-ART-ANDRIOD\app\src\main\cpp\VuforiaWrapper.h

// ==================== 標準庫和系統依賴 ====================
#include <jni.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <queue>
#include <chrono>
#include <unordered_map>
#include <cstring>  // 添加 memset 支持
#include <sstream>  // 添加 stringstream 支持
#include <iomanip>  // 用于 std::setprecision
#include <GLES3/gl3.h>       // OpenGL ES 3.0   
#include <EGL/egl.h>
#include "VuforiaEngine/VuforiaEngine.h"
#ifndef GL_TEXTURE_EXTERNAL_OES
#define GL_TEXTURE_EXTERNAL_OES 0x8D65
#endif
// ==================== 日誌宏定義 ====================
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "VuforiaWrapper"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

// ==================== ✅ 全域變數聲明（不是定義！）====================
// 這些實際定義在 vuforia_wrapper.cpp 中
extern jobject gAndroidContext;
extern JavaVM* gJavaVM;

// ==================== ✅ 函數聲明（不是定義！）====================
// 錯誤處理回調函數聲明
void errorCallback(const char* message, void* clientData);

// ==================== 錯誤處理宏 ====================
#define CHECK_VU_RESULT(result, operation) \
    do { \
        if ((result) != VU_SUCCESS) { \
            LOGE("%s failed with error: %d", (operation), (result)); \
            return false; \
        } \
        LOGD("%s succeeded", (operation)); \
    } while(0)

#define CHECK_VU_RESULT_RETURN_VOID(result, operation) \
    do { \
        if ((result) != VU_SUCCESS) { \
            LOGE("%s failed with error: %d", (operation), (result)); \
            return; \
        } \
        LOGD("%s succeeded", (operation)); \
    } while(0)

// ==================== 矩陣工具函數 ====================
namespace VuforiaWrapper {
    // 手動設置單位矩陣的工具函數
    inline void setIdentityMatrix(VuMatrix44F& matrix) {
        memset(&matrix, 0, sizeof(VuMatrix44F));
        matrix.data[0] = 1.0f;   // [0,0]
        matrix.data[5] = 1.0f;   // [1,1]
        matrix.data[10] = 1.0f;  // [2,2]
        matrix.data[15] = 1.0f;  // [3,3]
    }
    
    // 複製矩陣的工具函數
    inline void copyMatrix(VuMatrix44F& dest, const VuMatrix44F& src) {
        memcpy(&dest, &src, sizeof(VuMatrix44F));
    }
}

// ==================== 常量定義 ====================
namespace VuforiaWrapper {
    // 渲染常量
    static constexpr float DEFAULT_NEAR_PLANE = 0.1f;
    static constexpr float DEFAULT_FAR_PLANE = 1000.0f;
    
    // 相機常量
    static constexpr int DEFAULT_CAMERA_WIDTH = 1920;
    static constexpr int DEFAULT_CAMERA_HEIGHT = 1080;
    
    // Target 檢測常量
    static constexpr int MAX_SIMULTANEOUS_TARGETS = 10;
    static constexpr float TARGET_SCALE_FACTOR = 1.0f;
    
    // 版本字符串常量
    static constexpr size_t VERSION_STRING_SIZE = 256;
}

// ==================== 數據結構定義 ====================
namespace VuforiaWrapper {
    
    // Target 檢測事件
    enum class TargetEventType {
        TARGET_FOUND = 0,
        TARGET_TRACKING = 1,
        TARGET_LOST = 2,
        TARGET_EXTENDED_TRACKING = 3
    };
    
    // Target 事件數據
    struct TargetEvent {
        std::string targetName;
        TargetEventType eventType;
        VuMatrix44F poseMatrix;
        std::chrono::steady_clock::time_point timestamp;
        float confidence;
        
        TargetEvent() : eventType(TargetEventType::TARGET_LOST), confidence(0.0f) {
            // 使用新的矩陣初始化方法
            setIdentityMatrix(poseMatrix);
        }
    };
    
    // 相機幀數據
    struct CameraFrameData {
        int width;
        int height;
        VuImagePixelFormat format;
        std::vector<uint8_t> imageData;
        VuMatrix44F projectionMatrix;
        VuMatrix44F viewMatrix;
        int64_t timestamp;
        
        CameraFrameData() : width(0), height(0), format(VU_IMAGE_PIXEL_FORMAT_UNKNOWN), timestamp(0) {
            // 使用新的矩陣初始化方法
            setIdentityMatrix(projectionMatrix);
            setIdentityMatrix(viewMatrix);
        }
    };
    
    // 引擎狀態
    enum class EngineState {
        NOT_INITIALIZED = 0,
        INITIALIZED = 1,
        STARTED = 2,
        PAUSED = 3,
        ERROR_STATE = -1
    };
}

// ==================== 核心類別前向聲明 ====================
namespace VuforiaWrapper {
    class TargetEventManager;
    class CameraFrameExtractor;
    class VuforiaEngineWrapper;
}

// ==================== 目標事件管理器 ====================
namespace VuforiaWrapper {
    class TargetEventManager {
    private:
        std::queue<TargetEvent> mEventQueue;
        mutable std::mutex mQueueMutex;  // 修正：添加 mutable
        std::unordered_map<std::string, TargetEventType> mLastEventMap;
        
    public:
        TargetEventManager() = default;
        ~TargetEventManager() = default;
        
        // 線程安全的事件添加
        void addEvent(const std::string& targetName, TargetEventType eventType, 
                     const VuMatrix44F& poseMatrix, float confidence = 1.0f);
        
        // 批量處理事件（在主線程中調用）
        void processEvents(JNIEnv* env, jobject callback);
        
        // 清空事件隊列
        void clearEvents();
        
        // 獲取隊列大小
        size_t getEventCount() const;
        
    private:
        // 檢查事件是否需要觸發（避免重複事件）
        bool shouldTriggerEvent(const std::string& targetName, TargetEventType eventType);
        
        // 調用 Java 回調方法
        void callJavaCallback(JNIEnv* env, jobject callback, const TargetEvent& event);
    };
}

// ==================== 相機幀提取器 ====================
namespace VuforiaWrapper {
    class CameraFrameExtractor {
    private:
        CameraFrameData mLatestFrame;
        mutable std::mutex mFrameMutex;  // 修正：添加 mutable
        bool mFrameAvailable;
        
    public:
        CameraFrameExtractor() : mFrameAvailable(false) {}
        ~CameraFrameExtractor() = default;
        
        // 從 VuState 提取相機幀數據
        bool extractFrameData(const VuState* state);
        
        // 獲取最新幀數據（線程安全）
        bool getLatestFrame(CameraFrameData& frameData);
        
        // 檢查是否有新幀可用
        bool isFrameAvailable() const { return mFrameAvailable; }
        
    private:
        // 提取圖像數據 - 修正參數類型
        bool extractImageData(const VuCameraFrame* cameraFrame, CameraFrameData& frameData);
        
        // 提取渲染矩陣
        bool extractRenderMatrices(const VuState* state, CameraFrameData& frameData);
    };
}

// ==================== 主要 Wrapper 類別 ====================
namespace VuforiaWrapper {
    class VuforiaEngineWrapper {
    private:
        // Vuforia 核心組件
        VuEngine* mEngine;
        VuController* mController;
        VuController* mRenderController;     // 修正：使用 VuController*
        VuController* mCameraController;     // 修正：使用 VuController*
        VuController* mRecorderController;   // 修正：使用 VuController*
        
        // 平台相關
        AAssetManager* mAssetManager;
        
        // 狀態管理
        EngineState mEngineState;
        bool mImageTrackingActive;
        bool mDeviceTrackingEnabled;
        
        // Observer 管理 - 修正類型以解決編譯錯誤
        std::vector<VuObserver*> mImageTargetObservers;
        std::unordered_map<std::string, void*> mDatabases;  // 修正：暫時使用 void* 避免類型錯誤
        
        // 事件和數據管理
        std::unique_ptr<TargetEventManager> mEventManager;
        std::unique_ptr<CameraFrameExtractor> mFrameExtractor;
        
        // JNI 相關
        JavaVM* mJVM;
        jobject mTargetCallback;
        
        // ✅ 新增的成員變數 - 渲染循環狀態
        bool mRenderingLoopActive;
        
        // ✅ Surface相關狀態
        void* mCurrentSurface;
        int mSurfaceWidth;
        int mSurfaceHeight;
        bool mSurfaceReady;
        // ✅ 相機狀態
        bool mCameraActive;
        
        // ✅ 相機權限狀態
        bool mCameraPermissionGranted;
        bool mCameraHardwareSupported;
        
        // ===== 新增的渲染相关私有成员变量 =====
        
        // 渲染资源管理
        mutable std::mutex mRenderMutex;
        bool mRenderInitialized;
        GLuint mVideoBackgroundShaderProgram;
        GLuint mVideoBackgroundVAO;
        GLuint mVideoBackgroundVBO;
        GLuint mVideoBackgroundTextureId;
        
        // 性能监控
        mutable std::mutex mPerformanceMutex;
        std::chrono::steady_clock::time_point mLastFrameTime;
        float mCurrentFPS;
        long mTotalFrameCount;
        
        // 渲染配置
        VuRenderConfig mRenderConfig;
        bool mVideoBackgroundRenderingEnabled;
        int mRenderingQuality;
        
        // OpenGL状态保存
        struct OpenGLState {
            GLboolean depthTestEnabled;
            GLboolean cullFaceEnabled;
            GLboolean blendEnabled;
            GLenum blendSrcFactor;
            GLenum blendDstFactor;
            GLuint currentProgram;
            GLuint currentTexture;
        } mSavedGLState;
        
        // 同步對象
        mutable std::mutex mEngineMutex;
    bool isOpenGLInitialized() const { return false; }
    std::string getOpenGLInfo() const { return "Implemented in rendering module"; }    
    public:
        VuforiaEngineWrapper();
        ~VuforiaEngineWrapper();
        
        // 禁用拷貝和賦值
        VuforiaEngineWrapper(const VuforiaEngineWrapper&) = delete;
        VuforiaEngineWrapper& operator=(const VuforiaEngineWrapper&) = delete;
        
        // ==================== 生命週期管理 ====================
        bool initialize(const std::string& licenseKey);
        bool start();
        void pause();
        void resume();
        void stop();
        void deinitialize();
        
        // ==================== 狀態查詢 ====================
        EngineState getEngineState() const { return mEngineState; }
        bool isInitialized() const { return mEngineState != EngineState::NOT_INITIALIZED; }
        bool isRunning() const { return mEngineState == EngineState::STARTED; }
        
        // ==================== 設定方法 ====================
        void setAssetManager(AAssetManager* assetManager);
        void setTargetCallback(JNIEnv* env, jobject callback);
        void setScreenOrientation(int orientation);
        
        // ==================== 渲染相關 ====================
        bool initializeRendering();
        bool startRendering();
        void stopRendering();
        bool setupCameraBackground();
        
        // ==================== 相機控制 ====================
        bool startCamera();
        void stopCamera();
        bool setCameraVideoMode(VuCameraVideoModePreset preset);
        bool setCameraFocusMode(VuCameraFocusMode focusMode);
        
        // ==================== Image Target 管理 ====================
        bool loadImageTargetDatabase(const std::string& databasePath);
        bool createImageTargetObserver(const std::string& targetName, const std::string& databaseId = "");
        bool startImageTracking();
        void stopImageTracking();
        bool isImageTrackingActive() const { return mImageTrackingActive; }
        
        // ==================== Device Tracking ====================
        bool enableDeviceTracking();
        void disableDeviceTracking();
        bool isDeviceTrackingEnabled() const { return mDeviceTrackingEnabled; }
        
        // ==================== 3D 模型管理 ====================
        bool loadGLBModel(const std::string& modelPath);
        void unloadModel();
        bool isModelLoaded() const;
        
        // ==================== 主要渲染循環 ====================
        void renderFrame(JNIEnv* env);
        
        // ==================== 數據獲取 ====================
        bool getCameraFrame(CameraFrameData& frameData);
        std::vector<TargetEvent> getDetectedTargets();
        VuMatrix44F getProjectionMatrix() const;
        VuMatrix44F getViewMatrix() const;
        
        // ==================== 工具方法 ====================
        std::string getVuforiaVersion() const;
        int getVuforiaStatus() const;
            // ✅ 新增：設置Surface尺寸的方法
        void setSurfaceDimensions(int width, int height) {
            mSurfaceWidth = width;
            mSurfaceHeight = height;
            LOGI("📱 Surface dimensions stored: %dx%d", width, height);
        }
        
        // ✅ 新增：獲取Surface尺寸的方法
        void getSurfaceDimensions(int& width, int& height) const {
            width = mSurfaceWidth;
            height = mSurfaceHeight;
        }
        // ==================== 渲染循环控制方法（解决编译错误的关键）====================
        /**
         * 启动渲染循环
         * @return 成功返回true，失败返回false
         */
        bool startRenderingLoop();
        
        /**
         * 停止渲染循环 - 解决编译错误的核心方法
         */
        void stopRenderingLoop();
        
        /**
         * 检查渲染循环是否活跃
         * @return 活跃返回true，否则返回false
         */
        bool isRenderingLoopActive() const;

        // ==================== 相机生命周期管理 ====================
        /**
         * 检查相机是否活跃
         * @return 相机活跃返回true，否则返回false
         */
        bool isCameraActive() const;

        // ==================== Surface 管理方法 ====================
        /**
         * 设置渲染Surface
         * @param surface Android Surface对象指针
         */
        void setRenderingSurface(void* surface);
        
        /**
         * 处理Surface创建事件
         * @param width Surface宽度
         * @param height Surface高度
         */
        void onSurfaceCreated(int width, int height);
        
        /**
         * 处理Surface销毁事件
         */
        void onSurfaceDestroyed();

        // ==================== 扩展的状态查询方法 ====================
        /**
         * 获取详细的引擎状态信息
         * @return 状态描述字符串
         */
        std::string getEngineStatusDetail() const;
        
        /**
         * 获取内存使用情况
         * @return 内存使用情况描述
         */
        std::string getMemoryUsageInfo() const;
        
        /**
         * 检查引擎是否正在运行
         * @return 运行中返回true，否则返回false
         */
        bool isEngineRunning() const;

        // ==================== 安全的图像追踪控制 ====================
        /**
         * 安全地停止图像追踪
         * 使用更稳定的方式停止追踪，避免潜在的崩溃
         */
        void stopImageTrackingSafe();

        // ==================== ✅ 相机权限检查方法 ====================
        /**
         * 检查相机权限状态
         * 适用于 Vuforia 11.3.4 的权限检查
         * @return 有权限返回true，否则返回false
         */
        bool checkCameraPermission() const;
        
        /**
         * 检查相机是否可访问
         * 验证相机硬件是否可用且引擎状态正常
         * @return 相机可访问返回true，否则返回false
         */
        bool isCameraAccessible() const;
        
        /**
         * 获取相机权限详细状态
         * @return 权限状态描述字符串
         */
        std::string getCameraPermissionStatus() const;
        
        /**
         * 验证相机硬件支持
         * 检查设备是否支持所需的相机功能
         * @return 支持返回true，否则返回false
         */
        bool verifyCameraHardwareSupport() const;

        /**
         * 驗證 Vuforia 權限配置
         * @return 配置正確返回true，否則返回false
         */
        bool validateVuforiaPermissions() const;
        
        /**
         * 獲取權限錯誤詳細信息
         * @return 錯誤詳細描述字符串
         */
        std::string getPermissionErrorDetail() const;
        
        // ===== 新增的渲染相关公开方法 =====
        
        // OpenGL资源管理
        bool initializeOpenGLResources();
        void cleanupOpenGLResources();
        bool validateOpenGLSetup() const;
        
        // 视频背景渲染
        bool setupVideoBackgroundRendering();
        void renderFrameWithVideoBackground(JNIEnv* env);
        bool createVideoBackgroundShader();
        bool setupVideoBackgroundTexture();
        void renderVideoBackgroundWithProperShader(const VuRenderState& renderState);
        void renderVideoBackgroundWithTexture(const VuRenderState& renderState);  // 保留原有方法
        
        // 调试和诊断
        void debugCurrentRenderState();
        std::string getRenderingStatusDetail() const;
        
        // 性能监控
        float getCurrentRenderingFPS() const;
        long getTotalFrameCount() const;
        
        // 渲染配置
        void setVideoBackgroundRenderingEnabled(bool enabled);
        void setRenderingQuality(int quality);
        void onSurfaceChanged(int width, int height);
        
        // OpenGL状态管理
        void saveOpenGLState();
        void restoreOpenGLState();
        void updateRenderConfig();
        
    private:
        // ==================== 內部初始化方法 ====================
        bool createEngineConfig(VuEngineConfigSet** configSet, const std::string& licenseKey);
        bool setupControllers();
        bool configureRendering();
        bool configureCamera();
        
        // ==================== 內部處理方法 ====================
        void processVuforiaState(const VuState* state);
        void extractTargetObservations(const VuState* state);
        void updateCameraFrame(const VuState* state);
        
        // ==================== 資源管理 ====================
        void cleanup();
        void cleanupObservers();
        void cleanupDatabases();
        
        // ==================== 工具函數 ====================
        bool checkVuResult(VuResult result, const char* operation) const;
        std::vector<uint8_t> readAssetFile(const std::string& filename) const;
        
        // ==================== ✅ 相机权限预检查方法 ====================
        /**
         * 预先检查相机权限
         * 在引擎初始化前验证相机权限状态
         * @return 有权限返回true，否则返回false
         */
        bool preCheckCameraPermission();
        
        /**
         * 更新相機權限狀態
         */
        void updateCameraPermissionStatus();
        
        // ===== 新增的渲染相关私有方法 =====
        
        // 渲染相关私有方法
        void renderCameraBackgroundSimple(const VuState* state);        
        // 性能统计更新
        void updatePerformanceStats();
        
        // 渲染状态调试
        void debugRenderState(const VuRenderState& renderState) const;
    public:
        // 新增的公共方法 - 解決 VuMesh API 相關編譯錯誤
        void renderVideoBackgroundMesh(const VuRenderState& renderState);
        VuEngine* getEngine() const { return mEngine; }
    };
}

// ==================== 全局工具函數聲明 ====================
namespace VuforiaWrapper {
    // 矩陣轉換工具
    void convertVuforiaToFilamentMatrix(const VuMatrix44F& vuMatrix, float* filamentMatrix);
    void convertFilamentToVuforiaMatrix(const float* filamentMatrix, VuMatrix44F& vuMatrix);
    
    // 版本信息
    std::string getLibraryVersionString();
    
    // 錯誤碼轉換
    std::string vuResultToString(VuResult result);
    std::string vuEngineCreationErrorToString(VuEngineCreationError error);
    
    // ==================== ✅ 相机权限检查函数 ====================
    /**
     * 全局相机权限检查函数 - 适用于 Vuforia 11.3.4
     * @param env JNI环境
     * @param thiz Java对象
     * @return 有权限返回JNI_TRUE，否则返回JNI_FALSE
     */
    extern "C" JNIEXPORT jboolean JNICALL
    Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_checkCameraPermissionNative(
        JNIEnv* env, jobject thiz);
    
    /**
     * 检查相机是否可访问
     * @param env JNI环境
     * @param thiz Java对象
     * @return 相机可访问返回JNI_TRUE，否则返回JNI_FALSE
     */
    extern "C" JNIEXPORT jboolean JNICALL
    Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_isCameraAccessible(
        JNIEnv* env, jobject thiz);
    
    /**
     * 驗證 Vuforia 初始化狀態與權限
     */
    extern "C" JNIEXPORT jboolean JNICALL
    Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_validateVuforiaPermissionsNative(
        JNIEnv* env, jobject thiz);
    
    /**
     * 獲取權限錯誤詳細信息
     */
    extern "C" JNIEXPORT jstring JNICALL
    Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_getPermissionErrorDetailNative(
        JNIEnv* env, jobject thiz);

    /**
     * 獲取相機權限狀態詳細信息
     */
    extern "C" JNIEXPORT jstring JNICALL
    Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_getCameraPermissionStatusNative(
        JNIEnv* env, jobject thiz);
    
    // ==================== Vuforia Engine 生命周期函数 ====================
    /**
     * 恢复 Vuforia Engine
     * @param env JNI环境
     * @param thiz Java对象
     */
    extern "C" JNIEXPORT void JNICALL
    Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_resumeVuforiaEngineNative(
        JNIEnv* env, jobject thiz);
    
    /**
     * 暂停 Vuforia Engine
     * @param env JNI环境
     * @param thiz Java对象
     */
    extern "C" JNIEXPORT void JNICALL
    Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_pauseVuforiaEngineNative(
        JNIEnv* env, jobject thiz);
    
    /**
     * 清理 Vuforia Engine
     * @param env JNI环境
     * @param thiz Java对象
     */
    extern "C" JNIEXPORT void JNICALL
    Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_deinitVuforiaEngineNative(
        JNIEnv* env, jobject thiz);
        
    /**
     * 初始化 Vuforia Engine
     * @param env JNI环境
     * @param thiz Java对象
     * @param license_key 许可证密钥
     * @return 成功返回JNI_TRUE，失败返回JNI_FALSE
     */
    extern "C" JNIEXPORT jboolean JNICALL
    Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_initVuforiaEngineNative(
        JNIEnv* env, jobject thiz, jstring license_key);
}

// ==================== 單例訪問器 ====================
namespace VuforiaWrapper {
    // 獲取全局 VuforiaEngineWrapper 實例
    VuforiaEngineWrapper& getInstance();
    
    // 銷毀全局實例（在應用程序退出時調用）
    void destroyInstance();
}


#endif // VUFORIA_WRAPPER_H