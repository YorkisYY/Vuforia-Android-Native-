// ==================== 在 VuforiaEngineWrapper 类的 private 部分添加 ====================

private:
    // ===== 现有的 private 成员保持不变 =====
    // ... 您现有的所有 private 成员 ...
    
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

// ==================== 在 VuforiaEngineWrapper 类的 public 部分添加 ====================

public:
    // ===== 您现有的所有 public 方法保持不变 =====
    // ... 现有方法 ...
    
    // ===== 新增的渲染相关公开方法 =====
    
    // OpenGL资源管理
    bool initializeOpenGLResources();
    void cleanupOpenGLResources();
    bool isOpenGLInitialized() const;
    bool validateOpenGLSetup() const;
    std::string getOpenGLInfo() const;
    
    // 视频背景渲染
    bool setupVideoBackgroundRendering();
    void renderFrameWithVideoBackground(JNIEnv* env);
    bool createVideoBackgroundShader();
    bool setupVideoBackgroundTexture();
    void renderVideoBackgroundWithProperShader(const VuRenderState& renderState);
    
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

// ==================== 在 VuforiaEngineWrapper 类的 private 部分添加新的私有方法 ====================

private:
    // ===== 您现有的所有 private 方法保持不变 =====
    // ... 现有的私有方法 ...
    
    // ===== 新增的渲染相关私有方法 =====
    
    // 性能统计更新
    void updatePerformanceStats();
    
    // 渲染状态调试
    void debugRenderState(const VuRenderState& renderState) const;

// ==================== 需要修改的构造函数初始化列表 ====================

// 在您的 VuforiaEngineWrapper 构造函数中，需要添加这些新成员的初始化：
/*
VuforiaEngineWrapper::VuforiaEngineWrapper() 
    : // ... 您现有的初始化列表 ...
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
    // 您现有的构造函数代码...
    
    // 初始化性能统计
    mLastFrameTime = std::chrono::steady_clock::now();
    
    // 初始化OpenGL状态结构
    memset(&mSavedGLState, 0, sizeof(mSavedGLState));
    
    LOGI("VuforiaEngineWrapper created with enhanced rendering support");
}
*/

// ==================== 需要在头文件顶部添加的包含 ====================

// 在您的 #include 列表中添加（如果还没有的话）：
/*
#include <GLES3/gl3.h>       // OpenGL ES 3.0
#include <GLES2/gl2ext.h>    // OpenGL扩展
#include <iomanip>           // 用于 std::setprecision
*/