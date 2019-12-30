idf_component_register(#SRCS "micro_rtsp_server.c"
	            SRC_DIRS "." "../src"
                    INCLUDE_DIRS "." "../src"
                    REQUIRES freertos esp_common esp32-camera console)

	    #git_describe(MICRO_RTSP_SERVER_VER "../")

