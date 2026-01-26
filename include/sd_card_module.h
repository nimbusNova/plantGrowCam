#ifndef SD_CARD_MODULE_H
#define SD_CARD_MODULE_H

#include "SD_MMC.h"
#include "FS.h"
#include "esp_camera.h"
#include <vector>

struct ImageInfo {
    String filename;
    size_t size;
};

class SDCardModule {
public:
    SDCardModule();

    bool init();
    bool saveImage(camera_fb_t* fb, const String& filename);
    std::vector<ImageInfo> listImages();
    File openFile(const String& filename);
    int getNextImageNumber();

private:
    bool isInitialized;
    void printCardInfo();
};

#endif
