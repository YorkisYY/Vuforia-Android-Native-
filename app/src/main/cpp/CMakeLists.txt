cmake_minimum_required(VERSION 3.22.1)
project("vuforia_wrapper")

# ==================== 編譯器和標準設置 ====================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ==================== 建構類型檢查 ====================
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Target ABI: ${ANDROID_ABI}")

# ==================== Vuforia 路徑配置 ====================
# Vuforia 頭文件路徑 - 根據你的實際結構
set(VUFORIA_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)
set(VUFORIA_ENGINE_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include/VuforiaEngine)

# Vuforia 庫文件路徑
set(VUFORIA_LIBRARY_DIR ${CMAKE_SOURCE_DIR}/../jniLibs/${ANDROID_ABI})

# 檢查路徑是否存在
if(NOT EXISTS ${VUFORIA_INCLUDE_DIR})
    message(FATAL_ERROR "Vuforia include directory not found: ${VUFORIA_INCLUDE_DIR}")
endif()

if(NOT EXISTS ${VUFORIA_ENGINE_INCLUDE_DIR}/VuforiaEngine.h)
    message(FATAL_ERROR "VuforiaEngine.h not found: ${VUFORIA_ENGINE_INCLUDE_DIR}/VuforiaEngine.h")
endif()

if(NOT EXISTS ${VUFORIA_LIBRARY_DIR}/libVuforia.so)
    message(FATAL_ERROR "Vuforia library not found: ${VUFORIA_LIBRARY_DIR}/libVuforia.so")
endif()

message(STATUS "Vuforia include dir: ${VUFORIA_INCLUDE_DIR}")
message(STATUS "Vuforia engine include dir: ${VUFORIA_ENGINE_INCLUDE_DIR}")
message(STATUS "Vuforia library dir: ${VUFORIA_LIBRARY_DIR}")

# ==================== 包含目錄設置 ====================
include_directories(
    ${VUFORIA_INCLUDE_DIR}
    ${VUFORIA_ENGINE_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}  # 為了包含我們的 VuforiaWrapper.h
)

# ==================== 源文件配置 ====================
set(VUFORIA_WRAPPER_SOURCES
    vuforia_wrapper.cpp
    # 如果你有其他源文件，在這裡添加
)

# ==================== 創建共享庫 ====================
add_library(vuforia_wrapper SHARED ${VUFORIA_WRAPPER_SOURCES})

# ==================== 導入 Vuforia 預編譯庫 ====================
add_library(Vuforia SHARED IMPORTED)
set_target_properties(Vuforia PROPERTIES
    IMPORTED_LOCATION ${VUFORIA_LIBRARY_DIR}/libVuforia.so
    INTERFACE_INCLUDE_DIRECTORIES ${VUFORIA_INCLUDE_DIR}
)

# ==================== 查找系統庫 ====================
find_library(log-lib log)
find_library(android-lib android)
find_library(gles2-lib GLESv2)
find_library(egl-lib EGL)
find_library(jnigraphics-lib jnigraphics)
find_library(z-lib z)

# 檢查關鍵庫是否找到
if(NOT log-lib)
    message(FATAL_ERROR "log library not found")
endif()

if(NOT android-lib)
    message(FATAL_ERROR "android library not found")
endif()

# ==================== 編譯定義 ====================
target_compile_definitions(vuforia_wrapper PRIVATE
    # Android 平台定義
    ANDROID
    __ANDROID__
    VU_PLATFORM_ANDROID  # 重要：讓 Vuforia 知道這是 Android 平台
    
    # OpenGL 相關定義
    GL_GLEXT_PROTOTYPES
    EGL_EGLEXT_PROTOTYPES
    
    # Vuforia 特定定義
    VU_ANDROID
    
    # C++ 相關定義
    _GNU_SOURCE
)

# ==================== 編譯選項配置 ====================
target_compile_options(vuforia_wrapper PRIVATE
    # 基本編譯選項
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-unused-variable
    
    # 性能優化
    -ffast-math
    -fvisibility=hidden
    
    # 調試信息（即使在 Release 模式下也保留一些調試信息）
    -g
    
    # ARM 特定優化
    $<$<STREQUAL:${ANDROID_ABI},armeabi-v7a>:-mfpu=neon>
    $<$<STREQUAL:${ANDROID_ABI},armeabi-v7a>:-marm>
    $<$<STREQUAL:${ANDROID_ABI},arm64-v8a>:-march=armv8-a>
)

