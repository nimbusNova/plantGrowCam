#ifndef CONFIG_H
#define CONFIG_H

// WiFi credentials
#define WIFI_SSID "Michael"
#define WIFI_PASSWORD "wuxiaohua1011"

// Camera pins for ESP32-CAM (AI-Thinker)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Flash LED pin
#define FLASH_LED_PIN      4

// Timing configuration
#define CAPTURE_HOUR 15  // Hour to capture (24-hour format, 15 = 3pm)
#define TIMEZONE_OFFSET -8  // PST is UTC-8
#define DAYLIGHT_OFFSET 3600  // 1 hour for daylight saving time

// Web server port
#define WEB_SERVER_PORT 80

// SD card configuration
#define SD_MOUNT_POINT "/sdcard"

// Image filename prefix
#define IMAGE_PREFIX "/plant_"
#define IMAGE_EXTENSION ".jpg"

#endif
