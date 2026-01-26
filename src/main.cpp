#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "config.h"
#include "camera_module.h"
#include "sd_card_module.h"
#include "web_server_module.h"

// Module instances
CameraModule camera;
SDCardModule sdCard;
WebServerModule* webServer = nullptr;

// Timer variables
int lastCaptureDay = -1;  // Track the day of year we last captured
int imageCount = 0;

// Function declarations
bool connectWiFi();
bool isWiFiConnected();
void performScheduledCapture();
void setupTime();
bool shouldCaptureNow();

void setup() {
    Serial.begin(115200);
    delay(100);  // Wait for serial port to stabilize
    Serial.flush();  // Clear any garbage data
    delay(100);
    Serial.println("\n\nESP32-CAM Plant Monitor Starting...");

    // Initialize camera
    while (!camera.init()) {
        Serial.println("Camera initialization failed! Retrying...");
        delay(1000);
    }

    while (!isWiFiConnected()) {
        connectWiFi();
        Serial.println("WiFi connection failed! Retrying...");
        delay(1000);
    }

    // Connect to WiFi FIRST (required before web server)
    if (isWiFiConnected()) {
        Serial.println("WiFi connected! Initializing web server...");

        // Setup time synchronization via NTP
        setupTime();

        // Initialize web server ONLY after WiFi is connected
        webServer = new WebServerModule(&camera, &sdCard, &imageCount);
        webServer->init();
        webServer->printServerInfo();
    }

    // Initialize SD card ONLY after WiFi is connected
    while (!sdCard.init()) {
        Serial.println("SD Card initialization failed! Retrying...");
        delay(1000);
    }

    // Get the next image number
    imageCount = sdCard.getNextImageNumber();
    Serial.printf("Starting image count: %d\n", imageCount);

    Serial.println("Setup complete!");
}

void loop() {
    // Ensure WiFi is connected
    if (!isWiFiConnected()) {
        Serial.println("WiFi connection lost! Retrying...");
        webServer = nullptr;
        if (connectWiFi()) {
            Serial.println("WiFi connected! Initializing web server...");
            webServer = new WebServerModule(&camera, &sdCard, &imageCount);
            webServer->init();
            webServer->printServerInfo();
        }
    }

    // Handle web server requests only if web server is initialized
    if (webServer != nullptr) {
        webServer->handleClient();
    }

    // Check if it's time for automatic capture (daily at 3pm)
    if (shouldCaptureNow()) {
        performScheduledCapture();
    }
}

bool isWiFiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

bool connectWiFi() {
    Serial.println("Connecting to WiFi");
    
    // Set WiFi mode to station (client) mode
    WiFi.mode(WIFI_STA);
    
    // Disconnect any existing connection
    WiFi.disconnect();
    delay(100);
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.printf("Still connecting to WiFi %d...\n", attempts);
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        return true;
    } else {
        Serial.println("\nWiFi connection failed!");
        return false;
    }
}

void setupTime() {
    // Configure NTP time synchronization
    // Format: timezone offset, daylight offset, ntp server
    configTime(TIMEZONE_OFFSET * 3600, DAYLIGHT_OFFSET, "pool.ntp.org", "time.nist.gov");

    Serial.println("Waiting for NTP time sync...");
    time_t now = time(nullptr);
    int attempts = 0;
    while (now < 24 * 3600 && attempts < 20) {  // Wait until we have a valid time (after 1970)
        delay(500);
        Serial.print(".");
        now = time(nullptr);
        attempts++;
    }
    Serial.println();

    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        Serial.println("Time synchronized!");
        Serial.println(&timeinfo, "Current time: %A, %B %d %Y %H:%M:%S");
    } else {
        Serial.println("Failed to obtain time");
    }
}

bool shouldCaptureNow() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to get local time");
        return false;
    }

    int currentDay = timeinfo.tm_yday;  // Day of year (0-365)
    int currentHour = timeinfo.tm_hour;  // Hour (0-23)

    // Check if it's the configured capture hour or later today and we haven't captured yet today
    if (currentHour >= CAPTURE_HOUR && lastCaptureDay != currentDay) {
        lastCaptureDay = currentDay;
        return true;
    }

    return false;
}

void performScheduledCapture() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        Serial.print("Time for scheduled capture! ");
        Serial.println(&timeinfo, "Time: %A, %B %d %Y %H:%M:%S");
    } else {
        Serial.println("Time for scheduled capture!");
    }

    camera_fb_t *fb = camera.captureImage();
    if (!fb) {
        Serial.println("Scheduled capture failed");
        return;
    }

    String filename = String(IMAGE_PREFIX) + String(imageCount) + String(IMAGE_EXTENSION);
    imageCount++;

    bool success = sdCard.saveImage(fb, filename);
    camera.releaseFrameBuffer(fb);

    if (success) {
        Serial.printf("Scheduled capture saved: %s\n", filename.c_str());
    } else {
        Serial.println("Failed to save scheduled capture");
    }
}
