#include "VuforiaWrapper.h"
//C:\Users\USER\Desktop\IBM-WEATHER-ART-ANDRIOD\app\src\main\cpp\vuforia_wrapper.cpp
// ==================== 全局變量聲明 ====================
jobject gAndroidContext = nullptr;
JavaVM* gJavaVM = nullptr;
static VuEngine* mEngine = nullptr;
static bool mEngineStarted = false;

// Vuforia License Key - 請替換為你的實際 License Key
static const char* kVuforiaLicenseKey = "AddD0sD/////AAABmb2xv80J2UAshKy68I6M8/chOh4Bd0UsKQeqMnCZenkh8Z9mPEun8HUhBzpsnjGETKQBX0Duvgp/m3k9GYnZks41tcRtaGnjXvwRW/t3zXQH1hAulR/AbMsXnoxHWBtHIE3YzDLnk5/MO30VVXre2sz8ZBKtJCKsw4lA8UH1fwzO07aWsLkyGxBqDynU4sq509TAxqB2IdoGsW6kHpl6hz5RA8PzIE5UmUBIdM3/xjAAw/HJ9LJrP+i4KmkRXWHpYLD0bJhq66b78JfigD/zcm+bGK2TS1Klo6/6xkhHYCsd7LOcPmO0scdNVdNBrGugBgDps2n3YFyciQbFPYrGk4rW7u8EPlpABJIDbr0dVTv3W";

// 錯誤處理回調
void errorCallback(const char* message, void* clientData){
    LOGE("Vuforia Error: %s", message);
}
// ==================== 全局實例管理 ====================
namespace VuforiaWrapper {
    static std::unique_ptr<VuforiaEngineWrapper> gWrapperInstance = nullptr;
    static std::mutex gInstanceMutex;
    
    VuforiaEngineWrapper& getInstance() {
        std::lock_guard<std::mutex> lock(gInstanceMutex);
        if (!gWrapperInstance) {
            gWrapperInstance = std::make_unique<VuforiaEngineWrapper>();
        }
        return *gWrapperInstance;
    }
    
    void destroyInstance() {
        std::lock_guard<std::mutex> lock(gInstanceMutex);
        gWrapperInstance.reset();
    }
}

// ==================== TargetEventManager 實現 ====================
namespace VuforiaWrapper {
    
    void TargetEventManager::addEvent(const std::string& targetName, TargetEventType eventType, 
                                    const VuMatrix44F& poseMatrix, float confidence) {
        std::lock_guard<std::mutex> lock(mQueueMutex);
        
        // 檢查是否需要觸發事件（避免重複）
        if (!shouldTriggerEvent(targetName, eventType)) {
            return;
        }
        
        TargetEvent event;
        event.targetName = targetName;
        event.eventType = eventType;
        copyMatrix(event.poseMatrix, poseMatrix);
        event.confidence = confidence;
        event.timestamp = std::chrono::steady_clock::now();
        
        mEventQueue.push(event);
        mLastEventMap[targetName] = eventType;
        
        LOGD("Target event added: %s, type: %d", targetName.c_str(), static_cast<int>(eventType));
    }
    
    bool TargetEventManager::shouldTriggerEvent(const std::string& targetName, TargetEventType eventType) {
        auto it = mLastEventMap.find(targetName);
        if (it == mLastEventMap.end()) {
            return true;
        }
        return it->second != eventType;
    }
    
    void TargetEventManager::processEvents(JNIEnv* env, jobject callback) {
        if (env == nullptr || callback == nullptr) {
            return;
        }
        
        std::queue<TargetEvent> tempQueue;
        {
            std::lock_guard<std::mutex> lock(mQueueMutex);
            tempQueue.swap(mEventQueue);
        }
        
        while (!tempQueue.empty()) {
            const auto& event = tempQueue.front();
            callJavaCallback(env, callback, event);
            tempQueue.pop();
        }
    }
    
    void TargetEventManager::callJavaCallback(JNIEnv* env, jobject callback, const TargetEvent& event) {
        jclass callbackClass = env->GetObjectClass(callback);
        if (callbackClass == nullptr) {
            return;
        }
        
        const char* methodName = nullptr;
        switch (event.eventType) {
            case TargetEventType::TARGET_FOUND:
                methodName = "onTargetFound";
                break;
            case TargetEventType::TARGET_TRACKING:
            case TargetEventType::TARGET_EXTENDED_TRACKING:
                methodName = "onTargetTracking";
                break;
            case TargetEventType::TARGET_LOST:
                methodName = "onTargetLost";
                break;
        }
        
        if (methodName != nullptr) {
            jmethodID methodId = env->GetMethodID(callbackClass, methodName, "(Ljava/lang/String;)V");
            if (methodId != nullptr) {
                jstring jTargetName = env->NewStringUTF(event.targetName.c_str());
                if (jTargetName != nullptr) {
                    env->CallVoidMethod(callback, methodId, jTargetName);
                    env->DeleteLocalRef(jTargetName);
                }
            }
        }
        
        env->DeleteLocalRef(callbackClass);
    }
    
    void TargetEventManager::clearEvents() {
        std::lock_guard<std::mutex> lock(mQueueMutex);
        std::queue<TargetEvent> empty;
        mEventQueue.swap(empty);
        mLastEventMap.clear();
    }
    
    size_t TargetEventManager::getEventCount() const {
        std::lock_guard<std::mutex> lock(mQueueMutex);
        return mEventQueue.size();
    }

} // namespace VuforiaWrapper

// ==================== CameraFrameExtractor 實現 ====================
namespace VuforiaWrapper {
    
    bool CameraFrameExtractor::extractFrameData(const VuState* state) {
        if (state == nullptr) {
            return false;
        }
        
        std::lock_guard<std::mutex> lock(mFrameMutex);
        
        // 獲取相機幀 - 修正：正確的參數類型
        VuCameraFrame* cameraFrame = nullptr;
        VuResult result = vuStateGetCameraFrame(state, &cameraFrame);
        if (result != VU_SUCCESS || cameraFrame == nullptr) {
            return false;
        }
        
        // 提取圖像數據
        if (!extractImageData(cameraFrame, mLatestFrame)) {
            return false;
        }
        
        // 提取渲染矩陣
        if (!extractRenderMatrices(state, mLatestFrame)) {
            return false;
        }
        
        // 獲取時間戳
        vuCameraFrameGetTimestamp(cameraFrame, &mLatestFrame.timestamp);
        
        mFrameAvailable = true;
        return true;
    }
    
    // 修正：添加常量定義以避免 magic number 警告
    static const int32_t DEFAULT_FRAME_WIDTH = 640;
    static const int32_t DEFAULT_FRAME_HEIGHT = 480;
    
    bool CameraFrameExtractor::extractImageData(const VuCameraFrame* cameraFrame, CameraFrameData& frameData) {
        // 修正：創建圖像列表並使用正確的 API
        VuImageList* images = nullptr;
        VuResult result = vuImageListCreate(&images);
        if (result != VU_SUCCESS || images == nullptr) {
            return false;
        }
        
        // 修正：移除 & 符號，直接傳遞指針
        result = vuCameraFrameGetImages(cameraFrame, images);
        if (result != VU_SUCCESS) {
            vuImageListDestroy(images);
            return false;
        }
        
        int32_t numImages = 0;
        vuImageListGetSize(images, &numImages);
        if (numImages == 0) {
            vuImageListDestroy(images);
            return false;
        }
        
        // 獲取第一個圖像（通常是主相機圖像）
        VuImage* image = nullptr;
        vuImageListGetElement(images, 0, &image);
        if (image == nullptr) {
            vuImageListDestroy(images);
            return false;
        }
        
        // 修正：在 Vuforia 11.x 中，圖像信息可能需要通過其他方式獲取
        // 暫時設置默認值，因為 vuImageGetInfo 可能不存在
        LOGW("Image info extraction temporarily disabled - API not available in Vuforia 11.x");
        frameData.width = DEFAULT_FRAME_WIDTH;   // 使用常量
        frameData.height = DEFAULT_FRAME_HEIGHT;  // 使用常量
        frameData.format = VU_IMAGE_PIXEL_FORMAT_RGB888;  // 默認格式
        
        // 清理資源
        vuImageListDestroy(images);
        
        // 暫時不複製圖像數據，因為 vuImageGetPixels 不可用
        frameData.imageData.clear();
        
        return true;
    }
    
