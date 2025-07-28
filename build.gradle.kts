// Top-level build file where you can add configuration options common to all sub-projects/modules.
buildscript {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
    dependencies {
        classpath("com.android.tools.build:gradle:8.5.2")
        classpath("org.jetbrains.kotlin:kotlin-gradle-plugin:1.9.24")
        classpath("com.google.dagger:hilt-android-gradle-plugin:2.48.1")
        classpath("androidx.navigation:navigation-safe-args-gradle-plugin:2.7.6")
    }
}

allprojects {
    repositories {
        google()
        mavenCentral()
        maven { url = uri("https://jitpack.io") }
        // ARCore 需要的 repository
        maven { url = uri("https://storage.googleapis.com/ar-artifacts/m2repository") }
    }
}

tasks.register("clean", Delete::class) {
    delete(rootProject.buildDir)
}