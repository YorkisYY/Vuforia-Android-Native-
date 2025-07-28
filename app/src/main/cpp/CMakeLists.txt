cmake_minimum_required(VERSION 3.22.1)
project("vuforia_wrapper")

# 設置 C++ 標準
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 設置 Android STL
set(CMAKE_ANDROID_STL_TYPE c++_shared)

# 找到預建的 Vuforia 庫
find_library(log-lib log)

# 添加 Vuforia 庫
add_library(vuforia SHARED IMPORTED)
set_target_properties(vuforia PROPERTIES
    IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}/libVuforia.so)

# 創建你的 wrapper 庫
add_library(vuforia_wrapper SHARED vuforia_wrapper.cpp)

# 連結庫
target_link_libraries(vuforia_wrapper
    vuforia
    ${log-lib}
    android
    EGL
    GLESv2) 