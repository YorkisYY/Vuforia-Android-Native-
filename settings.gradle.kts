// 基於 Gradle 8.8 穩定性的最佳化配置
rootProject.name = "IBM-AI-Weather-Art-Android"
include(":app")

// 依賴管理 (避免 NPE 和提升穩定性)
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.PREFER_PROJECT)
    repositories {
        google {
            mavenContent {
                includeGroupAndSubgroups("androidx")
                includeGroupAndSubgroups("com.android")
                includeGroupAndSubgroups("com.google")
            }
        }
        mavenCentral()
        maven {
            url = uri("https://jitpack.io")
            name = "JitPack"
            content {
                includeGroupByRegex("com\\.github\\..*")
            }
        }
        // ARCore 專用 repository
        maven {
            url = uri("https://storage.googleapis.com/ar-artifacts/m2repository")
            name = "ARCore"
            content {
                includeGroup("com.google.ar")
                includeGroup("com.google.ar.sceneform")
            }
        }
        // Filament 引擎 repository (GLB 支援)
        maven {
            url = uri("https://storage.googleapis.com/filament-releases/maven")
            name = "Filament"
            content {
                includeGroup("com.google.android.filament")
            }
        }
    }
}

// 插件管理
pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}