#ifndef WEB_SERVER_MODULE_H
#define WEB_SERVER_MODULE_H

#include <WebServer.h>
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
    CameraModule* camera;
    SDCardModule* sdCard;
    int* imageCount;

    // Route handlers
    void handleRoot();
    void handleStream();
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
