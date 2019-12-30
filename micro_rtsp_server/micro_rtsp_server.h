#ifndef COMPONENT_MICRO_RTSP_H
#define COMPONENT_MICRO_RTSP_H

#if CONFIG_MICRO_RTSP_SERVER

#include "esp_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

void* initialize_camera(void);
int start_micro_rtsp_server(void* camera);
void register_micro_rtsp(void);

#ifdef __cplusplus
}
#endif
#endif // CONFIG_MICRO_RTSP_SERVER
#endif // COMPONENT_MICRO_RTSP_H
