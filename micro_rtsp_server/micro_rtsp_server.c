#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_wifi.h>
#include <tcpip_adapter.h>

#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ENABLE_CAMERA_LED_ON_CONNECTION
#define LED_PIN (4)

#define RTSP_PORT (8554)

#ifdef ENABLE_RTSPSERVER

static const char* TAG = "micro_rtsp_server";
// We wrap streamer around in context as we might want to pass some semaphore or flag to pause/stop streaming
typedef struct _micro_rtsp_ctx
{
	CStreamer * streamer;
	TaskHandle_t handle;
} micro_rtsp_ctx;

micro_rtsp_ctx mctx = {0};

void micro_rtsp_server(void *param)
{
	micro_rtsp_ctx* ctx = (micro_rtsp_ctx*)param;
	char rx_buffer[128];
	char addr_str[128]; // cant we shorten this if we are doing ipv4
	int addr_family;
	int ip_protocol;
	
	while (1) {
#ifdef CONFIG_EXAMPLE_IPV4
		struct sockaddr_in dest_addr;
		dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		dest_addr.sin_family = AF_INET;
		dest_addr.sin_port = htons(RTSP_PORT);
		addr_family = AF_INTE;
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
		int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol); // SOCK_DGRAM for UDP
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

		err = listen(listen_sock, 16);
		if (err != 0) {
			ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
			break;
		}
		ESP_LOGI(TAG, "Socket listening");
		struct sockaddr_in6 source_addr; // Large enough for both IPv4 or IPv6
		uint addr_len = sizeof(source_addr);
		while (1) {
			int client_sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
			ESP_LOGI(TAG, "Client connected. Client address: %s", inet_ntoa(source_addr.sin_addr));
                        ctx->streamer->addSession(client_sock);
		}


	}
	// handle requests sent on rtsp
	WiFiClient rtspClient = rtspServer.accept();
	if(rtspClient) {
#ifdef ENABLE_CAMERA_LED_ON_CONNECTION
		digitalWrite(LED_PIN, HIGH);
#endif
		Serial.print("client: ");
		Serial.print(rtspClient.remoteIP());
		Serial.println();
		ctx->streamer->addSession(rtspClient);
	}
}

void micro_rtsp_task(void *param)
{
	micro_rtsp_ctx* ctx = (micro_rtsp_ctx*)param;
	// stream frames

	uint32_t msecPerFrame = 100;
	static uint32_t lastimage = millis();
	// If we have an active client connection, just service that until gone
	ctx->streamer->handleRequests(0); // we don't use a timeout here,
	// instead we send only if we have new enough frames
	uint32_t now = millis();
	if(ctx->streamer->anySessions()) {
		if(now > lastimage + msecPerFrame || now < lastimage) { // handle clock rollover
			ctx->streamer->streamImage(now);
			lastimage = now;
			// check if we are overrunning our max frame rate			
			now = millis();
			if(now > lastimage + msecPerFrame) {
				printf("warning exceeding max frame rate of %d ms\n", now - lastimage);
			}
		}
	} else {
#ifdef ENABLE_CAMERA_LED_ON_CONNECTION
		// led off
                digitalWrite(LED_PIN, LOW);
#endif
        }

}

OV2640 camera;
void* init_camera(void)
{
	esp_err_t err =	camera.init(esp32cam_aithinker_config);
	if (err != ESP_OK)
	{
		return NULL;
	}

	return (void*)&camera;
}

int start_micro_rtsp(void* camera)
{
	/* 1. Create all necessary things in this function
	 * 2. Wrap around funcionality from "main" loop into the function
	 * 3. Create a classic FreeRTOS task
	 */
        if (camera == NULL)
	{
		return -1;
	}

	//WiFiServer * rtspServer;
       
	rtspServer = new WiFiServer(8554);
	if (rtspServer == NULL)
	{
		return -2;
	}

	rtspServer->begin();
	mctx.streamer = new OV2640Streamer(*((OV2640*)cam));

#ifdef ENABLE_CAMERA_LED_ON_CONNECTION
	pinMode(LED_PIN, OUTPUT);
        digitalWrite(LED_PIN, LOW);
#endif
	BaseType_t xReturned;
	xReturned = xTaskCreate(micro_rtsp_task, "micro_rtsp_task", 4*1024, (void*)&mctx, 5, &mctx.handle);
	if (xReturned != pdPASS)
	{
		return -3;
	}

	return 0;
}

#endif
#ifdef __cplusplus
}
#endif
