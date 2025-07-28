package com.example.ibm_ai_weather_art_android.ui;

import android.widget.Button;

/**
 * Control Panel
 * Manages all UI buttons and user interactions
 */
public class ControlPanel {
    private Button btnLoadModel;
    private Button btnPlaceObject;
    private Button btnClearObjects;
    private ControlCallback callback;

    public ControlPanel(Button btnLoadModel, Button btnPlaceObject, ControlCallback callback) {
        this.btnLoadModel = btnLoadModel;
        this.btnPlaceObject = btnPlaceObject;
        this.callback = callback;
        initializeControls();
    }

    /**
     * Add clear button (optional)
     */
    public void setClearButton(Button btnClearObjects) {
        this.btnClearObjects = btnClearObjects;
        if (btnClearObjects != null) {
            btnClearObjects.setOnClickListener(v -> callback.onClearObjectsClicked());
            btnClearObjects.setEnabled(false); // Initially disabled
        }
    }

    /**
     * Initialize controls
     */
    private void initializeControls() {
        // Load model button
        btnLoadModel.setOnClickListener(v -> callback.onLoadModelClicked());
        btnLoadModel.setEnabled(true);
        
        // Place object button
        btnPlaceObject.setOnClickListener(v -> callback.onPlaceObjectClicked());
        btnPlaceObject.setEnabled(false); // Initially disabled, enable after model loads
    }

    /**
     * UI state when model loading starts
     */
    public void onModelLoadingStarted() {
        btnLoadModel.setEnabled(false);
        btnLoadModel.setText("Loading...");
        btnPlaceObject.setEnabled(false);
    }

    /**
     * UI state when model loading succeeds
     */
    public void onModelLoadingSuccess() {
        btnLoadModel.setEnabled(true);
        btnLoadModel.setText("Reload Model");
        btnPlaceObject.setEnabled(true);
        btnPlaceObject.setText("Click Screen to Place");
    }

    /**
     * UI state when model loading fails
     */
    public void onModelLoadingFailed() {
        btnLoadModel.setEnabled(true);
        btnLoadModel.setText("Retry Loading");
        btnPlaceObject.setEnabled(false);
        btnPlaceObject.setText("Model Not Loaded");
    }

    /**
     * UI state when object is placed successfully
     */
    public void onObjectPlaced() {
        if (btnClearObjects != null) {
            btnClearObjects.setEnabled(true);
        }
        // Update place button text
        btnPlaceObject.setText("Continue Placing");
    }

    /**
     * UI state when all objects are cleared
     */
    public void onAllObjectsCleared() {
        if (btnClearObjects != null) {
            btnClearObjects.setEnabled(false);
        }
        btnPlaceObject.setText("Click Screen to Place");
    }

    /**
     * UI state when AR initialization completes
     */
    public void onARInitialized() {
        btnLoadModel.setEnabled(true);
    }

    /**
     * UI state when AR initialization fails
     */
    public void onARInitializationFailed() {
        btnLoadModel.setEnabled(false);
        btnPlaceObject.setEnabled(false);
        if (btnClearObjects != null) {
            btnClearObjects.setEnabled(false);
        }
    }

    /**
     * Set button availability
     */
    public void setButtonsEnabled(boolean enabled) {
        btnLoadModel.setEnabled(enabled);
        btnPlaceObject.setEnabled(enabled);
        if (btnClearObjects != null) {
            btnClearObjects.setEnabled(enabled);
        }
    }

    /**
     * Update place button text
     */
    public void updatePlaceButtonText(String text) {
        btnPlaceObject.setText(text);
    }

    /**
     * Control event callback interface
     */
    public interface ControlCallback {
        void onLoadModelClicked();
        void onPlaceObjectClicked();
        void onClearObjectsClicked();
    }
} 