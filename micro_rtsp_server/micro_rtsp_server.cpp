#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_wifi.h>
#include <tcpip_adapter.h>

#include <esp_timer.h>
#include <driver/gpio.h>

#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>

#include "micro_rtsp_server.h"
#include "OV2640.h"
#include "OV2640Streamer.h"
#include "CStreamer.h"

#include <esp_log.h>
#include <esp_console.h>

#if CONFIG_MICRO_RTSP_SERVER

//#define ENABLE_CAMERA_LED_ON_CONNECTION
#define LED_PIN (4)
#define CONFIG_EXAMPLE_IPV4 1
#define RTSP_PORT (8554)

const portTickType xDelayTime = 100 / portTICK_RATE_MS;
const portTickType xDelayTimeTask = 10 / portTICK_RATE_MS;
#define PRIORITY_SERVER 2
#define PRIORITY_TASK 1

static const char* TAG = "micro_rtsp_server";
// We wrap streamer around in context as we might want to pass some semaphore or flag to pause/stop streaming
struct _micro_rtsp_ctx
{
	CStreamer * streamer;
	TaskHandle_t handle_task;
	TaskHandle_t handle_server;
};
typedef struct _micro_rtsp_ctx micro_rtsp_ctx;

micro_rtsp_ctx mctx;

void micro_rtsp_server(void *param)
{
	micro_rtsp_ctx* ctx = (micro_rtsp_ctx*)param;
	char addr_str[128]; // cant we shorten this if we are doing ipv4
	int addr_family;
	int ip_protocol;
	
	while (1) {
#ifdef CONFIG_EXAMPLE_IPV4
		struct sockaddr_in dest_addr;
		dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		dest_addr.sin_family = AF_INET;
		dest_addr.sin_port = htons(RTSP_PORT);
		addr_family = AF_INET;
		ip_protocol = IPPROTO_IP;
		inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);
#else
		struct sockaddr_int6 dest_addr;
		bzero(&dest_addr.sin6_addr.un, sizeof(dest_addr.sin6_addr.un));
		dest_addr.sin6_family = AF_INET6;
		dest_addr.sin6_port = htons(RTSP_PORT);
		addr_family = AF_INET6;
		ip_protocol = IPPROTO_IPV6;
		inet6_ntoa_r(dest_addr.sin6_addr, addr_str, sizeof(addr_str) - 1);
#endif
		int listen_sock = socket(addr_family, SOCK_STREAM 
				//| SOCK_NONBLOCK
				, ip_protocol); // SOCK_DGRAM for UDP
		if (listen_sock < 0) {
			ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
			break;
		}
		ESP_LOGI(TAG, "Socket created");
		
		int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
		if (err != 0) {
			ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
			break;
		}
		ESP_LOGI(TAG, "Socket bound, port %d", RTSP_PORT);

		err = listen(listen_sock, 8);
		if (err != 0) {
			ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
			break;
		}
		ESP_LOGI(TAG, "Socket listening");
		struct sockaddr_in6 source_addr; // Large enough for both IPv4 or IPv6
		uint addr_len = sizeof(source_addr);
		while (1) {
			int client_sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
			if (client_sock > 0)
			{
#ifdef CONFIG_EXAMPLE_IPV4
			ESP_LOGI(TAG, "Client connected. Client address: %s", inet_ntoa(((struct sockaddr_in*)&source_addr)->sin_addr));
#else
			ESP_LOGI(TAG, "Client connected.");
#endif
#ifdef ENABLE_CAMERA_LED_ON_CONNECTION
			digitalWrite(LED_PIN, HIGH);
#endif
                        ctx->streamer->addSession(client_sock);
			vTaskDelay(xDelayTime);
			}
		}


	}
}

