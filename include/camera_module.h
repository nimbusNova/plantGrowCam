#ifndef CAMERA_MODULE_H
#define CAMERA_MODULE_H

#include "esp_camera.h"

class CameraModule {
public:
    CameraModule();

    bool init();
    camera_fb_t* captureImage();
    void releaseFrameBuffer(camera_fb_t* fb);
    void turnOnFlash();
    void turnOffFlash();
    camera_fb_t* captureWithFlash();

private:
    bool isInitialized;
    void configureCamera(camera_config_t &config);
};

#endif
