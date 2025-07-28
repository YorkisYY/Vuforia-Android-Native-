plugins {
    id("com.android.application")
}

android {
    namespace = "com.example.ibm_ai_weather_art_android"
    compileSdk = 34

    // 使用 Vuforia 官方支援的 NDK 版本
    ndkVersion = "21.4.7075529"

    defaultConfig {
        applicationId = "com.example.ibm_ai_weather_art_android"
        minSdk = 24
        targetSdk = 34
        versionCode = 1
        versionName = "1.0"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        
        // 支援 arm64-v8a, armeabi-v7a
        ndk {
            abiFilters += listOf("arm64-v8a", "armeabi-v7a")
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
        debug {
            // 不需額外 ABI 過濾器
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_1_8
        targetCompatibility = JavaVersion.VERSION_1_8
    }

    // CMake 配置
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    sourceSets {
        getByName("main") {
            jniLibs.srcDirs("src/main/jniLibs")
        }
    }
    
    // 防止 GLB 檔案被壓縮
    aaptOptions {
        noCompress("glb", "gltf", "bin")
    }
    
    // 確保 Filament 庫正確載入
    packagingOptions {
        pickFirst("**/libfilament-jni.so")
        pickFirst("**/libgltfio-jni.so")
    }
}

dependencies {
    implementation("androidx.appcompat:appcompat:1.6.1")
    implementation("com.google.android.material:material:1.11.0")
    implementation("androidx.constraintlayout:constraintlayout:2.1.4")
    
    // ========== Vuforia 標準依賴 ==========
    // 暫時移除 Vuforia.jar 依賴，因為我們主要使用 Filament 進行渲染
    // implementation(fileTree(mapOf("dir" to "libs", "include" to listOf("*.jar"))))
    // implementation(files("src/main/libs/Vuforia.jar")) // 標準 Vuforia.jar
    
    // ========== Filament 3D 渲染引擎 (GLB 支援) ==========
    // 使用更穩定的版本
    implementation("com.google.android.filament:filament-android:1.31.0")
    implementation("com.google.android.filament:filament-utils-android:1.31.0")
    implementation("com.google.android.filament:gltfio-android:1.31.0")
    
    // ========== 測試依賴 ==========
    testImplementation("junit:junit:4.13.2")
    androidTestImplementation("androidx.test.ext:junit:1.1.5")
    androidTestImplementation("androidx.test.espresso:espresso-core:3.5.1")
    
    // ✅ 新增：CameraX 依赖
    val cameraxVersion = "1.3.1"
    implementation("androidx.camera:camera-core:${cameraxVersion}")
    implementation("androidx.camera:camera-camera2:${cameraxVersion}")
    implementation("androidx.camera:camera-lifecycle:${cameraxVersion}")
    implementation("androidx.camera:camera-view:${cameraxVersion}")
}