#ifndef VUFORIA_RENDERING_JNI_H
#define VUFORIA_RENDERING_JNI_H
//C:\Users\USER\Desktop\IBM-WEATHER-ART-ANDRIOD\app\src\main\cpp\VuforiaRenderingJNI.h
// ==================== 标准依赖 ====================
#include <jni.h>
#include <android/log.h>

// ==================== 日志宏 ====================
#ifndef LOG_TAG
#define LOG_TAG "VuforiaRenderingJNI"
#endif

#define LOGI_RENDER(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE_RENDER(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD_RENDER(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGW_RENDER(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

// ==================== JNI方法声明 ====================
#ifdef __cplusplus
extern "C" {
#endif

// ==================== 渲染循环控制 ====================
/**
 * 停止渲染循环 - 解决编译错误的关键方法
 */
JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_stopRenderingLoopNative(
    JNIEnv* env, jobject thiz);

/**
 * 启动渲染循环
 */
JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_startRenderingLoopNative(
    JNIEnv* env, jobject thiz);

/**
 * 检查渲染循环是否活跃
 */
JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_isRenderingActiveNative(
    JNIEnv* env, jobject thiz);

// ==================== 相机控制 ====================
/**
 * 启动相机
 */
JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_startCameraNative(
    JNIEnv* env, jobject thiz);

/**
 * 停止相机
 */
JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_stopCameraNative(
    JNIEnv* env, jobject thiz);

/**
 * 检查相机是否活跃
 */
JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_isCameraActiveNative(
    JNIEnv* env, jobject thiz);

// ==================== Surface管理 ====================
/**
 * 设置渲染Surface
 */
JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_setSurfaceNative(
    JNIEnv* env, jobject thiz, jobject surface);

/**
 * Surface创建回调
 */
JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_onSurfaceCreatedNative(
    JNIEnv* env, jobject thiz, jint width, jint height);

/**
 * Surface销毁回调
 */
JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_onSurfaceDestroyedNative(
    JNIEnv* env, jobject thiz);

// ==================== 引擎状态查询 ====================
/**
 * 检查Vuforia引擎是否运行
 */
JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_isVuforiaEngineRunningNative(
    JNIEnv* env, jobject thiz);

// ==================== 诊断和调试 ====================
/**
 * 获取详细的引擎状态
 */
JNIEXPORT jstring JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_getEngineStatusDetailNative(
    JNIEnv* env, jobject thiz);

/**
 * 获取内存使用情况
 */
JNIEXPORT jstring JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_getMemoryUsageNative(
    JNIEnv* env, jobject thiz);

// ==================== 安全的图像追踪控制 ====================
/**
 * 安全地停止图像追踪
 */
JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_stopImageTrackingNativeSafe(
    JNIEnv* env, jobject thiz);
// ==================== 引擎生命周期控制 ====================
/**
 * 暂停Vuforia引擎 - 修复缺少的JNI函数
 */
JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_pauseVuforiaEngineNative(
    JNIEnv* env, jobject thiz);

/**
 * 恢复Vuforia引擎
 */
JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_resumeVuforiaEngineNative(
    JNIEnv* env, jobject thiz);

/**
 * 启动Vuforia引擎 - 解决引擎不启动的问题
 */
JNIEXPORT jboolean JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_startVuforiaEngineNative(
    JNIEnv* env, jobject thiz);

/**
 * 停止Vuforia引擎
 */
JNIEXPORT void JNICALL
Java_com_example_ibm_1ai_1weather_1art_1android_VuforiaCoreManager_stopVuforiaEngineNative(
    JNIEnv* env, jobject thiz);
#ifdef __cplusplus
}
#endif

#endif // VUFORIA_RENDERING_JNI_H