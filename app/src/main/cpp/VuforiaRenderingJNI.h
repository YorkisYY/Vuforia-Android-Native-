#ifndef VUFORIA_RENDERING_JNI_H
#define VUFORIA_RENDERING_JNI_H

// ========== 必要的系統頭文件 ==========
#include <jni.h>
#include <android/log.h>
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h> 
#include "VuforiaEngine/VuforiaEngine.h"
// C++ 標準庫
#include <mutex>
#include <chrono>
#include <string>
#include <vector>
#include <cstring>
#ifndef GL_TEXTURE_EXTERNAL_OES
#define GL_TEXTURE_EXTERNAL_OES 0x8D65
#endif

// ========== 日誌宏定義 ==========
#ifndef LOG_TAG
#define LOG_TAG "VuforiaRenderingJNI"
#endif
#define LOGI_RENDER(...) __android_log_print(ANDROID_LOG_INFO, "VuforiaRender", __VA_ARGS__)
#define LOGE_RENDER(...) __android_log_print(ANDROID_LOG_ERROR, "VuforiaRender", __VA_ARGS__)
#define LOGD_RENDER(...) __android_log_print(ANDROID_LOG_DEBUG, "VuforiaRender", __VA_ARGS__)
#define LOGW_RENDER(...) __android_log_print(ANDROID_LOG_WARN, "VuforiaRender", __VA_ARGS__)
// ========== 常量定義 ==========
namespace VuforiaRendering {
    // OpenGL ES 2.0 相關常量
    static const int SHADER_INFO_LOG_SIZE = 512;
    static const int MATRIX_SIZE = 16;
    static const float NEAR_PLANE = 2.0f;
    static const float FAR_PLANE = 2000.0f;
    
    // 渲染品質設定
    enum RenderQuality {
        QUALITY_LOW = 0,
        QUALITY_MEDIUM = 1,
        QUALITY_HIGH = 2
    };
}

// ========== VuforiaRenderingJNI 類定義 ==========
class VuforiaRenderingJNI {
private:
    // ===== 渲染資源管理 =====
    mutable std::mutex mRenderMutex;
    bool mRenderInitialized;
    
    // OpenGL ES 2.0 渲染資源
    GLuint mVideoBackgroundShaderProgram;
    GLuint mVideoBackgroundVAO;  // 注意：OpenGL ES 2.0 可能不支援 VAO
    GLuint mVideoBackgroundVBO;
    GLuint mVideoBackgroundTextureId;
    
    // 著色器句柄
    GLint mVertexHandle;
    GLint mTextureCoordHandle;
    GLint mMVPMatrixHandle;
    GLint mTexSamplerHandle;
    
    // ===== 性能監控 =====
    mutable std::mutex mPerformanceMutex;
    std::chrono::steady_clock::time_point mLastFrameTime;
    float mCurrentFPS;
    long mTotalFrameCount;
    
    // ===== 渲染配置 =====
    VuRenderConfig* mRenderConfig;  // Vuforia 11.x 使用指標
    bool mVideoBackgroundRenderingEnabled;
    VuforiaRendering::RenderQuality mRenderingQuality;
    
    // ===== OpenGL 狀態保存 (OpenGL ES 2.0) =====
    struct OpenGLState {
        GLboolean depthTestEnabled;
        GLboolean cullFaceEnabled;
        GLboolean blendEnabled;
        GLenum blendSrcFactor;
        GLenum blendDstFactor;
        GLuint currentProgram;
        GLuint currentTexture;
        GLint viewport[4];  // 視口參數
    } mSavedGLState;
    
    // ===== 視頻背景網格數據 (Vuforia 11.x 相容) =====
    struct VideoBackgroundMesh {
        std::vector<float> vertices;
        std::vector<float> texCoords;
        std::vector<unsigned short> indices;
        int numVertices;
        int numIndices;
    } mVideoBackgroundMeshData;

public:
    // ===== 構造函數和析構函數 =====
    VuforiaRenderingJNI();
    ~VuforiaRenderingJNI();
    
    // ===== OpenGL 資源管理 (OpenGL ES 2.0) =====
    bool initializeOpenGLResources();
    void cleanupOpenGLResources();
    bool validateOpenGLSetup() const;
    
    // ===== 著色器管理 =====
    bool createVideoBackgroundShader();
    GLuint compileShader(GLenum shaderType, const char* source);
    GLuint createShaderProgram(const char* vertexShader, const char* fragmentShader);
    void deleteShaderProgram();
    
    // ===== 視頻背景渲染 (Vuforia 11.x 相容) =====
    bool setupVideoBackgroundRendering();
    void renderFrameWithVideoBackground(JNIEnv* env);
    void renderVideoBackground(const VuRenderState* renderState);
    void renderVideoBackgroundMesh(const VuRenderState* renderState);
    
    // ===== 網格處理 (Vuforia 11.x VuMesh 相容) =====
    bool extractMeshData(const VuMesh* vuMesh);
    void renderSimpleQuad();  // 備用簡單四邊形渲染
    
    // ===== 性能監控 =====
    float getCurrentRenderingFPS() const;
    long getTotalFrameCount() const;
    void updatePerformanceStats();
    
    // ===== 渲染配置 =====
    void setVideoBackgroundRenderingEnabled(bool enabled);
    void setRenderingQuality(VuforiaRendering::RenderQuality quality);
    void onSurfaceChanged(int width, int height);
    
    // ===== OpenGL 狀態管理 =====
    void saveOpenGLState();
    void restoreOpenGLState();
    
    // ===== 調試和診斷 =====
    void debugCurrentRenderState();
    std::string getRenderingStatusDetail() const;
    void debugRenderState(const VuRenderState* renderState) const;
    std::string getOpenGLInfo() const;
    
    // ===== 錯誤處理 =====
    bool checkGLError(const char* operation) const;
    void logGLError(const char* operation, GLenum error) const;

private:
    // ===== 私有輔助方法 =====
    void initializeDefaultMesh();
    void updateRenderConfig();
    bool isOpenGLES2Supported() const;
    
    // ===== 著色器源碼 (OpenGL ES 2.0) =====
    static const char* getVertexShaderSource();
    static const char* getFragmentShaderSource();
    
    // ===== 內部渲染輔助 =====
    void setVertexAttribPointers();
    void bindVideoBackgroundTexture();
    void setupRenderingMatrices(const VuRenderState* renderState);
};

// ========== 全域函數聲明 (JNI 介面) ==========
extern "C" {
    // 這些函數將在 .cpp 檔案中實現
    JNIEXPORT jboolean JNICALL
    Java_com_yourpackage_VuforiaRenderer_initializeRendering(JNIEnv* env, jobject obj);
    
    JNIEXPORT void JNICALL
    Java_com_yourpackage_VuforiaRenderer_cleanupRendering(JNIEnv* env, jobject obj);
    
    JNIEXPORT void JNICALL
    Java_com_yourpackage_VuforiaRenderer_renderVideoBackground(JNIEnv* env, jobject obj);
    
    JNIEXPORT jfloat JNICALL
    Java_com_yourpackage_VuforiaRenderer_getCurrentFPS(JNIEnv* env, jobject obj);
}

// ========== 內聯函數定義 ==========
inline bool VuforiaRenderingJNI::checkGLError(const char* operation) const {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        logGLError(operation, error);
        return false;
    }
    return true;
}

inline void VuforiaRenderingJNI::logGLError(const char* operation, GLenum error) const {
    LOGE_RENDER("OpenGL error in %s: 0x%x", operation, error);
}

#endif // VUFORIA_RENDERING_JNI_H