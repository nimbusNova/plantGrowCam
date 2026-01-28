#include "web_server_module.h"
#include "config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "esp_camera.h"

WebServerModule::WebServerModule(CameraModule* cam, SDCardModule* sd, int* imgCount)
    : server(WEB_SERVER_PORT), camera(cam), sdCard(sd), imageCount(imgCount) {}

bool WebServerModule::init() {
    // Setup routes
    server.on("/", [this]() { this->handleRoot(); });
    server.on("/stream", [this]() { this->handleStream(); });
    server.on("/capture", [this]() { this->handleCapture(); });
    server.on("/list", [this]() { this->handleList(); });
    server.on("/download", [this]() { this->handleDownload(); });
    server.on("/flash/on", [this]() { this->handleFlashOn(); });
    server.on("/flash/off", [this]() { this->handleFlashOff(); });

    server.begin();
    Serial.println("Web server started");
    return true;
}

void WebServerModule::handleClient() {
    server.handleClient();
}

void WebServerModule::printServerInfo() {
    Serial.println("\n========================================");
    Serial.println("Plant Monitor Ready!");
    Serial.printf("IP Address: http://%s\n", WiFi.localIP().toString().c_str());
    Serial.println("========================================");
    Serial.println("Available endpoints:");
    Serial.println("  /               - Home page with links");
    Serial.println("  /stream         - Live camera stream");
    Serial.println("  /capture        - Take a picture now");
    Serial.println("  /list           - List all saved images");
    Serial.println("  /download?file= - Download an image");
    Serial.println("  /flash/on       - Turn flash ON");
    Serial.println("  /flash/off      - Turn flash OFF");
    Serial.println("========================================\n");
}

String WebServerModule::generateHTMLHeader(const String& title) {
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>" + title + "</title>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }";
    html += "h1 { color: #2e7d32; }";
    html += ".container { background: white; padding: 20px; border-radius: 10px; max-width: 800px; margin: auto; }";
    html += ".button { display: inline-block; padding: 10px 20px; margin: 10px 5px; background: #4CAF50; color: white; text-decoration: none; border-radius: 5px; }";
    html += ".button:hover { background: #45a049; }";
    html += "img { max-width: 100%; height: auto; border: 2px solid #ddd; margin-top: 10px; }";
    html += ".image-item { border: 1px solid #ddd; padding: 10px; margin: 10px 0; border-radius: 5px; }";
    html += "</style>";
    html += "</head><body>";
    html += "<div class='container'>";
    return html;
}

String WebServerModule::generateHTMLFooter() {
    return "</div></body></html>";
}

void WebServerModule::handleRoot() {
    String html = generateHTMLHeader("Plant Monitor");
    html += "<h1>Plant Monitor Dashboard</h1>";
    html += "<p>Welcome to your ESP32-CAM Plant Monitoring System</p>";
    html += "<div>";
    html += "<a class='button' href='/stream'>Live Stream</a>";
    html += "<a class='button' href='/capture'>Take Picture Now</a>";
    html += "<a class='button' href='/list'>View Saved Images</a>";
    html += "</div>";
    html += "<h2>Flash Control</h2>";
    html += "<div>";
    html += "<a class='button' href='/flash/on'>Turn Flash ON</a>";
    html += "<a class='button' href='/flash/off'>Turn Flash OFF</a>";
    html += "</div>";
    html += generateHTMLFooter();

    server.send(200, "text/html", html);
}