# ==================== Debug 特定配置 ====================
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(vuforia_wrapper PRIVATE
        DEBUG
        _DEBUG
        VU_DEBUG  # Vuforia 調試定義
    )
    
    target_compile_options(vuforia_wrapper PRIVATE
        -O0
        -fno-omit-frame-pointer
        -fno-inline-functions
        -DDEBUG_LEVEL=2
    )
    
    message(STATUS "Debug mode: Optimization disabled, debug symbols enabled")
endif()

# ==================== Release 特定配置 ====================
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(vuforia_wrapper PRIVATE
        NDEBUG
        RELEASE
    )
    
    target_compile_options(vuforia_wrapper PRIVATE
        -O3
        -fomit-frame-pointer
        -ffunction-sections
        -fdata-sections
        -flto
    )
    
    # Release 模式下的鏈接器優化
    target_link_options(vuforia_wrapper PRIVATE
        -Wl,--gc-sections
        -Wl,--strip-all
        -flto
    )
    
    message(STATUS "Release mode: Full optimization enabled")
endif()

# ==================== 鏈接庫配置 ====================
target_link_libraries(vuforia_wrapper
    # Vuforia 庫（必須放在前面）
    Vuforia
    
    # Android 系統庫
    ${log-lib}
    ${android-lib}
    
    # OpenGL ES 相關
    ${gles2-lib}
    ${egl-lib}
    
    # JNI 相關
    ${jnigraphics-lib}
    
    # 其他系統庫
    ${z-lib}
)

# ==================== 特定架構優化 ====================
# ARM64 特定優化
if(ANDROID_ABI STREQUAL "arm64-v8a")
    target_compile_options(vuforia_wrapper PRIVATE
        -mcpu=cortex-a73
        -mtune=cortex-a73
    )
    message(STATUS "ARM64 optimizations enabled")
endif()

# ARMv7 特定優化
if(ANDROID_ABI STREQUAL "armeabi-v7a")
    target_compile_options(vuforia_wrapper PRIVATE
        -mcpu=cortex-a9
        -mtune=cortex-a9
        -mfloat-abi=softfp
    )
    message(STATUS "ARMv7 optimizations enabled")
endif()

# ==================== Vuforia 特定配置檢查 ====================
# 檢查 Vuforia 頭文件是否存在
set(REQUIRED_VUFORIA_HEADERS
    "VuforiaEngine/VuforiaEngine.h"
    "VuforiaEngine/Core/Basic.h"
    "VuforiaEngine/Engine/Engine.h"
    "VuforiaEngine/Controller/CameraController.h"
    "VuforiaEngine/Observer/ImageTargetObserver.h"
)

foreach(HEADER ${REQUIRED_VUFORIA_HEADERS})
    if(NOT EXISTS "${VUFORIA_INCLUDE_DIR}/${HEADER}")
        message(WARNING "Required Vuforia header not found: ${HEADER}")
    else()
        message(STATUS "Found Vuforia header: ${HEADER}")
    endif()
endforeach()

# ==================== 自定義構建目標 ====================
# 清理目標
add_custom_target(clean-vuforia
    COMMAND ${CMAKE_COMMAND} -E remove -f 
        ${CMAKE_BINARY_DIR}/libvuforia_wrapper.so
    COMMENT "Cleaning Vuforia wrapper library"
)

# 驗證目標 - 檢查生成的庫
add_custom_target(verify-build
    COMMAND ${CMAKE_COMMAND} -E echo "Verifying build..."
    COMMAND test -f ${CMAKE_BINARY_DIR}/libvuforia_wrapper.so
    COMMAND ${CMAKE_COMMAND} -E echo "Build verification passed"
    DEPENDS vuforia_wrapper
    COMMENT "Verifying that the library was built correctly"
)

# ==================== 安裝配置 ====================
# 設置安裝目錄
set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/../../../build/intermediates/cmake/${CMAKE_BUILD_TYPE}/obj/${ANDROID_ABI})

# 安裝生成的庫
install(TARGETS vuforia_wrapper
    LIBRARY DESTINATION .
    COMPONENT Runtime
)