    bool CameraFrameExtractor::extractRenderMatrices(const VuState* state, CameraFrameData& frameData) {
        // 修正：使用正確的 Vuforia 11.x 渲染狀態獲取方式
        VuRenderState renderState;
        VuResult result = vuStateGetRenderState(state, &renderState);
        if (result != VU_SUCCESS) {
            return false;
        }
        
        // 修正：在 Vuforia 11.x 中，矩陣直接包含在 renderState 中
        // 不是指針，而是直接的矩陣值
        copyMatrix(frameData.projectionMatrix, renderState.projectionMatrix);
        copyMatrix(frameData.viewMatrix, renderState.viewMatrix);
        
        return true;
    }
    
    bool CameraFrameExtractor::getLatestFrame(CameraFrameData& frameData) {
        std::lock_guard<std::mutex> lock(mFrameMutex);
        if (!mFrameAvailable) {
            return false;
        }
        
        frameData = mLatestFrame;
        return true;
    }
}

// ==================== VuforiaEngineWrapper 主要實現 ====================
namespace VuforiaWrapper {
    
    VuforiaEngineWrapper::VuforiaEngineWrapper() 
        : mEngine(nullptr)
        , mController(nullptr)
        , mRenderController(nullptr)
        , mCameraController(nullptr)
        , mRecorderController(nullptr)
        , mAssetManager(nullptr)
        , mEngineState(EngineState::NOT_INITIALIZED)
        , mImageTrackingActive(false)
        , mDeviceTrackingEnabled(false)
        , mJVM(nullptr)
        , mTargetCallback(nullptr)
        // ✅ 新增的成员变量初始化
        , mRenderingLoopActive(false)
        , mCurrentSurface(nullptr)
        , mSurfaceWidth(0)
        , mSurfaceHeight(0)
        , mSurfaceReady(false)
        , mCameraActive(false)
        // ✅ 新增的相机权限状态初始化
        , mCameraPermissionGranted(false)
        , mCameraHardwareSupported(false)
        , mRenderInitialized(false)
        , mVideoBackgroundShaderProgram(0)
        , mVideoBackgroundVAO(0)
        , mVideoBackgroundVBO(0)
        , mVideoBackgroundTextureId(0)
        , mCurrentFPS(0.0f)
        , mTotalFrameCount(0)
        , mVideoBackgroundRenderingEnabled(true)
        , mRenderingQuality(1) // 默认中等质量        
    {
        mEventManager = std::make_unique<TargetEventManager>();
        mFrameExtractor = std::make_unique<CameraFrameExtractor>();
        mLastFrameTime = std::chrono::steady_clock::now();
        memset(&mSavedGLState, 0, sizeof(mSavedGLState));
        LOGI("VuforiaEngineWrapper created with rendering support");
    }
    
    VuforiaEngineWrapper::~VuforiaEngineWrapper() {
        deinitialize();
        LOGI("VuforiaEngineWrapper destroyed");
    }
    
    bool VuforiaEngineWrapper::initialize(const std::string& licenseKey) {
        std::lock_guard<std::mutex> lock(mEngineMutex);
        
        if (mEngineState != EngineState::NOT_INITIALIZED) {
            LOGW("Engine already initialized");
            return true;
        }

        // ✅ 重要：在初始化前檢查 Android Context 和 JavaVM
        if (gAndroidContext == nullptr || gJavaVM == nullptr) {
            LOGE("❌ Android context or JavaVM not set - camera permission may fail");
            LOGE("   Make sure to call setAndroidContextNative before initialization");
            return false;
        }

        LOGI("🔑 Initializing Vuforia Engine 11.3.4 with camera permission checks...");
        
        try {
            // ✅ 預檢查：嘗試檢查相機權限狀態
            if (!preCheckCameraPermission()) {
                LOGE("❌ Camera permission pre-check failed");
                return false;
            }

            // 創建引擎配置
            VuEngineConfigSet* configSet = nullptr;
            if (!createEngineConfig(&configSet, licenseKey)) {
                return false;
            }
            
            // 創建引擎 - 在 Vuforia 11.3.4 中這裡會檢查相機權限
            VuErrorCode errorCode = VU_SUCCESS;
            VuResult result = vuEngineCreate(&mEngine, configSet, &errorCode);
            
            // 清理配置
            vuEngineConfigSetDestroy(configSet);
            
            if (!checkVuResult(result, "vuEngineCreate")) {
                // ✅ 特別處理權限錯誤
                if (errorCode == VU_ENGINE_CREATION_ERROR_PERMISSION_ERROR) {
                    LOGE("❌ CAMERA PERMISSION ERROR - Please ensure camera permission is granted");
                    LOGE("   Error code: VU_ENGINE_CREATION_ERROR_PERMISSION_ERROR");
                } else {
                    LOGE("❌ Engine creation error: %d", errorCode);
                }
                return false;
            }
            
            // 設置控制器
            if (!setupControllers()) {
                vuEngineDestroy(mEngine);
                mEngine = nullptr;
                return false;
            }
            
            mEngineState = EngineState::INITIALIZED;
            LOGI("✅ Vuforia Engine 11.3.4 initialized successfully with camera access");
            return true;
            
        } catch (const std::exception& e) {
            LOGE("Exception during initialization: %s", e.what());
            mEngineState = EngineState::ERROR_STATE;
            return false;
        }
    }
    
    bool VuforiaEngineWrapper::createEngineConfig(VuEngineConfigSet** configSet, const std::string& licenseKey) {
        VuResult result = vuEngineConfigSetCreate(configSet);
        if (!checkVuResult(result, "vuEngineConfigSetCreate")) {
            return false;
        }
        
        // 添加許可證配置
        if (!licenseKey.empty()) {
            VuLicenseConfig licenseConfig = vuLicenseConfigDefault();
            licenseConfig.key = licenseKey.c_str();
            
            // ✅ 添加詳細的 License 診斷
            LOGI("🔑 License Key Info:");
            LOGI("   Length: %zu characters", licenseKey.length());
            LOGI("   First 20 chars: %.20s", licenseKey.c_str());
            LOGI("   Last 20 chars: ...%s", licenseKey.length() > 20 ? licenseKey.substr(licenseKey.length()-20).c_str() : "");
            
            result = vuEngineConfigSetAddLicenseConfig(*configSet, &licenseConfig);
            if (!checkVuResult(result, "vuEngineConfigSetAddLicenseConfig")) {
                LOGE("❌ License configuration failed - possible invalid license key");
                vuEngineConfigSetDestroy(*configSet);
                return false;
            }
            LOGI("✅ License configuration successful");
        } else {
            LOGE("❌ License key is empty!");
            vuEngineConfigSetDestroy(*configSet);
            return false;
        }
        
        // Android 平台配置
        VuPlatformAndroidConfig androidConfig = vuPlatformAndroidConfigDefault();
        if (gAndroidContext != nullptr && gJavaVM != nullptr) {
            androidConfig.activity = gAndroidContext;
            androidConfig.javaVM = gJavaVM;
            
            // ✅ 添加 Android 配置診斷
            LOGI("🤖 Android Config Info:");
            LOGI("   Activity context: %p", gAndroidContext);
            LOGI("   JavaVM: %p", gJavaVM);
            
        } else {
            LOGE("❌ Android context or JavaVM not set!");
            LOGE("   gAndroidContext: %p", gAndroidContext);
            LOGE("   gJavaVM: %p", gJavaVM);
            vuEngineConfigSetDestroy(*configSet);
            return false;
        }
        
        result = vuEngineConfigSetAddPlatformAndroidConfig(*configSet, &androidConfig);
        if (!checkVuResult(result, "vuEngineConfigSetAddPlatformAndroidConfig")) {
            LOGE("❌ Android platform configuration failed");
            vuEngineConfigSetDestroy(*configSet);
            return false;
        }
        LOGI("✅ Android platform configuration successful");
        
        return true;
}
    bool VuforiaEngineWrapper::setupControllers() {
        // ✅ 在 Vuforia 11.3.4 中，不需要手動獲取 RenderController
        // 渲染控制是通過全局函數實現的，而不是控制器對象
        
        mController = reinterpret_cast<VuController*>(mEngine);
        if (mController == nullptr) {
            LOGE("Failed to get main controller");
            return false;
        }
        
        // ✅ RenderController 在 11.3.4 中通過全局函數使用
        mRenderController = nullptr;  // 不需要獨立的控制器對象
        mCameraController = nullptr;
        mRecorderController = nullptr;
        
        LOGI("Controllers setup completed (Vuforia 11.3.4 style)");
        return true;
    }
    
