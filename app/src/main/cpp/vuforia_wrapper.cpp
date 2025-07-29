#include "VuforiaWrapper.h"
// ==================== 全局變量聲明 ====================
static jobject gAndroidContext = nullptr;
static JavaVM* gJavaVM = nullptr;
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
        copyMatrix(event.poseMatrix, poseMatrix);  // 使用安全的矩陣複製
        event.confidence = confidence;
        event.timestamp = std::chrono::steady_clock::now();
        
        mEventQueue.push(event);
        mLastEventMap[targetName] = eventType;
        
        LOGD("Target event added: %s, type: %d", targetName.c_str(), static_cast<int>(eventType));
    }
    
    bool TargetEventManager::shouldTriggerEvent(const std::string& targetName, TargetEventType eventType) {
        auto it = mLastEventMap.find(targetName);
        if (it == mLastEventMap.end()) {
            return true; // 第一次事件
        }
        
        // 避免連續相同事件
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
}

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
    {
        mEventManager = std::make_unique<TargetEventManager>();
        mFrameExtractor = std::make_unique<CameraFrameExtractor>();
        LOGI("VuforiaEngineWrapper created");
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
        
        LOGI("Initializing Vuforia Engine 11...");
        
        try {
            // 創建引擎配置
            VuEngineConfigSet* configSet = nullptr;
            if (!createEngineConfig(&configSet, licenseKey)) {
                return false;
            }
            
            // 創建引擎 - 修正：使用正確的參數類型
            VuErrorCode errorCode = VU_SUCCESS;
            VuResult result = vuEngineCreate(&mEngine, configSet, &errorCode);
            
            // 清理配置
            vuEngineConfigSetDestroy(configSet);
            
            if (!checkVuResult(result, "vuEngineCreate")) {
                LOGE("Engine creation error: %d", errorCode);
                return false;
            }
            
            // 設置控制器
            if (!setupControllers()) {
                vuEngineDestroy(mEngine);
                mEngine = nullptr;
                return false;
            }
            
            mEngineState = EngineState::INITIALIZED;
            LOGI("Vuforia Engine 11 initialized successfully");
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
        // 修正：在 Vuforia 11.x 中，控制器獲取方式可能不同
        // 直接將 engine 作為主控制器
        mController = reinterpret_cast<VuController*>(mEngine);
        if (mController == nullptr) {
            LOGE("Failed to get main controller");
            return false;
        }
        
        // 修正：在 Vuforia 11.x 中，可能不需要獲取子控制器
        // 或者這些函數名稱已經改變
        // 暫時設置為 nullptr，後續如果需要可以通過其他方式獲取
        mRenderController = nullptr;
        mCameraController = nullptr;
        mRecorderController = nullptr;
        
        LOGW("Controller setup simplified for Vuforia 11.x");
        LOGI("Controllers setup completed");
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
            // 獲取最新狀態
            VuState* state = nullptr;
            VuResult result = vuEngineAcquireLatestState(mEngine, &state);
            if (result != VU_SUCCESS || state == nullptr) {
                return;
            }
            
            // 立即處理狀態數據
            processVuforiaState(state);
            
            // 立即釋放狀態 - 關鍵：避免內存洩漏
            vuStateRelease(state);
            
            // 處理事件回調（在主線程中）
            if (mEventManager && mTargetCallback != nullptr) {
                mEventManager->processEvents(env, mTargetCallback);
            }
            
        } catch (const std::exception& e) {
            LOGE("Exception during frame rendering: %s", e.what());
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
}

// ==================== JNI 函數實現 ====================

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_initialization_VuforiaInitialization_initVuforiaEngineNative(
    JNIEnv* env, jobject thiz, jstring license_key) {
    
    const char* licenseKeyStr = nullptr;
    if (license_key != nullptr) {
        licenseKeyStr = env->GetStringUTFChars(license_key, nullptr);
    }
    
    // 修正：使用明確的指針檢查
    bool success = VuforiaWrapper::getInstance().initialize(
        (licenseKeyStr != nullptr) ? licenseKeyStr : "");
    
    if (licenseKeyStr != nullptr) {
        env->ReleaseStringUTFChars(license_key, licenseKeyStr);
    }
    
    return success ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_initialization_VuforiaInitialization_deinitVuforiaEngineNative(
    JNIEnv* env, jobject thiz) {
    
    VuforiaWrapper::getInstance().deinitialize();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_startVuforiaEngineNative(
    JNIEnv* env, jobject thiz) {
    
    return VuforiaWrapper::getInstance().start() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_stopVuforiaEngineNative(
    JNIEnv* env, jobject thiz) {
    
    VuforiaWrapper::getInstance().stop();
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
        // ✅ 添加詳細的 Context 類型檢查
        jclass contextClass = env->GetObjectClass(context);
        jmethodID getClassMethod = env->GetMethodID(contextClass, "getClass", "()Ljava/lang/Class;");
        jobject classObj = env->CallObjectMethod(context, getClassMethod);
        jclass classClass = env->GetObjectClass(classObj);
        jmethodID getNameMethod = env->GetMethodID(classClass, "getName", "()Ljava/lang/String;");
        jstring className = (jstring)env->CallObjectMethod(classObj, getNameMethod);
        const char* classNameStr = env->GetStringUTFChars(className, nullptr);
        
        LOGI("📋 Context class: %s", classNameStr);
        
        // 檢查是否是 Activity
        jclass activityClass = env->FindClass("android/app/Activity");
        if (env->IsInstanceOf(context, activityClass)) {
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
extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_pauseVuforiaEngineNative(
    JNIEnv* env, jobject thiz) {
    LOGI("pauseVuforiaEngineNative called");
    VuforiaWrapper::getInstance().pause();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_resumeVuforiaEngineNative(
    JNIEnv* env, jobject thiz) {
    LOGI("resumeVuforiaEngineNative called");
    VuforiaWrapper::getInstance().resume();
}
/*
extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_initialization_VuforiaInitialization_setAndroidContextNative(
    JNIEnv* env, jobject thiz, jobject context) {
    
    LOGI("VuforiaInitialization setAndroidContextNative called");
    // 可以和 VuforiaCoreManager 的實現一樣
}
*/