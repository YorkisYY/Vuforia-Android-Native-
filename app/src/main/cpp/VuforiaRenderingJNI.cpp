// ==================== VuforiaRenderingJNI.cpp ====================
// 专门处理Vuforia渲染相关的JNI方法
// 解决 stopRenderingLoop() 编译错误以及相关渲染功能
// 集成完整的Vuforia 11.3.4渲染实现
//C:\Users\USER\Desktop\IBM-WEATHER-ART-ANDRIOD\app\src\main\cpp\VuforiaRenderingJNI.cpp

#include "VuforiaRenderingJNI.h"
#include "VuforiaWrapper.h"  // 引用主要的Wrapper类     
#include <GLES2/gl2ext.h>    // OpenGL扩展
#include <jni.h>
#include <android/log.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <mutex>
#include <chrono>
#include <memory>
#include <string>
#include <cstring>
#include <mutex>
#include <chrono>
#include <memory>
#include <string>
// ==================== 渲染模块内部状态管理 ====================
namespace VuforiaRendering {
    
    // 渲染状态结构
    struct RenderingState {
        // OpenGL资源
        bool initialized;
        GLuint videoBackgroundShaderProgram;
        GLuint videoBackgroundVAO;
        GLuint videoBackgroundVBO;
        GLuint videoBackgroundTextureId;
        
        // 性能监控
        std::chrono::steady_clock::time_point lastFrameTime;
        float currentFPS;
        long totalFrameCount;
        
        // 渲染配置
        bool videoBackgroundRenderingEnabled;
        int renderingQuality;
        
        // OpenGL状态保存
        struct {
            GLboolean depthTestEnabled;
            GLboolean cullFaceEnabled;
            GLboolean blendEnabled;
            GLenum blendSrcFactor;
            GLenum blendDstFactor;
            GLuint currentProgram;
            GLuint currentTexture;
        } savedGLState;
        
        // ✅ 修正：浮點數後綴大寫
        RenderingState() : initialized(false), videoBackgroundShaderProgram(0),
                        videoBackgroundVAO(0), videoBackgroundVBO(0),
                        videoBackgroundTextureId(0), currentFPS(0.0F),
                        totalFrameCount(0), videoBackgroundRenderingEnabled(true),
                        renderingQuality(1) {
            lastFrameTime = std::chrono::steady_clock::now();
            memset(&savedGLState, 0, sizeof(savedGLState));
        }
    };
}
    // 全局渲染状态
    static VuforiaRendering::RenderingState g_renderingState;
    static std::mutex g_renderingMutex;
namespace VuforiaRendering {
    
