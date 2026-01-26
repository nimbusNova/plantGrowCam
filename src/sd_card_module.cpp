#include "sd_card_module.h"
#include "config.h"
#include <Arduino.h>

SDCardModule::SDCardModule() : isInitialized(false) {}

bool SDCardModule::init() {
    if (!SD_MMC.begin(SD_MOUNT_POINT, true)) { // true = 1-bit mode
        Serial.println("SD Card Mount Failed");
        return false;
    }

    uint8_t cardType = SD_MMC.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("No SD Card attached");
        return false;
    }

    printCardInfo();
    isInitialized = true;
    return true;
}

void SDCardModule::printCardInfo() {
    uint8_t cardType = SD_MMC.cardType();

    Serial.print("SD Card Type: ");
    if (cardType == CARD_MMC) {
        Serial.println("MMC");
    } else if (cardType == CARD_SD) {
        Serial.println("SDSC");
    } else if (cardType == CARD_SDHC) {
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }

    uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);

    uint64_t usedSize = SD_MMC.usedBytes() / (1024 * 1024);
    Serial.printf("SD Card Used: %lluMB\n", usedSize);
}

bool SDCardModule::saveImage(camera_fb_t* fb, const String& filename) {
    if (!isInitialized) {
        Serial.println("SD Card not initialized");
        return false;
    }

    if (!fb) {
        Serial.println("Invalid frame buffer");
        return false;
    }

    File file = SD_MMC.open(filename, FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for writing");
        return false;
    }

    size_t written = file.write(fb->buf, fb->len);
    file.close();

    if (written != fb->len) {
        Serial.println("Failed to write complete image");
        return false;
    }

    Serial.printf("Image saved: %s (%d bytes)\n", filename.c_str(), fb->len);
    return true;
}

std::vector<ImageInfo> SDCardModule::listImages() {
    std::vector<ImageInfo> images;

    if (!isInitialized) {
        Serial.println("SD Card not initialized");
        return images;
    }

    File root = SD_MMC.open("/");
    if (!root) {
        Serial.println("Failed to open root directory");
        return images;
    }

    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String filename = String(file.name());
            if (filename.endsWith(IMAGE_EXTENSION)) {
                ImageInfo info;
                info.filename = filename;
                info.size = file.size();
                images.push_back(info);
            }
        }
        file = root.openNextFile();
    }

    return images;
}

File SDCardModule::openFile(const String& filename) {
    if (!isInitialized) {
        Serial.println("SD Card not initialized");
        return File();
    }

    return SD_MMC.open(filename, FILE_READ);
}

int SDCardModule::getNextImageNumber() {
    if (!isInitialized) {
        return 0;
    }

    int maxNum = 0;
    File root = SD_MMC.open("/");
    if (!root) {
        return 0;
    }

    File file = root.openNextFile();
    while (file) {
        String name = String(file.name());
        if (name.startsWith(IMAGE_PREFIX) && name.endsWith(IMAGE_EXTENSION)) {
            int prefixLen = String(IMAGE_PREFIX).length();
            int extLen = String(IMAGE_EXTENSION).length();
            String numStr = name.substring(prefixLen, name.length() - extLen);
            int num = numStr.toInt();
            if (num >= maxNum) {
                maxNum = num + 1;
            }
        }
        file = root.openNextFile();
    }

    return maxNum;
}
