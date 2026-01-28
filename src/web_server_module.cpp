#include "web_server_module.h"
#include "config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "esp_camera.h"
#include "img_converters.h"

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

WebServerModule::WebServerModule(CameraModule* cam, SDCardModule* sd, int* imgCount)
    : server(WEB_SERVER_PORT), stream_httpd(NULL), camera(cam), sdCard(sd), imageCount(imgCount) {}

bool WebServerModule::init() {
    // Setup routes for WebServer (static pages)
    server.on("/", [this]() { this->handleRoot(); });
    server.on("/capture", [this]() { this->handleCapture(); });
    server.on("/list", [this]() { this->handleList(); });
    server.on("/download", [this]() { this->handleDownload(); });
    server.on("/flash/on", [this]() { this->handleFlashOn(); });
    server.on("/flash/off", [this]() { this->handleFlashOff(); });

    server.begin();
    Serial.println("Web server started on port 80");

    // Setup ESP HTTP Server for streaming (more efficient)
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 81;
    config.ctrl_port = 32768;

    httpd_uri_t stream_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = streamHandler,
        .user_ctx  = NULL
    };

    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &stream_uri);
        Serial.println("Stream server started on port 81");
    } else {
        Serial.println("Failed to start stream server");
    }

    return true;
}

void WebServerModule::handleClient() {
    server.handleClient();
}

void WebServerModule::printServerInfo() {
    String ip = WiFi.localIP().toString();
    Serial.println("\n========================================");
    Serial.println("Plant Monitor Ready!");
    Serial.printf("IP Address: http://%s\n", ip.c_str());
    Serial.println("========================================");
    Serial.println("Available endpoints:");
    Serial.printf("  http://%s/          - Home page\n", ip.c_str());
    Serial.printf("  http://%s:81/        - Live stream (ESP HTTP Server)\n", ip.c_str());
    Serial.printf("  http://%s/capture    - Take picture\n", ip.c_str());
    Serial.printf("  http://%s/list       - List images\n", ip.c_str());
    Serial.printf("  http://%s/download?file= - Download image\n", ip.c_str());
    Serial.printf("  http://%s/flash/on   - Flash ON\n", ip.c_str());
    Serial.printf("  http://%s/flash/off  - Flash OFF\n", ip.c_str());
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
    String ip = WiFi.localIP().toString();
    String html = generateHTMLHeader("Plant Monitor");
    html += "<h1>Plant Monitor Dashboard</h1>";
    html += "<p>Welcome to your ESP32-CAM Plant Monitoring System</p>";
    html += "<div>";
    html += "<a class='button' href='http://" + ip + ":81/' target='_blank'>Live Stream</a>";
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

// ESP HTTP Server streaming handler (proven to work well)
esp_err_t WebServerModule::streamHandler(httpd_req_t *req) {
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t * _jpg_buf = NULL;
    char * part_buf[64];

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if(res != ESP_OK){
        return res;
    }

    Serial.println("Stream started");

    while(true){
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Camera capture failed");
            res = ESP_FAIL;
        } else {
            if(fb->format != PIXFORMAT_JPEG){
                bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
                esp_camera_fb_return(fb);
                fb = NULL;
                if(!jpeg_converted){
                    Serial.println("JPEG compression failed");
                    res = ESP_FAIL;
                }
            } else {
                _jpg_buf_len = fb->len;
                _jpg_buf = fb->buf;
            }
        }
        if(res == ESP_OK){
            size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }
        if(fb){
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        } else if(_jpg_buf){
            free(_jpg_buf);
            _jpg_buf = NULL;
        }
        if(res != ESP_OK){
            break;
        }
    }

    Serial.println("Stream ended");
    return res;
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
