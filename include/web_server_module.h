#ifndef WEB_SERVER_MODULE_H
#define WEB_SERVER_MODULE_H

#include <WebServer.h>
#include "esp_http_server.h"
#include "camera_module.h"
#include "sd_card_module.h"

class WebServerModule {
public:
    WebServerModule(CameraModule* cam, SDCardModule* sd, int* imgCount);

    bool init();
    void handleClient();
    void printServerInfo();

private:
    WebServer server;
    httpd_handle_t stream_httpd;
    CameraModule* camera;
    SDCardModule* sdCard;
    int* imageCount;

    // Route handlers
    void handleRoot();
    static esp_err_t streamHandler(httpd_req_t *req);
    void handleCapture();
    void handleList();
    void handleDownload();
    void handleFlashOn();
    void handleFlashOff();

    // Helper functions
    String captureAndSaveImage();
    String generateHTMLHeader(const String& title);
    String generateHTMLFooter();
};

#endif
