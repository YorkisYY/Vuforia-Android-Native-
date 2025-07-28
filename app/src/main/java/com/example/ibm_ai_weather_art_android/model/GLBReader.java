package com.example.ibm_ai_weather_art_android.model;

import android.content.Context;
import android.net.Uri;
import java.io.IOException;
import java.io.InputStream;

/**
 * GLB file reader
 * Handles reading and validation of GLB files
 */
public class GLBReader {
    
    private Context context;
    private static final String MODELS_PATH = "models/";

    public GLBReader(Context context) {
        this.context = context;
    }

    /**
     * Check if GLB file exists
     */
    public boolean checkGLBExists(String fileName) {
        String fullPath = MODELS_PATH + fileName;
        try (InputStream inputStream = context.getAssets().open(fullPath)) {
            return true;
        } catch (IOException e) {
            return false;
        }
    }

    /**
     * Get URI of GLB file
     */
    public Uri getGLBUri(String fileName) {
        return Uri.parse(MODELS_PATH + fileName);
    }

    /**
     * List all available GLB files
     */
    public String[] listAvailableGLBFiles() {
        try {
            String[] allFiles = context.getAssets().list(MODELS_PATH);
            if (allFiles == null) return new String[0];
            java.util.List<String> glbFiles = new java.util.ArrayList<>();
            for (String file : allFiles) {
                if (file.toLowerCase().endsWith(".glb")) {
                    glbFiles.add(file);
                }
            }
            return glbFiles.toArray(new String[0]);
        } catch (IOException e) {
            return new String[0];
        }
    }

    /**
     * Get GLB file size
     */
    public long getGLBFileSize(String fileName) {
        String fullPath = MODELS_PATH + fileName;
        try (InputStream inputStream = context.getAssets().open(fullPath)) {
            return inputStream.available();
        } catch (IOException e) {
            return -1;
        }
    }

    /**
     * Validate GLB file format
     */
    public boolean validateGLBFile(String fileName) {
        if (!fileName.toLowerCase().endsWith(".glb")) {
            return false;
        }
        return checkGLBExists(fileName);
    }

    /**
     * GLB file info class
     */
    public static class GLBFileInfo {
        public String fileName;
        public String fullPath;
        public long fileSize;
        public boolean isValid;

        public GLBFileInfo(String fileName, String fullPath, long fileSize, boolean isValid) {
            this.fileName = fileName;
            this.fullPath = fullPath;
            this.fileSize = fileSize;
            this.isValid = isValid;
        }
    }

    /**
     * Get detailed info of GLB file
     */
    public GLBFileInfo getGLBFileInfo(String fileName) {
        String fullPath = MODELS_PATH + fileName;
        long fileSize = getGLBFileSize(fileName);
        boolean isValid = validateGLBFile(fileName) && fileSize > 0;
        return new GLBFileInfo(fileName, fullPath, fileSize, isValid);
    }
} 