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
    
    // 🔧 修正：使用 Gradle 8+ 語法，確保 Filament 庫被包含
    packaging {
        jniLibs {
            pickFirsts.add("**/libfilament-jni.so")
            pickFirsts.add("**/libgltfio-jni.so")
            pickFirsts.add("**/libc++_shared.so")
        }
    }
}

dependencies {
    implementation("androidx.appcompat:appcompat:1.6.1")
    implementation("com.google.android.material:material:1.11.0")
    implementation("androidx.constraintlayout:constraintlayout:2.1.4")
    
    // ========== Filament 3D 渲染引擎 (GLB 支援) ==========
    implementation("com.google.android.filament:filament-android:1.31.0")
    implementation("com.google.android.filament:filament-utils-android:1.31.0")
    implementation("com.google.android.filament:gltfio-android:1.31.0")
    
    // ========== 測試依賴 ==========
    testImplementation("junit:junit:4.13.2")
    androidTestImplementation("androidx.test.ext:junit:1.1.5")
    androidTestImplementation("androidx.test.espresso:espresso-core:3.5.1")
}