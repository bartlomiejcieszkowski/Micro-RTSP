#ifndef OV2640_H_
#define OV2640_H_

#include <Arduino.h>
#include <pgmspace.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_camera.h"

#include "camera_pins.h"

class OV2640
{
public:
    OV2640(){
        fb = NULL;
    };
    ~OV2640(){
    };
    esp_err_t init(void);
    void run(void);
    size_t getSize(void);
    uint8_t *getfb(void);
    int getWidth(void);
    int getHeight(void);
    framesize_t getFrameSize(void);
    pixformat_t getPixelFormat(void);

    void setFrameSize(framesize_t size);
    void setPixelFormat(pixformat_t format);

private:
    void runIfNeeded(); // grab a frame if we don't already have one

    // camera_framesize_t _frame_size;
    // camera_pixelformat_t _pixel_format;
    camera_config_t _cam_config;

    camera_fb_t *fb;
};

#endif //OV2640_H_