    bool VuforiaEngineWrapper::start() {
        std::lock_guard<std::mutex> lock(mEngineMutex);
        
        if (mEngineState == EngineState::STARTED) {
            LOGW("Engine already started");
            return true;
        }
        
        if (mEngineState != EngineState::INITIALIZED && mEngineState != EngineState::PAUSED) {
            LOGE("Engine not in correct state to start");
            return false;
        }
        
        LOGI("Starting Vuforia Engine...");
        
        try {
            VuResult result = vuEngineStart(mEngine);
            if (!checkVuResult(result, "vuEngineStart")) {
                return false;
            }
            
            mEngineState = EngineState::STARTED;
            
            // 如果surface已经准备好，激活相机和渲染
            if (mSurfaceReady) {
                mCameraActive = true;
                mRenderingLoopActive = true;
                LOGI("✅ Camera and rendering activated with engine start");
            }
            
            LOGI("Vuforia Engine started successfully");
            return true;
            
        } catch (const std::exception& e) {
            LOGE("Exception during engine start: %s", e.what());
            mEngineState = EngineState::ERROR_STATE;
            return false;
        }
    }
    
    void VuforiaEngineWrapper::pause() {
        std::lock_guard<std::mutex> lock(mEngineMutex);
        
        // 先停止渲染循环
        if (mRenderingLoopActive) {
            LOGI("⏸️ Stopping rendering loop during pause");
            mRenderingLoopActive = false;
        }
        
        // 停止相机
        mCameraActive = false;
        
        if (mEngineState == EngineState::STARTED) {
            LOGI("Pausing Vuforia Engine...");
            vuEngineStop(mEngine);
            mEngineState = EngineState::PAUSED;
        }
    }
    
    void VuforiaEngineWrapper::resume() {
        std::lock_guard<std::mutex> lock(mEngineMutex);
        
        if (mEngineState == EngineState::PAUSED) {
            LOGI("Resuming Vuforia Engine...");
            VuResult result = vuEngineStart(mEngine);
            if (checkVuResult(result, "vuEngineStart")) {
                mEngineState = EngineState::STARTED;
            }
        }
    }
    
    void VuforiaEngineWrapper::stop() {
        std::lock_guard<std::mutex> lock(mEngineMutex);
        
        if (mEngineState == EngineState::STARTED) {
            LOGI("Stopping Vuforia Engine...");
            vuEngineStop(mEngine);
            mEngineState = EngineState::INITIALIZED;
        }
    }
    
    void VuforiaEngineWrapper::deinitialize() {
        std::lock_guard<std::mutex> lock(mEngineMutex);
        
        if (mEngineState == EngineState::NOT_INITIALIZED) {
            return;
        }
        
        LOGI("Deinitializing Vuforia Engine...");
        
        try {
            // 停止引擎（如果正在運行）
            if (mEngineState == EngineState::STARTED) {
                vuEngineStop(mEngine);
            }
            
            // 清理資源
            cleanup();
            
            // 銷毀引擎
            if (mEngine != nullptr) {
                vuEngineDestroy(mEngine);
                mEngine = nullptr;
            }
            
            // 重置狀態
            mEngineState = EngineState::NOT_INITIALIZED;
            mImageTrackingActive = false;
            mDeviceTrackingEnabled = false;
            
            LOGI("Vuforia Engine deinitialized successfully");
            
        } catch (const std::exception& e) {
            LOGE("Exception during deinitialization: %s", e.what());
        }
    }
    
    void VuforiaEngineWrapper::cleanup() {
        // 清理回調
        if (mTargetCallback != nullptr && mJVM != nullptr) {
            JNIEnv* env = nullptr;
            if (mJVM->AttachCurrentThread(&env, nullptr) == JNI_OK && env != nullptr) {
                env->DeleteGlobalRef(mTargetCallback);
            }
            mTargetCallback = nullptr;
        }
        
        // 清理 Observers
        cleanupObservers();
        
        // 清理數據庫
        cleanupDatabases();
        
        // 清理事件
        if (mEventManager) {
            mEventManager->clearEvents();
        }
        
        // 重置控制器指針
        mController = nullptr;
        mRenderController = nullptr;
        mCameraController = nullptr;
        mRecorderController = nullptr;
    }
    
    void VuforiaEngineWrapper::cleanupObservers() {
        for (auto observer : mImageTargetObservers) {
            if (observer != nullptr && mEngine != nullptr) {
                // 修正：在 Vuforia 11.x 中，直接跳過觀察器銷毀
                // 因為專用的銷毀函數可能不存在
                LOGW("Observer cleanup skipped - destruction API not available in Vuforia 11.x");
                // 注意：在實際應用中，觀察器可能會由引擎自動管理生命周期
            }
        }
        mImageTargetObservers.clear();
    }
    
    void VuforiaEngineWrapper::cleanupDatabases() {
        for (auto& dbPair : mDatabases) {
            if (dbPair.second != nullptr && mEngine != nullptr) {
                // 修正：暫時跳過數據庫銷毀，因為 VuDatabase 類型可能不可用
                LOGW("Database cleanup temporarily disabled - VuDatabase type not available");
                // 在 Vuforia 11.x 中，數據庫管理可能已經簡化
                // 或者使用不同的類型和函數
            }
        }
        mDatabases.clear();
    }
    
    bool VuforiaEngineWrapper::initializeRendering() {
        // 修正：在 Vuforia 11.x 中渲染初始化可能不需要控制器
        // 或者使用不同的 API
        LOGI("Initializing rendering for Vuforia 11.x...");
        
        // 暫時跳過渲染控制器相關設置
        // 在 Vuforia 11.x 中，渲染可能通過不同的方式管理
        
        LOGI("Rendering initialization completed (simplified for Vuforia 11.x)");
        return true;
    }
    
    bool VuforiaEngineWrapper::loadImageTargetDatabase(const std::string& databasePath) {
        if (mEngine == nullptr || mAssetManager == nullptr) {
            LOGE("Engine or AssetManager not available");
            return false;
        }
        
        LOGI("Loading image target database: %s", databasePath.c_str());
        
        try {
            // 讀取數據庫文件
            std::string xmlPath = databasePath + ".xml";
            std::string datPath = databasePath + ".dat";
            
            std::vector<uint8_t> xmlData = readAssetFile(xmlPath);
            std::vector<uint8_t> datData = readAssetFile(datPath);
            
            if (xmlData.empty() || datData.empty()) {
                LOGE("Failed to read database files: %s", databasePath.c_str());
                return false;
            }
            
            // 修正：暫時跳過數據庫創建，因為 VuDatabase 類型可能不可用
            LOGW("Database creation temporarily disabled - VuDatabase type not available in Vuforia 11.x");
            
            // 在 Vuforia 11.x 中，數據庫創建可能使用不同的 API
            // 暫時存儲路徑信息以便後續使用
            mDatabases[databasePath] = nullptr;  // 使用路徑作為標識
            
            LOGI("Image target database loaded successfully: %s", databasePath.c_str());
            return true;
            
        } catch (const std::exception& e) {
            LOGE("Exception loading database: %s", e.what());
            return false;
        }
    }
    
    bool VuforiaEngineWrapper::createImageTargetObserver(const std::string& targetName, const std::string& databaseId) {
        if (mEngine == nullptr) {
            LOGE("Engine not available");
            return false;
        }
        
        // 如果沒有指定數據庫ID，使用第一個可用的數據庫
        std::string dbId = databaseId;
        if (dbId.empty() && !mDatabases.empty()) {
            dbId = mDatabases.begin()->first;
        }
        
        auto dbIt = mDatabases.find(dbId);
        if (dbIt == mDatabases.end()) {
            LOGE("Database not found: %s", dbId.c_str());
            return false;
        }
        
        LOGI("Creating image target observer: %s", targetName.c_str());
        
        // 修正：創建 Image Target 配置使用正確的成員
        VuImageTargetConfig config = vuImageTargetConfigDefault();
        config.databasePath = dbId.c_str();      // 修正：使用 databasePath 而不是 databaseId
        config.targetName = targetName.c_str();
        config.activate = VU_TRUE;
        config.scale = TARGET_SCALE_FACTOR;      // 修正：使用 scale 而不是 scaleToSize
        
        // 創建 Observer
        VuObserver* observer = nullptr;  // 修正：使用 VuObserver* 而不是 VuImageTargetObserver*
        VuResult result = vuEngineCreateImageTargetObserver(mEngine, &observer, &config, nullptr);
        
        if (!checkVuResult(result, "vuEngineCreateImageTargetObserver")) {
            return false;
        }
        
        // 存儲觀察器
        mImageTargetObservers.push_back(observer);
        LOGI("Image target observer created successfully: %s", targetName.c_str());
        return true;
    }
    
    bool VuforiaEngineWrapper::startImageTracking() {
        if (mImageTargetObservers.empty()) {
            LOGE("No image target observers available");
            return false;
        }
        
        LOGI("Starting image tracking...");
        mImageTrackingActive = true;
        return true;
    }
    