void WebServerModule::handleStream() {
    WiFiClient client = server.client();

    if (!client.connected()) {
        return;
    }

    // Increase TCP send buffer for better throughput
    client.setNoDelay(true);  // Disable Nagle's algorithm for lower latency

    // Get camera sensor and optimize for streaming
    sensor_t * s = esp_camera_sensor_get();

    // Store original settings
    framesize_t original_framesize = FRAMESIZE_UXGA;
    int original_quality = 10;
    int original_fb_count = 2;

    if (s) {
        original_framesize = (framesize_t)s->status.framesize;
        original_quality = s->status.quality;

        // Aggressive streaming optimizations - prioritize speed
        s->set_framesize(s, FRAMESIZE_HVGA);  // 480x320 - smaller than VGA
        s->set_quality(s, 18);  // Higher compression = much smaller files
        s->set_brightness(s, 0);
        s->set_contrast(s, 0);
        s->set_saturation(s, 0);
        s->set_sharpness(s, 0);
        s->set_denoise(s, 0);
        s->set_gainceiling(s, GAINCEILING_2X);
        s->set_ae_level(s, 0);
        s->set_special_effect(s, 0);
        s->set_wb_mode(s, 0);
        s->set_awb_gain(s, 1);
        s->set_aec2(s, 0);         // Disable AEC2 for speed
        s->set_whitebal(s, 1);     // Keep white balance
        s->set_hmirror(s, 0);
        s->set_vflip(s, 0);
    }

    // Send HTTP headers
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
    client.println("Access-Control-Allow-Origin: *");
    client.println("X-Framerate: 30");
    client.println();
    client.flush();

    Serial.println("Stream started with optimized settings");

    uint32_t frame_count = 0;
    uint32_t last_log_time = millis();

    while (client.connected()) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Frame capture failed");
            continue;
        }

        // Fast streaming: minimize overhead
        client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", fb->len);

        // Send frame in one write
        client.write(fb->buf, fb->len);
        client.print("\r\n");

        uint32_t frame_size = fb->len;
        esp_camera_fb_return(fb);

        frame_count++;

        // Log FPS every 50 frames
        if (frame_count % 50 == 0) {
            uint32_t now = millis();
            float fps = 50000.0 / (now - last_log_time);
            Serial.printf("Streaming: %.1f FPS, %u bytes/frame\n", fps, frame_size);
            last_log_time = now;
        }
    }

    // Restore original camera settings
    if (s) {
        s->set_framesize(s, original_framesize);
        s->set_quality(s, original_quality);
    }

    Serial.printf("Stream ended after %u frames\n", frame_count);
}

void WebServerModule::handleCapture() {
    Serial.println("Manual capture requested via web interface");
    String result = captureAndSaveImage();

    String html = generateHTMLHeader("Capture Result");
    html += "<h1>Capture Result</h1>";
    html += "<p>" + result + "</p>";
    html += "<a class='button' href='/'>← Back to Home</a>";
    html += "<a class='button' href='/list'>View Images</a>";
    html += generateHTMLFooter();

    server.send(200, "text/html", html);
}

void WebServerModule::handleList() {
    String html = generateHTMLHeader("Saved Images");
    html += "<h1>Saved Images</h1>";
    html += "<a class='button' href='/'>← Back to Home</a>";

    std::vector<ImageInfo> images = sdCard->listImages();

    if (images.empty()) {
        html += "<p>No images found. Take your first picture!</p>";
    } else {
        for (const auto& img : images) {
            html += "<div class='image-item'>";
            html += "<h3>" + img.filename + "</h3>";
            html += "<p>Size: " + String(img.size / 1024) + " KB</p>";
            html += "<a href='/download?file=" + img.filename + "' download>💾 Download</a>";
            html += "</div>";
        }
        html += "<p>Total images: " + String(images.size()) + "</p>";
    }

    html += generateHTMLFooter();
    server.send(200, "text/html", html);
}

void WebServerModule::handleDownload() {
    if (!server.hasArg("file")) {
        server.send(400, "text/plain", "Missing file parameter");
        return;
    }

    String filename = server.arg("file");
    if (!filename.startsWith("/")) {
        filename = "/" + filename;
    }

    File file = sdCard->openFile(filename);
    if (!file) {
        server.send(404, "text/plain", "File not found");
        return;
    }

    server.sendHeader("Content-Disposition", "attachment; filename=" + server.arg("file"));
    server.streamFile(file, "image/jpeg");
    file.close();
}

String WebServerModule::captureAndSaveImage() {
    camera_fb_t *fb = camera->captureImage();

    if (!fb) {
        Serial.println("Camera capture failed");
        return "Camera capture failed";
    }

    // Generate timestamp-based filename
    struct tm timeinfo;
    String filename;
    if (getLocalTime(&timeinfo)) {
        char timestamp[32];
        strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", &timeinfo);
        filename = String(IMAGE_PREFIX) + String(timestamp) + String(IMAGE_EXTENSION);
    } else {
        // Fallback to counter if time not available
        filename = String(IMAGE_PREFIX) + String(*imageCount) + String(IMAGE_EXTENSION);
    }
    (*imageCount)++;

    bool success = sdCard->saveImage(fb, filename);
    camera->releaseFrameBuffer(fb);

    if (!success) {
        return "Failed to save image to SD card";
    }

    return "Image saved successfully: " + filename;
}

void WebServerModule::handleFlashOn() {
    camera->turnOnFlash();
    Serial.println("Flash turned ON via web interface");

    // Redirect back to homepage
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
}

void WebServerModule::handleFlashOff() {
    camera->turnOffFlash();
    Serial.println("Flash turned OFF via web interface");

    // Redirect back to homepage
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
}
