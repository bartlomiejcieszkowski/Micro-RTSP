#ifndef COMPONENT_MICRO_RTSP_H
#define COMPONENT_MICRO_RTSP_H

#include "esp_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

void* init_camera(void);
int start_micro_rtsp_server(void* camera);


#ifdef __cplusplus
}
#endif
#endif // COMPONENT_MICRO_RTSP_H
