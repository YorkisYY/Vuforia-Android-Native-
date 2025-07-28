cmake_minimum_required(VERSION 3.22.1)
project("vuforia_wrapper")

set(CMAKE_CXX_STANDARD 17)

add_library(vuforia_wrapper SHARED vuforia_wrapper.cpp)

find_library(log-lib log)
find_library(android-lib android)

target_link_libraries(
    vuforia_wrapper
    ${log-lib}
    ${android-lib}
    GLESv2
    EGL
)

target_compile_definitions(vuforia_wrapper PRIVATE
    ANDROID
    GL_GLEXT_PROTOTYPES
)