#pragma once

#ifdef ARDUINO_ARCH_ESP32
#include "platglue-esp32.h"
#elif defined(ESP_IDF_ARCH_ESP32)
#include "platglue-espidf-esp32.h"
#else
#include "platglue-posix.h"
#endif