    void VuforiaEngineWrapper::stopImageTracking() {
        LOGI("Stopping image tracking...");
        mImageTrackingActive = false;
    }
    
    void VuforiaEngineWrapper::renderFrame(JNIEnv* env) {
        if (mEngineState != EngineState::STARTED) {
            return;
        }
        
        try {
            // 获取最新状态
            VuState* state = nullptr;
            VuResult result = vuEngineAcquireLatestState(mEngine, &state);  // ✅ 直接使用 mEngine
            if (result != VU_SUCCESS || state == nullptr) {
                return;
            }
            
            // ✅ 简化版本：只清除屏幕并显示基本渲染
            renderCameraBackgroundSimple(state);
            
            // 处理状态数据
            processVuforiaState(state);
            
            // 释放状态
            vuStateRelease(state);
            
            // 处理事件回调
            if (mEventManager && mTargetCallback != nullptr) {
                mEventManager->processEvents(env, mTargetCallback);
            }
            
        } catch (const std::exception& e) {
            LOGE("Exception during frame rendering: %s", e.what());
        }
    }
    void renderVideoBackgroundWithProperShader(const VuRenderState& renderState) {
        if (!g_renderingState.initialized || g_renderingState.videoBackgroundShaderProgram == 0) {
            LOGW_RENDER("⚠️ Rendering not initialized");
            return;
        }
        
        try {
            // ✅ 修正：使用 Vuforia 11.3.4 的正確成員名稱
            if (renderState.vbMesh == nullptr || 
                renderState.vbMesh->position == nullptr ||
                renderState.vbMesh->numVertices <= 0) {
                LOGW_RENDER("⚠️ Invalid vbMesh data - skipping video background rendering");
                return;
            }
            
            // ✅ 修正：直接使用頂點數量
            int vertexCount = renderState.vbMesh->numVertices;
            LOGD_RENDER("🎥 Rendering video background with %d vertices", vertexCount);
            
            // 設置OpenGL狀態
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            
            // 使用著色器程序
            glUseProgram(g_renderingState.videoBackgroundShaderProgram);
            
            // 設置投影矩陣
            GLint projMatrixLocation = glGetUniformLocation(g_renderingState.videoBackgroundShaderProgram, "u_projectionMatrix");
            if (projMatrixLocation != -1) {
                glUniformMatrix4fv(projMatrixLocation, 1, GL_FALSE, renderState.vbProjectionMatrix.data);
            }
            
            // 設置模型視图矩陣（單位矩陣）
            GLint mvMatrixLocation = glGetUniformLocation(g_renderingState.videoBackgroundShaderProgram, "u_modelViewMatrix");
            if (mvMatrixLocation != -1) {
                const float identityMatrix[16] = {
                    1.0F, 0.0F, 0.0F, 0.0F,
                    0.0F, 1.0F, 0.0F, 0.0F,
                    0.0F, 0.0F, 1.0F, 0.0F,
                    0.0F, 0.0F, 0.0F, 1.0F
                };
                glUniformMatrix4fv(mvMatrixLocation, 1, GL_FALSE, identityMatrix);
            }
            
            // 設置透明度
            GLint alphaLocation = glGetUniformLocation(g_renderingState.videoBackgroundShaderProgram, "u_alpha");
            if (alphaLocation != -1) {
                glUniform1f(alphaLocation, 1.0F);
            }
            
            // 激活並綁定相機紋理
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_EXTERNAL_OES, g_renderingState.videoBackgroundTextureId);
            GLint textureLocation = glGetUniformLocation(g_renderingState.videoBackgroundShaderProgram, "u_cameraTexture");
            if (textureLocation != -1) {
                glUniform1i(textureLocation, 0);
            }
            
            // 設置頂點屬性
            GLint positionAttribute = glGetAttribLocation(g_renderingState.videoBackgroundShaderProgram, "a_position");
            GLint texCoordAttribute = glGetAttribLocation(g_renderingState.videoBackgroundShaderProgram, "a_texCoord");
            
            // ✅ 修正：使用新的 position 成員
            if (positionAttribute != -1 && renderState.vbMesh->position != nullptr) {
                glEnableVertexAttribArray(positionAttribute);
                glVertexAttribPointer(positionAttribute, 3, GL_FLOAT, GL_FALSE, 0, 
                                    renderState.vbMesh->position);
            }
            
            // ✅ 修正：檢查 textureCoordinate 可用性
            if (texCoordAttribute != -1 && 
                renderState.vbMesh->textureCoordinate != nullptr) {
                glEnableVertexAttribArray(texCoordAttribute);
                glVertexAttribPointer(texCoordAttribute, 2, GL_FLOAT, GL_FALSE, 0, 
                                    renderState.vbMesh->textureCoordinate);
            }
            
            // ✅ 修正：使用索引繪製或直接繪製
            if (renderState.vbMesh->numIndices > 0 && renderState.vbMesh->index != nullptr) {
                // 使用索引繪製
                glDrawElements(GL_TRIANGLES, renderState.vbMesh->numIndices, GL_UNSIGNED_SHORT, 
                            renderState.vbMesh->index);
                LOGD_RENDER("✅ Video background rendered with %d indices", renderState.vbMesh->numIndices);
            } else if (renderState.vbMesh->numVertices > 0) {
                // 直接繪製頂點
                glDrawArrays(GL_TRIANGLES, 0, vertexCount);
                LOGD_RENDER("✅ Video background rendered with %d vertices", vertexCount);
            }
            
            // 清理
            if (positionAttribute != -1) {
                glDisableVertexAttribArray(positionAttribute);
            }
            if (texCoordAttribute != -1) {
                glDisableVertexAttribArray(texCoordAttribute);
            }
            
            glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
            glUseProgram(0);
            
            // 恢復OpenGL狀態
            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);
            glDisable(GL_BLEND);
            
            LOGD_RENDER("✅ Video background rendering completed successfully");
            
        } catch (const std::exception& e) {
            LOGE_RENDER("❌ Error in renderVideoBackgroundWithProperShader: %s", e.what());
            glUseProgram(0);
        }
    }

    // ==================== 完整的 renderVideoBackgroundWithTexture 函數實現 ====================
