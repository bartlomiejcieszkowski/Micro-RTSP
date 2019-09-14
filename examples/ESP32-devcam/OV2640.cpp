#include "OV2640.h"

#define TAG "OV2640"

void OV2640::run(void)
{
    if (fb)
        //return the frame buffer back to the driver for reuse
        esp_camera_fb_return(fb);

    fb = esp_camera_fb_get();
}

void OV2640::runIfNeeded(void)
{
    if (!fb)
        run();
}

int OV2640::getWidth(void)
{
    runIfNeeded();
    return fb->width;
}

int OV2640::getHeight(void)
{
    runIfNeeded();
    return fb->height;
}

size_t OV2640::getSize(void)
{
    runIfNeeded();
    if (!fb)
        return 0; // FIXME - this shouldn't be possible but apparently the new cam board returns null sometimes?
    return fb->len;
}

uint8_t *OV2640::getfb(void)
{
    runIfNeeded();
    if (!fb)
        return NULL; // FIXME - this shouldn't be possible but apparently the new cam board returns null sometimes?

    return fb->buf;
}

framesize_t OV2640::getFrameSize(void)
{
    return _cam_config.frame_size;
}

void OV2640::setFrameSize(framesize_t size)
{
    _cam_config.frame_size = size;
}

pixformat_t OV2640::getPixelFormat(void)
{
    return _cam_config.pixel_format;
}

void OV2640::setPixelFormat(pixformat_t format)
{
    switch (format)
    {
    case PIXFORMAT_RGB565:
    case PIXFORMAT_YUV422:
    case PIXFORMAT_GRAYSCALE:
    case PIXFORMAT_JPEG:
        _cam_config.pixel_format = format;
        break;
    default:
        _cam_config.pixel_format = PIXFORMAT_GRAYSCALE;
        break;
    }
}

esp_err_t OV2640::init(void)
{
    memset(&_cam_config, 0, sizeof(_cam_config));

    _cam_config.pin_pwdn = PWDN_GPIO_NUM;
    _cam_config.pin_reset = RESET_GPIO_NUM;

    _cam_config.pin_xclk = XCLK_GPIO_NUM;

    _cam_config.pin_sscb_sda = SIOD_GPIO_NUM;
    _cam_config.pin_sscb_scl = SIOC_GPIO_NUM;

    _cam_config.pin_d7 = Y9_GPIO_NUM;
    _cam_config.pin_d6 = Y8_GPIO_NUM;
    _cam_config.pin_d5 = Y7_GPIO_NUM;
    _cam_config.pin_d4 = Y6_GPIO_NUM;
    _cam_config.pin_d3 = Y5_GPIO_NUM;
    _cam_config.pin_d2 = Y4_GPIO_NUM;
    _cam_config.pin_d1 = Y3_GPIO_NUM;
    _cam_config.pin_d0 = Y2_GPIO_NUM;
    _cam_config.pin_vsync = VSYNC_GPIO_NUM;
    _cam_config.pin_href = HREF_GPIO_NUM;
    _cam_config.pin_pclk = PCLK_GPIO_NUM;
    _cam_config.xclk_freq_hz = 20000000;
    _cam_config.ledc_timer = LEDC_TIMER_0;
    _cam_config.ledc_channel = LEDC_CHANNEL_0;
    _cam_config.pixel_format = PIXFORMAT_JPEG;

    //init with high specs to pre-allocate larger buffers
    if(psramFound()){
      printf("config with PSRAM");
      _cam_config.frame_size = FRAMESIZE_UXGA;
      _cam_config.jpeg_quality = 10;
      _cam_config.fb_count = 2;
    } else {
      printf("config without PSRAM");
      _cam_config.frame_size = FRAMESIZE_SVGA;
      _cam_config.jpeg_quality = 12;
      _cam_config.fb_count = 1;
    }

    esp_err_t err = esp_camera_init(&_cam_config);
    if (err != ESP_OK)
    {
        printf("Camera probe failed with error 0x%x", err);
        return err;
    }
    // ESP_ERROR_CHECK(gpio_install_isr_service(0));

    return ESP_OK;
}
