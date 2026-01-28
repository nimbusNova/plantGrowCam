#include "camera_module.h"
#include "config.h"
#include <Arduino.h>

CameraModule::CameraModule() : isInitialized(false) {
    pinMode(FLASH_LED_PIN, OUTPUT);
    digitalWrite(FLASH_LED_PIN, LOW);
}

bool CameraModule::init() {
    camera_config_t config;
    configureCamera(config);

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x\n", err);
        return false;
    }

    isInitialized = true;
    Serial.println("Camera initialized successfully");
    return true;
}

void CameraModule::configureCamera(camera_config_t &config) {
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.grab_mode = CAMERA_GRAB_LATEST;  // Always get latest frame for streaming

    if (psramFound()) {
        config.frame_size = FRAMESIZE_SVGA; // 800x600 - smaller default
        config.jpeg_quality = 10;  // High quality for still photos
        config.fb_count = 2;  // Double buffering for better streaming
        config.fb_location = CAMERA_FB_IN_PSRAM;  // Use PSRAM for frame buffers
    } else {
        config.frame_size = FRAMESIZE_VGA; // 640x480
        config.jpeg_quality = 12;
        config.fb_count = 2;  // Try double buffer even without PSRAM
    }
}

camera_fb_t* CameraModule::captureImage() {
    if (!isInitialized) {
        Serial.println("Camera not initialized");
        return nullptr;
    }

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        return nullptr;
    }

    return fb;
}

void CameraModule::releaseFrameBuffer(camera_fb_t* fb) {
    if (fb) {
        esp_camera_fb_return(fb);
    }
}

void CameraModule::turnOnFlash() {
    digitalWrite(FLASH_LED_PIN, HIGH);
}

void CameraModule::turnOffFlash() {
    digitalWrite(FLASH_LED_PIN, LOW);
}

camera_fb_t* CameraModule::captureWithFlash() {
    turnOnFlash();
    delay(300); // Give time for flash to stabilize and sensor to adjust
    camera_fb_t *fb = captureImage();
    delay(50);  // Small delay before turning off flash
    turnOffFlash();
    return fb;
}