    // 性能統計更新函數 - 修正編譯錯誤
    void updatePerformanceStats() {
        try {
            auto currentTime = std::chrono::steady_clock::now();
            auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTime - g_renderingState.lastFrameTime).count();
            
            if (timeDiff > 0) {
                g_renderingState.currentFPS = 1000.0F / static_cast<float>(timeDiff);
            }
            
            g_renderingState.lastFrameTime = currentTime;
            g_renderingState.totalFrameCount++;
            
            // 每100幀記錄一次性能信息
            if (g_renderingState.totalFrameCount % 100 == 0) {
                LOGD_RENDER("📊 Performance: FPS=%.2f, Frames=%ld", 
                           g_renderingState.currentFPS, g_renderingState.totalFrameCount);
            }
        } catch (const std::exception& e) {
            LOGE_RENDER("❌ Error updating performance stats: %s", e.what());
        }
    }

    // 創建視頻背景著色器 - 完全修正版本
    bool createVideoBackgroundShader() {
        LOGI_RENDER("🎨 Creating video background shader program...");
        
        // 顶点着色器源码 - 适用于 Vuforia 11.3.4
        const char* vertexShaderSource = R"(
            #version 300 es
            precision highp float;
            
            layout(location = 0) in vec3 a_position;
            layout(location = 1) in vec2 a_texCoord;
            
            uniform mat4 u_projectionMatrix;
            uniform mat4 u_modelViewMatrix;
            
            out vec2 v_texCoord;
            
            void main() {
                gl_Position = u_projectionMatrix * u_modelViewMatrix * vec4(a_position, 1.0);
                v_texCoord = a_texCoord;
            }
        )";
        
        // 片段着色器源码 - 支援相机纹理
        const char* fragmentShaderSource = R"(
            #version 300 es
            #extension GL_OES_EGL_image_external_essl3 : require
            precision highp float;
            
            in vec2 v_texCoord;
            uniform samplerExternalOES u_cameraTexture;
            uniform float u_alpha;
            
            out vec4 fragColor;
            
            void main() {
                vec4 cameraColor = texture(u_cameraTexture, v_texCoord);
                fragColor = vec4(cameraColor.rgb, cameraColor.a * u_alpha);
            }
        )";
        
        // 编译顶点着色器
        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
        glCompileShader(vertexShader);
        
        GLint vertexCompileStatus;
        glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &vertexCompileStatus);
        if (vertexCompileStatus != GL_TRUE) {
            const int LOG_BUFFER_SIZE = 512;
            GLchar infoLog[LOG_BUFFER_SIZE];
            glGetShaderInfoLog(vertexShader, sizeof(infoLog), nullptr, infoLog);
            LOGE_RENDER("❌ Vertex shader compilation failed: %s", infoLog);
            glDeleteShader(vertexShader);
            return false;
        }
        
        // 编译片段着色器
        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
        glCompileShader(fragmentShader);
        
        GLint fragmentCompileStatus;
        glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &fragmentCompileStatus);
        if (fragmentCompileStatus != GL_TRUE) {
            const int LOG_BUFFER_SIZE = 512;
            GLchar infoLog[LOG_BUFFER_SIZE];
            glGetShaderInfoLog(fragmentShader, sizeof(infoLog), nullptr, infoLog);
            LOGE_RENDER("❌ Fragment shader compilation failed: %s", infoLog);
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            return false;
        }
        
        // 创建着色器程序
        g_renderingState.videoBackgroundShaderProgram = glCreateProgram();
        glAttachShader(g_renderingState.videoBackgroundShaderProgram, vertexShader);
        glAttachShader(g_renderingState.videoBackgroundShaderProgram, fragmentShader);
        glLinkProgram(g_renderingState.videoBackgroundShaderProgram);
        
        GLint linkStatus;
        glGetProgramiv(g_renderingState.videoBackgroundShaderProgram, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE) {
            const int LOG_BUFFER_SIZE = 512;
            GLchar infoLog[LOG_BUFFER_SIZE];
            glGetProgramInfoLog(g_renderingState.videoBackgroundShaderProgram, sizeof(infoLog), nullptr, infoLog);
            LOGE_RENDER("❌ Shader program linking failed: %s", infoLog);
            
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            glDeleteProgram(g_renderingState.videoBackgroundShaderProgram);
            g_renderingState.videoBackgroundShaderProgram = 0;
            return false;
        }
        
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        
        LOGI_RENDER("✅ Video background shader program created successfully (ID: %d)", 
                g_renderingState.videoBackgroundShaderProgram);
        return true;
    }
    
    // 設置視頻背景紋理 - 完全修正版本
    bool setupVideoBackgroundTexture() {
        LOGI_RENDER("📷 Setting up video background texture...");
        
        glGenTextures(1, &g_renderingState.videoBackgroundTextureId);
        if (g_renderingState.videoBackgroundTextureId == 0) {
            LOGE_RENDER("❌ Failed to generate texture ID");
            return false;
        }
        
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, g_renderingState.videoBackgroundTextureId);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
        
        LOGI_RENDER("✅ Video background texture setup complete (ID: %d)", 
                   g_renderingState.videoBackgroundTextureId);
        return true;
    }
    
    // 渲染視頻背景 - 完全修正版本
    void renderVideoBackgroundWithProperShader(const VuRenderState& renderState) {
        if (!g_renderingState.initialized || g_renderingState.videoBackgroundShaderProgram == 0) {
            LOGW_RENDER("⚠️ Rendering not initialized");
            return;
        }
        
        try {
            if (renderState.vbMesh == nullptr) {
                LOGW_RENDER("⚠️ vbMesh is null - skipping video background rendering");
                return;
            }
            
            if (renderState.vbMesh->numVertices <= 0) {
                LOGW_RENDER("⚠️ No vertices in vbMesh - skipping video background rendering");
                return;
            }
            
            if (renderState.vbMesh->pos == nullptr) {
                LOGW_RENDER("⚠️ Position data is null - skipping video background rendering");
                return;
            }
            
            LOGD_RENDER("🎥 Rendering video background with %d vertices", renderState.vbMesh->numVertices);
            
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
            
            // 激活並綁定相机紋理
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_EXTERNAL_OES, g_renderingState.videoBackgroundTextureId);
            GLint textureLocation = glGetUniformLocation(g_renderingState.videoBackgroundShaderProgram, "u_cameraTexture");
            if (textureLocation != -1) {
                glUniform1i(textureLocation, 0);
            }
            
            // 設置頂點屬性
            GLint positionAttribute = glGetAttribLocation(g_renderingState.videoBackgroundShaderProgram, "a_position");
            GLint texCoordAttribute = glGetAttribLocation(g_renderingState.videoBackgroundShaderProgram, "a_texCoord");
            
            if (positionAttribute != -1 && renderState.vbMesh->pos != nullptr) {
                glEnableVertexAttribArray(positionAttribute);
                glVertexAttribPointer(positionAttribute, 3, GL_FLOAT, GL_FALSE, 0, renderState.vbMesh->pos);
            }
            
            if (texCoordAttribute != -1 && renderState.vbMesh->tex != nullptr) {
                glEnableVertexAttribArray(texCoordAttribute);
                glVertexAttribPointer(texCoordAttribute, 2, GL_FLOAT, GL_FALSE, 0, renderState.vbMesh->tex);
            }
            
            if (renderState.vbMesh->faceIndices != nullptr && renderState.vbMesh->numFaces > 0) {
                glDrawElements(GL_TRIANGLES, renderState.vbMesh->numFaces * 3, GL_UNSIGNED_INT, renderState.vbMesh->faceIndices);
                LOGD_RENDER("✅ Video background rendered with %d faces (%d indices)", 
                        renderState.vbMesh->numFaces, renderState.vbMesh->numFaces * 3);
            } else if (renderState.vbMesh->numVertices > 0) {
                glDrawArrays(GL_TRIANGLES, 0, renderState.vbMesh->numVertices);
                LOGD_RENDER("✅ Video background rendered with %d vertices", renderState.vbMesh->numVertices);
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

    // 調試渲染狀態 - 完全修正版本
    void debugRenderState(const VuRenderState& renderState) {
        LOGD_RENDER("🔍 Render State Debug Info:");
        
        if (renderState.vbMesh != nullptr) {
            LOGD_RENDER("   vbMesh.numVertices: %d", renderState.vbMesh->numVertices);
            LOGD_RENDER("   vbMesh.numFaces: %d", renderState.vbMesh->numFaces);
            LOGD_RENDER("   vbMesh.pos: %p", renderState.vbMesh->pos);
            LOGD_RENDER("   vbMesh.tex: %p", renderState.vbMesh->tex);
            LOGD_RENDER("   vbMesh.normal: %p", renderState.vbMesh->normal);
            LOGD_RENDER("   vbMesh.faceIndices: %p", renderState.vbMesh->faceIndices);
            
            if (renderState.vbMesh->pos != nullptr && renderState.vbMesh->numVertices >= 1) {
                LOGD_RENDER("   First vertex position: (%.3f, %.3f, %.3f)", 
                        renderState.vbMesh->pos[0],
                        renderState.vbMesh->pos[1],
                        renderState.vbMesh->pos[2]);
            }
            
            if (renderState.vbMesh->tex != nullptr && renderState.vbMesh->numVertices >= 1) {
                LOGD_RENDER("   First vertex texCoord: (%.3f, %.3f)", 
                        renderState.vbMesh->tex[0],
                        renderState.vbMesh->tex[1]);
            }
            
            LOGD_RENDER("   Total vertices: %d", renderState.vbMesh->numVertices);
            LOGD_RENDER("   Total faces: %d (indices: %d)", renderState.vbMesh->numFaces, renderState.vbMesh->numFaces * 3);
        } else {
            LOGD_RENDER("❌ vbMesh is null");
        }
    }


// ==================== JNI 实现 - 使用内部渲染状态 ====================

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_initializeOpenGLResourcesNative(
    JNIEnv* env, jobject thiz) {
    
    LOGI_RENDER("🎨 initializeOpenGLResourcesNative called - Vuforia 11.3.4");
    
    std::lock_guard<std::mutex> lock(g_renderingMutex);  // ✅ 修正：直接使用變數名
    
    try {
        if (g_renderingState.initialized) {  // ✅ 修正：直接使用變數名
            LOGW_RENDER("OpenGL resources already initialized");
            return JNI_TRUE;
        }
        
        // 检查 OpenGL 版本
        const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
        const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        
        LOGI_RENDER("📱 OpenGL Info:");
        LOGI_RENDER("   Version: %s", version ? version : "Unknown");
        LOGI_RENDER("   Vendor: %s", vendor ? vendor : "Unknown");
        LOGI_RENDER("   Renderer: %s", renderer ? renderer : "Unknown");
        
        // 创建着色器和纹理
        if (!createVideoBackgroundShader()) {
            LOGE_RENDER("❌ Failed to create video background shader");
            return JNI_FALSE;
        }
        
        if (!setupVideoBackgroundTexture()) {
            LOGE_RENDER("❌ Failed to setup video background texture");
            return JNI_FALSE;
        }
        
        // 创建缓冲区
        glGenBuffers(1, &g_renderingState.videoBackgroundVBO);  // ✅ 修正：直接使用變數名
        if (g_renderingState.videoBackgroundVBO == 0) {  // ✅ 修正：直接使用變數名
            LOGE_RENDER("❌ Failed to generate VBO");
            return JNI_FALSE;
        }
        
        // 创建VAO（如果支持）
        glGenVertexArrays(1, &g_renderingState.videoBackgroundVAO);  // ✅ 修正：直接使用變數名
        if (g_renderingState.videoBackgroundVAO == 0) {  // ✅ 修正：直接使用變數名
            LOGW_RENDER("⚠️ VAO not supported, using direct vertex attribute setup");
        }
        
        g_renderingState.initialized = true;  // ✅ 修正：直接使用變數名
        LOGI_RENDER("✅ OpenGL resources initialized successfully");
        return JNI_TRUE;
        
    } catch (const std::exception& e) {
        LOGE_RENDER("❌ Exception in initializeOpenGLResourcesNative: %s", e.what());
        return JNI_FALSE;
    }
}


extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_renderFrameWithVideoBackgroundNative(
    JNIEnv* env, jobject thiz) {
    
    std::lock_guard<std::mutex> lock(g_renderingMutex);  // ✅ 修正：直接使用變數名
    
    if (!g_renderingState.initialized) {  // ✅ 修正：直接使用變數名
        return;
    }
    
    try {
        // ✅ 修正：使用公共方法獲取引擎
        VuEngine* engine = VuforiaWrapper::getInstance().getEngine();
        if (engine == nullptr) {
            return;
        }
        
        // 獲取Vuforia狀態
        VuState* state = nullptr;
        VuResult result = vuEngineAcquireLatestState(engine, &state);
        if (result != VU_SUCCESS || state == nullptr) {
            return;
        }
        
        // 獲取渲染狀態
        VuRenderState renderState;
        result = vuStateGetRenderState(state, &renderState);
        if (result != VU_SUCCESS) {
            vuStateRelease(state);
            return;
        }
        
        // 更新性能統計
        updatePerformanceStats();
        
        // 清除緩衝區
        glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // ✅ 修正：使用正確的條件檢查
        if (g_renderingState.videoBackgroundRenderingEnabled &&  // ✅ 修正：直接使用變數名
            renderState.vbMesh != nullptr &&
            renderState.vbMesh->numVertices > 0) {
            renderVideoBackgroundWithProperShader(renderState);
        }
        
        // 釋放狀態
        vuStateRelease(state);
        
    } catch (const std::exception& e) {
        LOGE_RENDER("❌ Exception in renderFrameWithVideoBackgroundNative: %s", e.what());
    }
}
// ==================== 其他JNI实现 ====================


extern "C" JNIEXPORT jlong JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_getFrameCountNative(
    JNIEnv* env, jobject thiz) {
    
    std::lock_guard<std::mutex> lock(g_renderingMutex);  // ✅ 修正：直接使用變數名
    return static_cast<jlong>(g_renderingState.totalFrameCount);  // ✅ 修正：直接使用變數名
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_debugRenderStateNative(
    JNIEnv* env, jobject thiz) {
    
    LOGD_RENDER("🔍 debugRenderStateNative called");
    
    std::lock_guard<std::mutex> lock(g_renderingMutex);  // ✅ 修正：直接使用變數名
    
    LOGD_RENDER("=== Rendering State Debug ===");
    LOGD_RENDER("Initialized: %s", g_renderingState.initialized ? "Yes" : "No");  // ✅ 修正：直接使用變數名
    LOGD_RENDER("Shader: %u", g_renderingState.videoBackgroundShaderProgram);  // ✅ 修正：直接使用變數名
    LOGD_RENDER("Texture: %u", g_renderingState.videoBackgroundTextureId);  // ✅ 修正：直接使用變數名
    LOGD_RENDER("VBO: %u", g_renderingState.videoBackgroundVBO);  // ✅ 修正：直接使用變數名
    LOGD_RENDER("VAO: %u", g_renderingState.videoBackgroundVAO);  // ✅ 修正：直接使用變數名
    LOGD_RENDER("FPS: %.2f", g_renderingState.currentFPS);  // ✅ 修正：直接使用變數名
    LOGD_RENDER("Frames: %ld", g_renderingState.totalFrameCount);  // ✅ 修正：直接使用變數名
}

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
        std::string info = "OpenGL ES 3.0 - Vuforia Rendering Module";
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
        if (!g_renderingState.initialized) {
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
        // ✅ 修正：浮點數後綴大寫
        return 0.0F;
    } catch (...) {
        LOGE_RENDER("❌ Unknown error in getCurrentFPSNative");
        // ✅ 修正：浮點數後綴大寫
        return 0.0F;
    }
}


// ==================== 高级渲染配置实现 ====================

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

}