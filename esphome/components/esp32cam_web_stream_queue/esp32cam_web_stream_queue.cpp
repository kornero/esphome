#include "esp32cam_web_stream_queue.h"

// using namespace esphome;
namespace esphome {
namespace esp32cam_web_stream_queue {

#define PART_BOUNDARY "imgboundary"
static const char *STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *STREAM_CHUNK_BOUNDARY = "--" PART_BOUNDARY "\r\n";
static const char *STREAM_CHUNK_CONTENT_TYPE = "Content-Type: %s\r\n";
static const char *STREAM_CHUNK_CONTENT_LENGTH = "Content-Length: %u\r\n";
static const char *STREAM_CHUNK_NEW_LINE = "\r\n";

static const char *JPG_CONTENT_TYPE = "image/jpeg";

static const char *const TAG_WEB_CAM = "web_cam";

static const int ESP32CAM_WEB_CHUNK_MAX_FPS = 25;
static const int ESP32CAM_WEB_CHUNK_MAX_RATE = 1000 / ESP32CAM_WEB_CHUNK_MAX_FPS;  // 15 fps

static camera_fb_t *ESP32CAM_WEB_CHUNK_FB;

int ESP32CAM_WEB_CHUNK_STEP = 0;
size_t ESP32CAM_WEB_CHUNK_SENT = 0;
uint32_t ESP32CAM_WEB_CHUNK_LAST_UPDATE;

Esp32CamWebStreamQueue *global_esp32cam_web_stream_queue;

void Esp32CamWebStreamQueue::handleRequest(AsyncWebServerRequest *req) {
  if (millis() - ESP32CAM_WEB_CHUNK_LAST_UPDATE < 5000) {
    ESP_LOGE(TAG_WEB_CAM, "Already streaming!");
    req->send(500, "text/plain", "Already streaming!");
  }

  resetSteps();

  digitalWrite(33, LOW);  // Turn on

  AsyncWebServerResponse *response =
      req->beginChunkedResponse(STREAM_CONTENT_TYPE, [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
        try {
          if (ESP32CAM_WEB_CHUNK_SENT == -1) {
            while (millis() - ESP32CAM_WEB_CHUNK_LAST_UPDATE < ESP32CAM_WEB_CHUNK_MAX_RATE) {
              delay(10);
            }

            if (xQueueReceive(global_esp32cam_web_stream_queue->ESP32CAM_WEB_FB_GET_QUEUE, &ESP32CAM_WEB_CHUNK_FB,
                              0L) != pdTRUE) {
              // no frame ready
              ESP_LOGD(TAG_WEB_CAM, "No frame ready");
              return RESPONSE_TRY_AGAIN;
            }

            //            ESP32CAM_WEB_CHUNK_FB = esp_camera_fb_get();
            if (!ESP32CAM_WEB_CHUNK_FB) {
              ESP_LOGE(TAG_WEB_CAM, "Camera capture failed");

              xQueueSend(global_esp32cam_web_stream_queue->ESP32CAM_WEB_FB_RETURN_QUEUE, &ESP32CAM_WEB_CHUNK_FB,
                         portMAX_DELAY);

              esp_camera_deinit();
              initCamera();

              return 0;
            }

            ESP32CAM_WEB_CHUNK_LAST_UPDATE = millis();
            ESP32CAM_WEB_CHUNK_SENT = 0;
          }

          switch (ESP32CAM_WEB_CHUNK_STEP) {
            case 0: {
              size_t i = strlen(STREAM_CHUNK_BOUNDARY);

              if (maxLen < i) {
                ESP_LOGW(TAG_WEB_CAM, "Not enough space for boundary");
                return RESPONSE_TRY_AGAIN;
              }

              memcpy(buffer, STREAM_CHUNK_BOUNDARY, i);

              ESP32CAM_WEB_CHUNK_STEP++;

              return i;
            }

            case 1: {
              if (maxLen < (strlen(STREAM_CHUNK_CONTENT_TYPE) + strlen(JPG_CONTENT_TYPE))) {
                ESP_LOGW(TAG_WEB_CAM, "Not enough space for content type");
                return RESPONSE_TRY_AGAIN;
              }

              size_t i = sprintf((char *) buffer, STREAM_CHUNK_CONTENT_TYPE, JPG_CONTENT_TYPE);

              ESP32CAM_WEB_CHUNK_STEP++;
              ESP32CAM_WEB_CHUNK_STEP++;  // Skip content length.

              return i;
            }

            case 2: {
              if (maxLen < (strlen(STREAM_CHUNK_CONTENT_LENGTH) + 8)) {
                ESP_LOGW(TAG_WEB_CAM, "Not enough space for content length");
                return RESPONSE_TRY_AGAIN;
              }

              size_t i = sprintf((char *) buffer, STREAM_CHUNK_CONTENT_LENGTH, ESP32CAM_WEB_CHUNK_FB->len);

              ESP32CAM_WEB_CHUNK_STEP++;

              return i;
            }

            case 3: {
              size_t i = strlen(STREAM_CHUNK_NEW_LINE);
              if (maxLen < i) {
                ESP_LOGW(TAG_WEB_CAM, "Not enough space for new line");
                return RESPONSE_TRY_AGAIN;
              }

              memcpy(buffer, STREAM_CHUNK_NEW_LINE, i);

              ESP32CAM_WEB_CHUNK_STEP++;

              return i;
            }

            case 4: {
              size_t i = ESP32CAM_WEB_CHUNK_FB->len - ESP32CAM_WEB_CHUNK_SENT;
              size_t m = maxLen;

              if (m < i) {
                memcpy(buffer, ESP32CAM_WEB_CHUNK_FB->buf + ESP32CAM_WEB_CHUNK_SENT, m);
                ESP32CAM_WEB_CHUNK_SENT += m;
                return m;
              }

              memcpy(buffer, ESP32CAM_WEB_CHUNK_FB->buf + ESP32CAM_WEB_CHUNK_SENT, i);

              xQueueSend(global_esp32cam_web_stream_queue->ESP32CAM_WEB_FB_RETURN_QUEUE, &ESP32CAM_WEB_CHUNK_FB,
                         portMAX_DELAY);
              //              esp_camera_fb_return(ESP32CAM_WEB_CHUNK_FB);
              ESP32CAM_WEB_CHUNK_SENT = -1;

              ESP32CAM_WEB_CHUNK_STEP++;

              return i;
            }

            case 5: {
              size_t i = strlen(STREAM_CHUNK_NEW_LINE);
              if (maxLen < i) {
                ESP_LOGW(TAG_WEB_CAM, "Not enough space for new line");
                return RESPONSE_TRY_AGAIN;
              }

              memcpy(buffer, STREAM_CHUNK_NEW_LINE, i);

              ESP32CAM_WEB_CHUNK_STEP = 0;

              return i;
            }

            default:
              ESP_LOGE(TAG_WEB_CAM, "Wrong step %d", ESP32CAM_WEB_CHUNK_STEP);

              return 0;
          }
        } catch (...) {
          ESP_LOGE(TAG_WEB_CAM, "EXCEPTION !");
          return 0;
        }
      });

  response->addHeader("Access-Control-Allow-Origin", "*");
  req->onDisconnect([]() -> void {
    ESP_LOGI(TAG_WEB_CAM, "Disconnected.");

//    resetSteps();

    digitalWrite(33, HIGH);  // Turn off
  });

  req->send(response);
}

void Esp32CamWebStreamQueue::resetSteps() {
  // Clear from old stream.
  if (ESP32CAM_WEB_CHUNK_FB) {
    xQueueSend(global_esp32cam_web_stream_queue->ESP32CAM_WEB_FB_RETURN_QUEUE, &ESP32CAM_WEB_CHUNK_FB, 0L);
  }

  //    esp_camera_fb_return(ESP32CAM_WEB_CHUNK_FB);
  ESP32CAM_WEB_CHUNK_SENT = -1;
  ESP32CAM_WEB_CHUNK_STEP = 0;
  ESP32CAM_WEB_CHUNK_LAST_UPDATE = millis();
}

void Esp32CamWebStreamQueue::setup() {
  global_esp32cam_web_stream_queue = this;

  this->base_->init();

  initCamera();

  this->ESP32CAM_WEB_FB_GET_QUEUE = xQueueCreate(1, sizeof(camera_fb_t *));
  this->ESP32CAM_WEB_FB_RETURN_QUEUE = xQueueCreate(1, sizeof(camera_fb_t *));

  xTaskCreatePinnedToCore(&Esp32CamWebStreamQueue::esp32cam_web_fb_task,
                          "esp32cam_web_fb_task",  // name
                          1024,                    // stack size
                          nullptr,                 // task pv params
                          0,                       // priority
                          nullptr,                 // handle
                          1                        // core
  );

  this->base_->add_handler(this);
}

float Esp32CamWebStreamQueue::get_setup_priority() const { return setup_priority::AFTER_WIFI; }

void Esp32CamWebStreamQueue::esp32cam_web_fb_task(void *pv) {
  //  ESP_LOGD("esp32cam_web_fb_task", "Started!");
  while (true) {
    camera_fb_t *framebuffer = esp_camera_fb_get();
    xQueueSend(global_esp32cam_web_stream_queue->ESP32CAM_WEB_FB_GET_QUEUE, &framebuffer, portMAX_DELAY);
    // return is no-op for config with 1 fb
    xQueueReceive(global_esp32cam_web_stream_queue->ESP32CAM_WEB_FB_RETURN_QUEUE, &framebuffer, portMAX_DELAY);
    esp_camera_fb_return(framebuffer);
  }
}

void Esp32CamWebStreamQueue::dump_config() {
  if (psramFound()) {
    ESP_LOGCONFIG(TAG_WEB_CAM, "PSRAM");
  } else {
    ESP_LOGCONFIG(TAG_WEB_CAM, "PSRAM not found.");
  }

  ESP_LOGCONFIG(TAG_WEB_CAM, "Max FPS %d.", ESP32CAM_WEB_CHUNK_MAX_FPS);
}

// Initialise cameras
void Esp32CamWebStreamQueue::initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 5;         // Y2_GPIO_NUM;
  config.pin_d1 = 18;        // Y3_GPIO_NUM;
  config.pin_d2 = 19;        // Y4_GPIO_NUM;
  config.pin_d3 = 21;        // Y5_GPIO_NUM;
  config.pin_d4 = 36;        // Y6_GPIO_NUM;
  config.pin_d5 = 39;        // Y7_GPIO_NUM;
  config.pin_d6 = 34;        // Y8_GPIO_NUM;
  config.pin_d7 = 35;        // Y9_GPIO_NUM;
  config.pin_xclk = 0;       // XCLK_GPIO_NUM;
  config.pin_pclk = 22;      // PCLK_GPIO_NUM;
  config.pin_vsync = 25;     // VSYNC_GPIO_NUM;
  config.pin_href = 23;      // HREF_GPIO_NUM;
  config.pin_sscb_sda = 26;  // SIOD_GPIO_NUM;
  config.pin_sscb_scl = 27;  // SIOC_GPIO_NUM;
  config.pin_pwdn = 32;      // PWDN_GPIO_NUM;
  config.pin_reset = -1;     // RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;

  // Try to use RAW
  //  config.pixel_format = PIXFORMAT_RGB565;
  //  config.frame_size = FRAMESIZE_QVGA;

  //  config.pixel_format = PIXFORMAT_JPEG;
  //  config.frame_size = FRAMESIZE_QVGA;
  //  config.jpeg_quality = 12;

  if (psramFound()) {
    ESP_LOGCONFIG(TAG_WEB_CAM, "PSRAM");
    config.fb_count = 2;
  } else {
    ESP_LOGCONFIG(TAG_WEB_CAM, "PSRAM not found.");
    config.fb_count = 1;
  }

  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 10;
  config.fb_count = 2;

  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    ESP_LOGCONFIG(TAG_WEB_CAM, "Camera init failed!");
    return;
  }
}

}  // namespace esp32cam_web_stream_queue
}  // namespace esphome