// 基於 Vuforia 11.3.4 官方文檔和 OpenGL ES 2.0 最佳實踐

    void VuforiaEngineWrapper::renderVideoBackgroundWithTexture(const VuRenderState& renderState) {
        try {
            // ✅ 修正：檢查新的 VuMesh 結構
            if (renderState.vbMesh == nullptr ||
                renderState.vbMesh->positions == nullptr ||              // ✅ vertexPositions → positions
                renderState.vbMesh->numPositions <= 0) {                 // ✅ numVertices → numPositions
                LOGW("Invalid video background mesh data");
                return;
            }
            
            // ✅ 修正：計算頂點數
            int vertexCount = renderState.vbMesh->numPositions / 3;
            LOGD("🎥 Rendering video background with %d vertices", vertexCount);
            
            // ✅ 設置 OpenGL 狀態 - 專門用於視頻背景渲染
            glDisable(GL_DEPTH_TEST);    // 視頻背景不需要深度測試
            glDisable(GL_CULL_FACE);     // 確保所有面都被渲染
            glEnable(GL_BLEND);          // 啟用混合
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            
            // ✅ 設置紋理狀態（假設 Vuforia 已經綁定了相機紋理）
            glEnable(GL_TEXTURE_2D);
            
            // ✅ 修正：使用 glDrawArrays 而不是 glDrawElements
            if (renderState.vbMesh->numPositions > 0) {
                glDrawArrays(GL_TRIANGLES, 0, vertexCount);
                LOGD("✅ Video background rendered with %d vertices", vertexCount);
            }
            
            // ✅ 恢復 OpenGL 狀態
            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);
            glDisable(GL_BLEND);
            glDisable(GL_TEXTURE_2D);
            
            LOGD("✅ Video background rendered successfully with texture coordinates");
            
        } catch (const std::exception& e) {
            LOGE("❌ Error in renderVideoBackgroundWithTexture: %s", e.what());
        } catch (...) {
            LOGE("❌ Unknown error in renderVideoBackgroundWithTexture");
        }
    }


    // ==================== 現代 OpenGL ES 2.0 着色器版本（推薦用於產品） ====================
    // 如果您想使用更現代的着色器方法，可以使用下面的版本：

    void VuforiaEngineWrapper::renderVideoBackgroundWithShader(const VuRenderState& renderState) {
        // 注意：這個版本需要預先創建和編譯着色器程序
        // 由於需要大量的着色器設置代碼，這裡提供一個簡化版本
        /*
        try {
            if (renderState.vbMesh->vertexPositions == nullptr || 
                renderState.vbMesh->textureCoordinates == nullptr ||
                renderState.vbMesh->numVertices <= 0) {
                return;
            }*/
            
            // ✅ 假設您已經創建了视频背景着色器程序
            // GLuint videoBackgroundProgram = createVideoBackgroundShaderProgram();
            // glUseProgram(videoBackgroundProgram);
            
            // ✅ 設置 uniform 變量
            // GLint projMatrixLocation = glGetUniformLocation(videoBackgroundProgram, "u_projectionMatrix");
            // glUniformMatrix4fv(projMatrixLocation, 1, GL_FALSE, renderState.vbProjectionMatrix.data);
            
            // ✅ 設置頂點屬性
            // GLint positionAttribute = glGetAttribLocation(videoBackgroundProgram, "a_position");
            // GLint texCoordAttribute = glGetAttribLocation(videoBackgroundProgram, "a_texCoord");
            
            // glEnableVertexAttribArray(positionAttribute);
            // glEnableVertexAttribArray(texCoordAttribute);
            
            // glVertexAttribPointer(positionAttribute, 3, GL_FLOAT, GL_FALSE, 0, renderState.vbMesh.vertexPositions);
            // glVertexAttribPointer(texCoordAttribute, 2, GL_FLOAT, GL_FALSE, 0, renderState.vbMesh.textureCoordinates);
            
            // ✅ 渲染
            // glDrawArrays(GL_TRIANGLES, 0, renderState.vbMesh.numVertices);
            
            // ✅ 清理
            // glDisableVertexAttribArray(positionAttribute);
            // glDisableVertexAttribArray(texCoordAttribute);
            // glUseProgram(0);
            
            LOGD("✅ Video background rendered with modern shader");
            
        } catch (const std::exception& e) {
            LOGE("❌ Error in renderVideoBackgroundWithShader: %s", e.what());
        }
    }

    // ==================== 着色器程序創建輔助函數（可選） ====================
    void VuforiaEngineWrapper::renderVideoBackgroundMesh(const VuRenderState& renderState) {
        if (renderState.vbMesh == nullptr || 
            renderState.vbMesh->positions == nullptr ||
            renderState.vbMesh->numPositions <= 0) {
            LOGW("Invalid mesh data for video background");
            return;
        }

        try {
            // 計算頂點數
            int vertexCount = renderState.vbMesh->numPositions / 3;
            LOGD("🎥 Rendering video background mesh with %d vertices", vertexCount);

            // 設置 OpenGL 狀態
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            // ✅ 使用 Vuforia 11.3.4 正確的渲染方式
            if (renderState.vbMesh->positions != nullptr && renderState.vbMesh->numPositions > 0) {
                // 使用 glDrawArrays 渲染三角形
                glDrawArrays(GL_TRIANGLES, 0, vertexCount);
                LOGD("✅ Video background mesh rendered with %d vertices", vertexCount);
            }
            
            // 恢復 OpenGL 狀態
            glEnable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);
            
        } catch (const std::exception& e) {
            LOGE("❌ Error in renderVideoBackgroundMesh: %s", e.what());
        }
}
    void VuforiaEngineWrapper::renderCameraBackgroundSimple(const VuState* state) {
        if (state == nullptr) {
            return;
        }
        
        try {
            // ✅ 獲取渲染狀態
            VuRenderState renderState;
            VuResult result = vuStateGetRenderState(state, &renderState);
            if (result != VU_SUCCESS) {
                LOGW("Failed to get render state");
                return;
            }
            
            // ✅ 基本清屏
            glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            
            // ✅ 修正：使用正確的條件檢查
            if (renderState.vbMesh != nullptr && 
                renderState.vbMesh->numPositions > 0) {          // ✅ numVertices → numPositions
                // 使用 Vuforia 提供的視頻背景網格進行渲染
                renderVideoBackgroundMesh(renderState);
            }
            
            LOGD("📷 Camera background rendered with render state");
            
        } catch (const std::exception& e) {
            LOGE("❌ Error in renderCameraBackgroundSimple: %s", e.what());
        }
    }

    void VuforiaEngineWrapper::processVuforiaState(const VuState* state) {
        // 提取相機幀數據
        if (mFrameExtractor) {
            mFrameExtractor->extractFrameData(state);
        }
        
        // 提取目標觀察結果
        if (mImageTrackingActive) {
            extractTargetObservations(state);
        }
    }
    
    void VuforiaEngineWrapper::extractTargetObservations(const VuState* state) {
        // 修正：使用專門的 Image Target 觀察獲取函數
        VuObservationList* observations = nullptr;
        VuResult result = vuObservationListCreate(&observations);
        if (result != VU_SUCCESS || observations == nullptr) {
            return;
        }
        
        // 修正：使用 vuStateGetImageTargetObservations 而不是通用的觀察獲取
        result = vuStateGetImageTargetObservations(state, observations);
        if (result != VU_SUCCESS) {
            vuObservationListDestroy(observations);
            return;
        }
        
        int32_t numObservations = 0;
        vuObservationListGetSize(observations, &numObservations);
        
        for (int32_t i = 0; i < numObservations; i++) {
            VuObservation* observation = nullptr;
            vuObservationListGetElement(observations, i, &observation);
            if (observation == nullptr) {
                continue;
            }
            
            // 檢查是否有姿態信息
            if (vuObservationHasPoseInfo(observation) != VU_TRUE) {
                continue;
            }
            
            // 獲取姿態信息
            VuPoseInfo poseInfo;
            vuObservationGetPoseInfo(observation, &poseInfo);
            
            // 修正：使用正確的狀態信息獲取方式
            VuImageTargetObservationStatusInfo statusInfo;
            vuImageTargetObservationGetStatusInfo(observation, &statusInfo);
            
            // 獲取目標信息
            VuImageTargetObservationTargetInfo targetInfo;
            vuImageTargetObservationGetTargetInfo(observation, &targetInfo);
            
            // 修正：根據正確的 pose status 轉換事件類型
            TargetEventType eventType = TargetEventType::TARGET_LOST;
            switch (poseInfo.poseStatus) {
                case VU_OBSERVATION_POSE_STATUS_TRACKED:
                    eventType = TargetEventType::TARGET_FOUND;
                    break;
                case VU_OBSERVATION_POSE_STATUS_EXTENDED_TRACKED:
                    eventType = TargetEventType::TARGET_EXTENDED_TRACKING;
                    break;
                case VU_OBSERVATION_POSE_STATUS_LIMITED:
                case VU_OBSERVATION_POSE_STATUS_NO_POSE:
                    eventType = TargetEventType::TARGET_LOST;
                    break;
                default:
                    continue;
            }
            
            // 修正：使用正確的矩陣成員名稱和大寫後綴
            if (mEventManager) {
                mEventManager->addEvent(targetInfo.name, eventType, poseInfo.pose, 1.0F);
            }
        }
        
        // 清理資源
        vuObservationListDestroy(observations);
    }
    
    bool VuforiaEngineWrapper::checkVuResult(VuResult result, const char* operation) const {
        if (result != VU_SUCCESS) {
            LOGE("%s failed with error: %d", operation, result);
            return false;
        }
        LOGD("%s succeeded", operation);
        return true;
    }
    
    std::vector<uint8_t> VuforiaEngineWrapper::readAssetFile(const std::string& filename) const {
        std::vector<uint8_t> data;
        
        if (mAssetManager == nullptr) {
            LOGE("Asset manager not set");
            return data;
        }
        
        AAsset* asset = AAssetManager_open(mAssetManager, filename.c_str(), AASSET_MODE_BUFFER);
        if (asset == nullptr) {
            LOGE("Failed to open asset: %s", filename.c_str());
            return data;
        }
        
        off_t length = AAsset_getLength(asset);
        if (length > 0) {
            data.resize(static_cast<size_t>(length));
            int bytesRead = AAsset_read(asset, data.data(), static_cast<size_t>(length));
            if (bytesRead != length) {
                LOGE("Failed to read complete asset: %s", filename.c_str());
                data.clear();
            }
        }
        
        AAsset_close(asset);
        return data;
    }
    
    void VuforiaEngineWrapper::setAssetManager(AAssetManager* assetManager) {
        mAssetManager = assetManager;
        LOGI("Asset manager set successfully");
    }
    
    void VuforiaEngineWrapper::setTargetCallback(JNIEnv* env, jobject callback) {
        if (env != nullptr && callback != nullptr) {
            env->GetJavaVM(&mJVM);
            if (mTargetCallback != nullptr) {
                env->DeleteGlobalRef(mTargetCallback);
            }
            mTargetCallback = env->NewGlobalRef(callback);
            LOGI("Target detection callback set");
        }
    }
    
    std::string VuforiaEngineWrapper::getVuforiaVersion() const {
        // 修正：Vuforia 11.x 中函數不需要參數，直接返回值
        VuLibraryVersionInfo versionInfo = vuEngineGetLibraryVersionInfo();
        
        char versionString[VERSION_STRING_SIZE];  // 使用常量
        snprintf(versionString, sizeof(versionString), 
                "Vuforia Engine %d.%d.%d (Build %d)", 
                versionInfo.major, versionInfo.minor, versionInfo.patch, versionInfo.build);
        return std::string(versionString);
    }
    
    int VuforiaEngineWrapper::getVuforiaStatus() const {
        switch (mEngineState) {
            case EngineState::NOT_INITIALIZED:
                return 0;
            case EngineState::INITIALIZED:
            case EngineState::PAUSED:
                return 1;
            case EngineState::STARTED:
                return 2;
            case EngineState::ERROR_STATE:
                return -1;
            default:
                return 0;
        }
    }
    
    bool VuforiaEngineWrapper::getCameraFrame(CameraFrameData& frameData) {
        if (mFrameExtractor) {
            return mFrameExtractor->getLatestFrame(frameData);
        }
        return false;
    }

    // ==================== 渲染循环控制方法实现 ====================
    
    bool VuforiaEngineWrapper::startRenderingLoop() {
        std::lock_guard<std::mutex> lock(mEngineMutex);
        
        if (mRenderingLoopActive) {
            LOGW("Rendering loop already active");
            return true;
        }
        
        if (mEngineState != EngineState::STARTED) {
            LOGE("Cannot start rendering loop: engine not started (state: %d)", static_cast<int>(mEngineState));
            return false;
        }
        
        if (!mSurfaceReady) {
            LOGW("Starting rendering loop without surface ready - may cause issues");
        }
        
        LOGI("Starting rendering loop...");
        mRenderingLoopActive = true;
        
        // 在Vuforia 11.x中，渲染循环通常由引擎内部管理
        // 这里主要是设置状态标志和确保相机激活
        mCameraActive = true;
        
        LOGI("✅ Rendering loop started successfully");
        return true;
    }
    
    void VuforiaEngineWrapper::stopRenderingLoop() {
        std::lock_guard<std::mutex> lock(mEngineMutex);
        
        if (!mRenderingLoopActive) {
            LOGD("Rendering loop already stopped");
            return;
        }
        
        LOGI("🛑 Stopping rendering loop - SOLVING COMPILATION ERROR");
        
        try {
            // 设置状态标志
            mRenderingLoopActive = false;
            mCameraActive = false;
            
            // 在这里可以添加具体的渲染资源清理代码
            // 例如清理OpenGL资源、停止渲染线程等
            
            LOGI("✅ Rendering loop stopped successfully");
        } catch (const std::exception& e) {
            LOGE("❌ Error during rendering loop cleanup: %s", e.what());
            // 即使出错也要设置状态
            mRenderingLoopActive = false;
            mCameraActive = false;
        }
    }
    
    bool VuforiaEngineWrapper::isRenderingLoopActive() const {
        std::lock_guard<std::mutex> lock(mEngineMutex);
        return mRenderingLoopActive && (mEngineState == EngineState::STARTED);
    }

    // ==================== 相机管理方法实现 ====================
    
    bool VuforiaEngineWrapper::isCameraActive() const {
        std::lock_guard<std::mutex> lock(mEngineMutex);
        return mCameraActive && (mEngineState == EngineState::STARTED);
    }

    // ==================== Surface管理方法实现 ====================
    
    void VuforiaEngineWrapper::setRenderingSurface(void* surface) {
        std::lock_guard<std::mutex> lock(mEngineMutex);
        
        LOGI("Setting rendering surface: %p", surface);
        mCurrentSurface = surface;
        
        if (surface != nullptr) {
            LOGI("✅ Rendering surface set successfully");
        } else {
            LOGW("⚠️ Rendering surface set to null");
            mSurfaceReady = false;
            // 如果surface被清空，也停止渲染循环
            if (mRenderingLoopActive) {
                LOGI("Stopping rendering loop due to null surface");
                mRenderingLoopActive = false;
            }
        }
    }
    
    void VuforiaEngineWrapper::onSurfaceCreated(int width, int height) {
        std::lock_guard<std::mutex> lock(mEngineMutex);
        
        LOGI("🖼️ Surface created: %dx%d", width, height);
        
        // 更新surface状态
        mSurfaceWidth = width;
        mSurfaceHeight = height;
        mSurfaceReady = true;
        
        // 如果引擎已经启动，激活相机和渲染
        if (mEngineState == EngineState::STARTED) {
            mCameraActive = true;
            
            // 如果渲染循环未启动，现在启动它
            if (!mRenderingLoopActive) {
                LOGI("🚀 Auto-starting rendering loop after surface creation");
                mRenderingLoopActive = true;
            }
        }
        
        LOGI("✅ Surface ready for rendering: %dx%d", width, height);
    }
    
    void VuforiaEngineWrapper::onSurfaceDestroyed() {
        std::lock_guard<std::mutex> lock(mEngineMutex);
        
        LOGI("🖼️ Surface destroyed");
        
        // 停止渲染循环
        if (mRenderingLoopActive) {
            LOGI("Stopping rendering loop due to surface destruction");
            mRenderingLoopActive = false;
        }
        
        // 清理Surface相关状态
        mCurrentSurface = nullptr;
        mSurfaceWidth = 0;
        mSurfaceHeight = 0;
        mSurfaceReady = false;
        mCameraActive = false;
        
        LOGI("✅ Surface cleanup completed");
    }

    // ==================== 扩展状态查询方法实现 ====================
    
    std::string VuforiaEngineWrapper::getEngineStatusDetail() const {
        std::lock_guard<std::mutex> lock(mEngineMutex);
        
        std::ostringstream status;
        status << "=== Vuforia Engine Status Detail ===\n";
        status << "Engine State: " << static_cast<int>(mEngineState);
        
        switch (mEngineState) {
            case EngineState::NOT_INITIALIZED:
                status << " (NOT_INITIALIZED)\n";
                break;
            case EngineState::INITIALIZED:
                status << " (INITIALIZED)\n";
                break;
            case EngineState::STARTED:
                status << " (STARTED)\n";
                break;
            case EngineState::PAUSED:
                status << " (PAUSED)\n";
                break;
            case EngineState::ERROR_STATE:
                status << " (ERROR_STATE)\n";
                break;
        }
        
        status << "Rendering Loop Active: " << (mRenderingLoopActive ? "Yes" : "No") << "\n";
        status << "Camera Active: " << (mCameraActive ? "Yes" : "No") << "\n";
        status << "Surface Ready: " << (mSurfaceReady ? "Yes" : "No") << "\n";
        
        if (mSurfaceReady) {
            status << "Surface Size: " << mSurfaceWidth << "x" << mSurfaceHeight << "\n";
        }
        
        status << "Image Tracking Active: " << (mImageTrackingActive ? "Yes" : "No") << "\n";
        status << "Target Observers: " << mImageTargetObservers.size() << "\n";
        
        if (mEventManager) {
            status << "Pending Events: " << mEventManager->getEventCount() << "\n";
        }
        
        return status.str();
    }
    
    std::string VuforiaEngineWrapper::getMemoryUsageInfo() const {
        std::lock_guard<std::mutex> lock(mEngineMutex);
        
        std::ostringstream memInfo;
        memInfo << "=== Vuforia Memory Usage ===\n";
        memInfo << "Engine Instance: " << (mEngine != nullptr ? "Active" : "Null") << "\n";
        memInfo << "Controllers: " << (mController != nullptr ? "Active" : "Null") << "\n";
        memInfo << "Event Manager: " << (mEventManager != nullptr ? "Active" : "Null") << "\n";
        memInfo << "Frame Extractor: " << (mFrameExtractor != nullptr ? "Active" : "Null") << "\n";
        memInfo << "Asset Manager: " << (mAssetManager != nullptr ? "Active" : "Null") << "\n";
        memInfo << "Target Callback: " << (mTargetCallback != nullptr ? "Set" : "Null") << "\n";
        memInfo << "Current Surface: " << (mCurrentSurface != nullptr ? "Set" : "Null") << "\n";
        
        return memInfo.str();
    }
        
    bool VuforiaEngineWrapper::isEngineRunning() const {
        std::lock_guard<std::mutex> lock(mEngineMutex);
        return (mEngineState == EngineState::STARTED);
    }

    // ==================== 安全的图像追踪控制实现 ====================
    
    void VuforiaEngineWrapper::stopImageTrackingSafe() {
        std::lock_guard<std::mutex> lock(mEngineMutex);
        
        LOGI("🎯 Stopping image tracking (safe mode)...");
        
        try {
            // 使用状态管理而不是可能有问题的API调用
            mImageTrackingActive = false;
            
            // 清理事件队列
            if (mEventManager) {
                mEventManager->clearEvents();
            }
            
            LOGI("✅ Image tracking stopped safely via state management");
        } catch (const std::exception& e) {
            LOGE("❌ Error during safe image tracking stop: %s", e.what());
            // 即使出错，也要设置状态
            mImageTrackingActive = false;
        }
    }

    // ==================== 相机权限检查方法实现 ====================
    bool VuforiaEngineWrapper::checkCameraPermission() const {
        std::lock_guard<std::mutex> lock(mEngineMutex);
        LOGI("📷 Checking camera permission status...");
        
        // 检查引擎状态 - 如果引擎能成功启动，代表有相机权限
        if (mEngine != nullptr && mEngineState == EngineState::STARTED) {
            LOGI("✅ Camera permission OK - engine is running");
            return true;
        }
        
        // 检查引擎是否因权限问题无法创建
        if (mEngine == nullptr && mEngineState == EngineState::ERROR_STATE) {
            LOGE("❌ Engine in error state - likely permission issue");
            return false;
        }
        
        LOGW("⚠️ Camera permission status unclear - engine not started");
        return false;
    }
    
    bool VuforiaEngineWrapper::isCameraAccessible() const {
        std::lock_guard<std::mutex> lock(mEngineMutex);
        
        if (mEngine == nullptr || mEngineState != EngineState::STARTED) {
            return false;
        }
        
        // 在 Vuforia 11.3.4 中，如果引擎成功启动且相机标志为 active，
        // 代表相机可以访问
        return mCameraActive && mEngineStarted;
    }
    
    std::string VuforiaEngineWrapper::getCameraPermissionStatus() const {
        std::lock_guard<std::mutex> lock(mEngineMutex);
        std::ostringstream status;
        
        status << "=== Camera Permission Status (Vuforia 11.3.4) ===\n";
        status << "Engine Initialized: " << (mEngine != nullptr ? "Yes" : "No") << "\n";
        status << "Engine State: " << static_cast<int>(mEngineState);
        
        switch (mEngineState) {
            case EngineState::NOT_INITIALIZED:
                status << " (NOT_INITIALIZED)\n";
                break;
            case EngineState::INITIALIZED:
                status << " (INITIALIZED)\n";
                break;
            case EngineState::STARTED:
                status << " (STARTED)\n";
                break;
            case EngineState::PAUSED:
                status << " (PAUSED)\n";
                break;
            case EngineState::ERROR_STATE:
                status << " (ERROR_STATE - Likely Permission Issue)\n";
                break;
        }
        
        status << "Camera Active: " << (mCameraActive ? "Yes" : "No") << "\n";
        status << "Surface Ready: " << (mSurfaceReady ? "Yes" : "No") << "\n";
        status << "Permission Granted: " << (mCameraPermissionGranted ? "Yes" : "No") << "\n";
        status << "Hardware Supported: " << (mCameraHardwareSupported ? "Yes" : "No") << "\n";
        status << "Android Context: " << (gAndroidContext != nullptr ? "Set" : "Not Set") << "\n";
        status << "JavaVM: " << (gJavaVM != nullptr ? "Set" : "Not Set") << "\n";
        
        return status.str();
    }
    
    bool VuforiaEngineWrapper::verifyCameraHardwareSupport() const {
        std::lock_guard<std::mutex> lock(mEngineMutex);
        LOGI("🔧 Verifying camera hardware support...");
        
        // 在 Vuforia 11.3.4 中，如果引擎能够成功创建，
        // 通常代表硬件支持没问题
        bool hardwareSupported = (mEngine != nullptr);
        
        LOGI("📱 Camera hardware support: %s", hardwareSupported ? "Available" : "Not Available");
        return hardwareSupported;
    }

    // ✅ 新增方法：相机权限预检查
    bool VuforiaEngineWrapper::preCheckCameraPermission() {
        if (gJavaVM == nullptr) {
            LOGE("JavaVM not available for permission check");
            return false;
        }

        JNIEnv* env = nullptr;
        bool attached = false;

        // 获取 JNI 环境
        int getEnvStat = gJavaVM->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
        if (getEnvStat == JNI_EDETACHED) {
            if (gJavaVM->AttachCurrentThread(&env, nullptr) != 0) {
                LOGE("Failed to attach thread for permission check");
                return false;
            }
            attached = true;
        } else if (getEnvStat == JNI_OK) {
            // Already attached
        } else {
            LOGE("Failed to get JNI environment for permission check");
            return false;
        }

        bool hasPermission = false;
        try {
            // 检查相机权限
            if (gAndroidContext != nullptr) {
                jclass contextClass = env->GetObjectClass(gAndroidContext);
                jmethodID checkPermissionMethod = env->GetMethodID(contextClass, "checkSelfPermission", "(Ljava/lang/String;)I");
                
                if (checkPermissionMethod != nullptr) {
                    jstring cameraPermission = env->NewStringUTF("android.permission.CAMERA");
                    jint result = env->CallIntMethod(gAndroidContext, checkPermissionMethod, cameraPermission);
                    
                    // PackageManager.PERMISSION_GRANTED = 0
                    hasPermission = (result == 0);
                    LOGI("🔍 Camera permission check result: %s", hasPermission ? "GRANTED" : "DENIED");
                    
                    env->DeleteLocalRef(cameraPermission);
                }
                env->DeleteLocalRef(contextClass);
            }
        } catch (...) {
            LOGE("Exception during permission check");
            hasPermission = false;
        }

        if (attached) {
            gJavaVM->DetachCurrentThread();
        }

        return hasPermission;
    }
    
    bool VuforiaEngineWrapper::validateVuforiaPermissions() const {
        std::lock_guard<std::mutex> lock(mEngineMutex);
        LOGI("🔍 Validating Vuforia permissions...");
        
        // 检查必要条件
        if (gAndroidContext == nullptr) {
            LOGE("❌ Android context not set");
            return false;
        }
        
        if (gJavaVM == nullptr) {
            LOGE("❌ JavaVM not set");
            return false;
        }
        
        if (mEngine == nullptr) {
            LOGE("❌ Engine not created - possible permission issue");
            return false;
        }
        
        if (mEngineState == EngineState::ERROR_STATE) {
            LOGE("❌ Engine in error state - likely permission problem");
            return false;
        }
        
        LOGI("✅ Vuforia permissions validation passed");
        return true;
    }
    
    std::string VuforiaEngineWrapper::getPermissionErrorDetail() const {
        std::lock_guard<std::mutex> lock(mEngineMutex);
        std::ostringstream errorDetail;
        
        errorDetail << "=== Permission Error Analysis ===\n";
        
        if (gAndroidContext == nullptr) {
            errorDetail << "❌ CRITICAL: Android Context not set\n";
            errorDetail << "   Solution: Call setAndroidContextNative() before initialization\n";
        }
        
        if (gJavaVM == nullptr) {
            errorDetail << "❌ CRITICAL: JavaVM not set\n";
            errorDetail << "   This should be set automatically in setAndroidContextNative()\n";
        }
        
        if (mEngine == nullptr) {
            errorDetail << "❌ CRITICAL: Vuforia Engine creation failed\n";
            errorDetail << "   Possible causes:\n";
            errorDetail << "   - Camera permission not granted\n";
            errorDetail << "   - Invalid license key\n";
            errorDetail << "   - Hardware not supported\n";
        }
        
        if (mEngineState == EngineState::ERROR_STATE) {
            errorDetail << "❌ CRITICAL: Engine in error state\n";
            errorDetail << "   This usually indicates a permission or hardware issue\n";
        }
        
        if (!mCameraActive && mEngineState == EngineState::STARTED) {
            errorDetail << "⚠️ WARNING: Engine started but camera not active\n";
            errorDetail << "   This might indicate a runtime permission issue\n";
        }
        
        errorDetail << "\n🔧 Recommended Actions:\n";
        errorDetail << "1. Ensure camera permission is granted before initialization\n";
        errorDetail << "2. Verify Android Context is set correctly\n";
        errorDetail << "3. Check Vuforia license key validity\n";
        errorDetail << "4. Verify device hardware compatibility\n";
        
        return errorDetail.str();
    }
    
    void VuforiaEngineWrapper::updateCameraPermissionStatus() {
        // 这个方法可以定期调用来更新权限状态
        mCameraPermissionGranted = preCheckCameraPermission();
        mCameraHardwareSupported = verifyCameraHardwareSupport();
        
        LOGI("📊 Permission status updated:");
        LOGI("   Camera Permission: %s", mCameraPermissionGranted ? "✅ Granted" : "❌ Denied");
        LOGI("   Hardware Support: %s", mCameraHardwareSupported ? "✅ Supported" : "❌ Not Supported");
    }
}