void micro_rtsp_task(void *param)
{
	micro_rtsp_ctx* ctx = (micro_rtsp_ctx*)param;
	// stream frames

	const uint32_t msecPerFrame = 100;
	const uint32_t usecPerFrame = msecPerFrame * 1000;
	uint64_t lastimage = esp_timer_get_time();
	static uint32_t littlecounter = 0;
	while(1) {
		// If we have an active client connection, just service that until gone
		//ESP_LOGI(TAG, "Frame task - start %llu", esp_timer_get_time());
		uint64_t strt = esp_timer_get_time();
		ctx->streamer->handleRequests(0); // we don't use a timeout here,
		uint64_t end = esp_timer_get_time();
		ESP_LOGI(TAG, "handleRequests %llu", end-strt);
		// instead we send only if we have new enough frames
		uint64_t now = esp_timer_get_time();
		//ESP_LOGI(TAG, "Frame task - middl %llu", now);
		if(ctx->streamer->anySessions()) {
			if(now > lastimage + usecPerFrame || now < lastimage) { // handle clock rollover
				//ESP_LOGI(TAG, "Frame(%u)", littlecounter++);
				ctx->streamer->streamImage(now);
				lastimage = now;
				// check if we are overrunning our max frame rate			
				now = esp_timer_get_time();
				if(now > lastimage + usecPerFrame) {
					printf("warning exceeding max frame rate of %llu ms\n", (now - lastimage)/1000);
				}
			}
		} else {
#ifdef ENABLE_CAMERA_LED_ON_CONNECTION
			// led off
        	        digitalWrite(LED_PIN, LOW);
#endif
	        }
		//ESP_LOGI(TAG, "Frame task - end   %llu", esp_timer_get_time());
		//vTaskDelay(xDelayTimeTask);
		yieldthread();
	}
}


void* init_camera_cpp(void)
{
	static int initialized = 0;
        static OV2640 camera;
	if (initialized) {
		return &camera;
	}

    // from esp32-camera sample
    if(esp32cam_aithinker_config.pin_pwdn != -1) {
        //pinMode(esp32cam_aithinker_config.pin_pwdn, OUTPUT);
	//digitalWrite(esp32cam_config.pin_pwdn, LOW);
        gpio_set_direction((gpio_num_t)esp32cam_aithinker_config.pin_pwdn, GPIO_MODE_OUTPUT);
	gpio_set_level((gpio_num_t)esp32cam_config.pin_pwdn, 0);
    }


	esp_err_t err = camera.init(esp32cam_aithinker_config);
	if (err != ESP_OK)
	{
		return NULL;
	}
	initialized = 1;

	return (void*)&camera;
}

int start_micro_rstp_cpp(void* camera)	
{
	static uint initialized = 0;
	if (initialized) {
		return -404;
	}

	mctx.handle_task = NULL;
	mctx.handle_server = NULL;
	mctx.streamer = NULL;

	/* 1. Create all necessary things in this function
	 * 2. Wrap around funcionality from "main" loop into the function
	 * 3. Create a classic FreeRTOS task
	 */
        if (camera == NULL)
	{
		return -1;
	}

	mctx.streamer = new OV2640Streamer(*((OV2640*)camera));

#ifdef ENABLE_CAMERA_LED_ON_CONNECTION
	pinMode(LED_PIN, OUTPUT);
        digitalWrite(LED_PIN, LOW);
#endif
	BaseType_t xReturned;
	xReturned = xTaskCreate(micro_rtsp_task, "micro_rtsp_task", 4*1024, (void*)&mctx, PRIORITY_TASK, &mctx.handle_task);
	if (xReturned != pdPASS)
	{
		return -3;
	}

	xReturned = xTaskCreate(micro_rtsp_server, "micro_rtsp_server", 4*1024, (void*)&mctx, PRIORITY_SERVER, &mctx.handle_server);
	if (xReturned != pdPASS)
	{
		return -4;
	}

	return 0;
}

// C Wraps
#ifdef __cplusplus
extern "C" {
#endif
int start_micro_rtsp_server(void* camera)
{
    return start_micro_rstp_cpp(camera);
}

void* initialize_camera(void)
{
	return init_camera_cpp();
}

static int start_server(int argc, char **argv)
{
	ESP_LOGI(TAG, "start_server - init_camera");
	void* cam = initialize_camera();
	ESP_LOGI(TAG, "start_server - start_micro_rtsp_server");
	return start_micro_rtsp_server(cam);
}

void register_micro_rtsp(void)
{
    const esp_console_cmd_t join_cmd = {
        .command = "micro_rtsp",
        .help = "Runs micro_rtsp server",
        .hint = NULL,
        .func = &start_server,
        .argtable = NULL
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&join_cmd) );
}

#ifdef __cplusplus
}
#endif

#endif //CONFIG_MICRO_RTSP_SERVER