# ==================== 性能分析支持 ====================
# 如果啟用了性能分析
option(ENABLE_PROFILING "Enable profiling support" OFF)
if(ENABLE_PROFILING)
    target_compile_definitions(vuforia_wrapper PRIVATE
        ENABLE_PROFILING=1
    )
    target_compile_options(vuforia_wrapper PRIVATE
        -pg
        -fno-omit-frame-pointer
    )
    target_link_options(vuforia_wrapper PRIVATE
        -pg
    )
    message(STATUS "Profiling support enabled")
endif()

# ==================== 內存檢查支持 ====================
option(ENABLE_ASAN "Enable Address Sanitizer" OFF)
if(ENABLE_ASAN AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(vuforia_wrapper PRIVATE
        -fsanitize=address
        -fno-omit-frame-pointer
    )
    target_link_options(vuforia_wrapper PRIVATE
        -fsanitize=address
    )
    message(STATUS "Address Sanitizer enabled")
endif()

# ==================== 編譯時間優化 ====================
# 智能預編譯頭文件配置 - 基於社群最佳實踐
# 檢查是否使用 clang 編譯器並且不是 Android NDK 環境
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.16")
    # 檢查編譯器類型
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND NOT ANDROID)
        target_precompile_headers(vuforia_wrapper PRIVATE
            <jni.h>
            <android/log.h>
            <string>
            <vector>
            <memory>
            <mutex>
        )
        message(STATUS "Precompiled headers enabled (Clang non-Android)")
        set(PCH_ENABLED TRUE)
    else()
        message(STATUS "Precompiled headers disabled (Android NDK compatibility)")
        set(PCH_ENABLED FALSE)
    endif()
else()
    message(STATUS "Precompiled headers disabled (CMake < 3.16)")
    set(PCH_ENABLED FALSE)
endif()

# ==================== 靜態分析支持 ====================
# 只在沒有預編譯頭文件衝突時啟用 clang-tidy
find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
if(CLANG_TIDY_EXE AND NOT PCH_ENABLED AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set_target_properties(vuforia_wrapper PROPERTIES
        CXX_CLANG_TIDY "${CLANG_TIDY_EXE};-checks=-*,readability-*,performance-*,bugprone-*"
    )
    message(STATUS "clang-tidy static analysis enabled")
else()
    if(PCH_ENABLED)
        message(STATUS "clang-tidy disabled (conflicts with precompiled headers)")
    elseif(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(STATUS "clang-tidy disabled (requires Clang compiler)")
    else()
        message(STATUS "clang-tidy disabled (not found)")
    endif()
endif()

# ==================== 生成 compile_commands.json ====================
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ==================== 編譯資訊輸出 ====================
# 輸出重要的編譯信息
message(STATUS "=== Vuforia Wrapper Build Configuration ===")
message(STATUS "CMake Version: ${CMAKE_VERSION}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Target ABI: ${ANDROID_ABI}")
message(STATUS "Android API Level: ${ANDROID_PLATFORM}")
message(STATUS "NDK Version: ${ANDROID_NDK_REVISION}")
message(STATUS "Vuforia Library: ${VUFORIA_LIBRARY_DIR}/libVuforia.so")
message(STATUS "==========================================")

# ==================== 最終驗證 ====================
# 確保所有必需的依賴都已找到
get_target_property(VUFORIA_WRAPPER_LINK_LIBS vuforia_wrapper LINK_LIBRARIES)
message(STATUS "Linked libraries: ${VUFORIA_WRAPPER_LINK_LIBS}")

# 警告檢查
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(WARNING "Unusual build type: ${CMAKE_BUILD_TYPE}")
endif()

# 成功配置消息
message(STATUS "CMake configuration completed successfully!")
message(STATUS "Ready to build vuforia_wrapper library for ${ANDROID_ABI}")

# ==================== 開發者幫助信息 ====================
message(STATUS "")
message(STATUS "Available targets:")
message(STATUS "  vuforia_wrapper    - Build the main library")
message(STATUS "  clean-vuforia      - Clean generated files")
message(STATUS "  verify-build       - Verify build output")
message(STATUS "")
message(STATUS "Build commands:")
message(STATUS "  ninja vuforia_wrapper     - Build library")
message(STATUS "  ninja clean-vuforia       - Clean build")
message(STATUS "  ninja verify-build        - Verify build")
message(STATUS "")

# ==================== 結束 ====================