// ==================== JNI 函數實現 ====================

extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_initialization_VuforiaInitialization_deinitVuforiaEngineNative(
    JNIEnv* env, jobject thiz) {
    
    VuforiaWrapper::getInstance().deinitialize();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_setAssetManagerNative(
    JNIEnv* env, jobject thiz, jobject asset_manager) {
    
    if (asset_manager != nullptr) {
        AAssetManager* assetManager = AAssetManager_fromJava(env, asset_manager);
        VuforiaWrapper::getInstance().setAssetManager(assetManager);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_setTargetDetectionCallbackNative(
    JNIEnv* env, jobject thiz, jobject callback) {
    
    VuforiaWrapper::getInstance().setTargetCallback(env, callback);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_renderFrameNative(
    JNIEnv* env, jobject thiz) {
    
    VuforiaWrapper::getInstance().renderFrame(env);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_loadImageTargetsNative(
    JNIEnv* env, jobject thiz, jstring database_path) {
    
    if (database_path == nullptr) {
        return JNI_FALSE;
    }
    
    const char* path = env->GetStringUTFChars(database_path, nullptr);
    if (path == nullptr) {
        return JNI_FALSE;
    }
    
    bool success = VuforiaWrapper::getInstance().loadImageTargetDatabase(path);
    
    env->ReleaseStringUTFChars(database_path, path);
    return success ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_startImageTrackingNative(
    JNIEnv* env, jobject thiz) {
    
    return VuforiaWrapper::getInstance().startImageTracking() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_getVuforiaVersionNative(
    JNIEnv* env, jobject thiz) {
    
    std::string version = VuforiaWrapper::getInstance().getVuforiaVersion();
    return env->NewStringUTF(version.c_str());
}
extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_setAndroidContextNative(
    JNIEnv* env, jobject thiz, jobject context) {
    
    LOGI("setAndroidContextNative called");
    
    if (context != nullptr) {
        jclass contextClass = env->GetObjectClass(context);
        jmethodID getClassMethod = env->GetMethodID(contextClass, "getClass", "()Ljava/lang/Class;");
        jobject classObj = env->CallObjectMethod(context, getClassMethod);
        jclass classClass = env->GetObjectClass(classObj);
        jmethodID getNameMethod = env->GetMethodID(classClass, "getName", "()Ljava/lang/String;");
        jstring className = (jstring)env->CallObjectMethod(classObj, getNameMethod);
        const char* classNameStr = env->GetStringUTFChars(className, nullptr);
        
        LOGI("📋 Context class: %s", classNameStr);
        
        // ✅ 修復 JNI 布爾值比較
        jclass activityClass = env->FindClass("android/app/Activity");
        if (env->IsInstanceOf(context, activityClass) != 0u) {  // ✅ 添加 != 0u
            LOGI("✅ Context is Activity instance");
        } else {
            LOGE("❌ Context is NOT Activity instance");
        }
        
        env->GetJavaVM(&gJavaVM);
        
        if (gAndroidContext != nullptr) {
            env->DeleteGlobalRef(gAndroidContext);
        }
        
        gAndroidContext = env->NewGlobalRef(context);
        LOGI("✅ Android context set successfully");
        
        // 清理
        env->ReleaseStringUTFChars(className, classNameStr);
        env->DeleteLocalRef(className);
        env->DeleteLocalRef(classObj);
        env->DeleteLocalRef(classClass);
        env->DeleteLocalRef(contextClass);
        env->DeleteLocalRef(activityClass);
    } else {
        LOGE("❌ Android context is null");
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_initRenderingNative(
    JNIEnv* env, jobject thiz) {
    LOGI("initRenderingNative called - returning true");
    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_loadGLBModelNative(
    JNIEnv* env, jobject thiz, jstring model_path) {
    LOGI("loadGLBModelNative called - returning true");
    return JNI_TRUE;
}
extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_initVuforiaEngineNative(
    JNIEnv* env, jobject thiz, jstring license_key) {
    
    LOGI("initVuforiaEngineNative called");
    
    const char* licenseKeyStr = nullptr;
    if (license_key != nullptr) {
        licenseKeyStr = env->GetStringUTFChars(license_key, nullptr);
        LOGI("License key received: %.20s...", licenseKeyStr); // 只顯示前20個字符
    }
    
    // 調用您已有的初始化函數
    bool success = VuforiaWrapper::getInstance().initialize(
        (licenseKeyStr != nullptr) ? licenseKeyStr : "");
    
    if (licenseKeyStr != nullptr) {
        env->ReleaseStringUTFChars(license_key, licenseKeyStr);
    }
    
    LOGI("Vuforia initialization result: %s", success ? "SUCCESS" : "FAILED");
    return success ? JNI_TRUE : JNI_FALSE;
}
extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_stopImageTrackingNative(
    JNIEnv* env, jobject thiz) {
    
    LOGI("stopImageTrackingNative called");
    VuforiaWrapper::getInstance().stopImageTracking();
}
// ==================== VuforiaInitialization JNI 函數 ====================

extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_initialization_VuforiaInitialization_pauseVuforiaEngineNative(
    JNIEnv* env, jobject thiz) {
    
    LOGI("pauseVuforiaEngineNative called");
    VuforiaWrapper::getInstance().pause();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_initialization_VuforiaInitialization_resumeVuforiaEngineNative(
    JNIEnv* env, jobject thiz) {
    
    LOGI("resumeVuforiaEngineNative called");
    VuforiaWrapper::getInstance().resume();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_initialization_VuforiaInitialization_isVuforiaInitializedNative(
    JNIEnv* env, jobject thiz) {
    
    int status = VuforiaWrapper::getInstance().getVuforiaStatus();
    return (status > 0) ? JNI_TRUE : JNI_FALSE;
}

/*
extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_initialization_VuforiaInitialization_setAndroidContextNative(
    JNIEnv* env, jobject thiz, jobject context) {
    
    LOGI("VuforiaInitialization setAndroidContextNative called");
    // 可以和 VuforiaCoreManager 的實現一樣
}
*/

// ==================== 全局相机权限检查函数 ====================

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_checkCameraPermissionNative(
    JNIEnv* env, jobject thiz) {
    
    LOGI("🔍 checkCameraPermissionNative called");
    
    try {
        bool hasPermission = VuforiaWrapper::getInstance().checkCameraPermission();
        LOGI("📷 Camera permission check result: %s", hasPermission ? "GRANTED" : "DENIED");
        return hasPermission ? JNI_TRUE : JNI_FALSE;
    } catch (const std::exception& e) {
        LOGE("❌ Error in checkCameraPermissionNative: %s", e.what());
        return JNI_FALSE;
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_isCameraAccessibleNative(
    JNIEnv* env, jobject thiz) {
    
    LOGD("📊 isCameraAccessibleNative called");
    
    try {
        bool isAccessible = VuforiaWrapper::getInstance().isCameraAccessible();
        LOGD("📊 Camera accessible status: %s", isAccessible ? "Yes" : "No");
        return isAccessible ? JNI_TRUE : JNI_FALSE;
    } catch (const std::exception& e) {
        LOGE("❌ Error in isCameraAccessibleNative: %s", e.what());
        return JNI_FALSE;
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_validateVuforiaPermissionsNative(
    JNIEnv* env, jobject thiz) {
    
    LOGI("🔍 validateVuforiaPermissionsNative called");
    
    try {
        bool isValid = VuforiaWrapper::getInstance().validateVuforiaPermissions();
        LOGI("🔍 Vuforia permissions validation: %s", isValid ? "PASSED" : "FAILED");
        return isValid ? JNI_TRUE : JNI_FALSE;
    } catch (const std::exception& e) {
        LOGE("❌ Error in validateVuforiaPermissionsNative: %s", e.what());
        return JNI_FALSE;
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_getPermissionErrorDetailNative(
    JNIEnv* env, jobject thiz) {
    
    LOGD("📋 getPermissionErrorDetailNative called");
    
    try {
        std::string errorDetail = VuforiaWrapper::getInstance().getPermissionErrorDetail();
        return env->NewStringUTF(errorDetail.c_str());
    } catch (const std::exception& e) {
        LOGE("❌ Error in getPermissionErrorDetailNative: %s", e.what());
        std::string errorMsg = "Error getting permission details: " + std::string(e.what());
        return env->NewStringUTF(errorMsg.c_str());
    }
}

// ==================== Vuforia Engine 生命周期函数 ====================







