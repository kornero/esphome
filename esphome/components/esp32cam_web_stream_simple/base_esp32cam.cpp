#include "esphome.h"

#include "base_esp32cam.h"

namespace esphome {
namespace base_esp32cam {

static const char *const TAG = "base_esp32cam";

BaseEsp32Cam *global_base_esp32cam;

void BaseEsp32Cam::setup() {
  this->init_camera();

  global_base_esp32cam = this;

  this->queue_get = xQueueCreate(1, sizeof(camera_fb_t *));
  this->queue_return = xQueueCreate(1, sizeof(camera_fb_t *));

  xTaskCreatePinnedToCore(&BaseEsp32Cam::esp32cam_fb_task,
                          "esp32cam_fb_task",  // name
                          1024,                // stack size
                          nullptr,             // task pv params
                          0,                   // priority
                          nullptr,             // handle
                          1                    // core
  );
}

void BaseEsp32Cam::init_camera() {
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
    ESP_LOGI(TAG, "PSRAM");
    config.fb_count = 2;
  } else {
    ESP_LOGI(TAG, "PSRAM not found.");
    config.fb_count = 1;
  }

  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 10;
  config.fb_count = 2;

  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Camera init failed!");
    return;
  }
}

camera_fb_t *BaseEsp32Cam::get_fb() {
  ESP_LOGD(TAG, "get_fb >>>");

  camera_fb_t *fb;
  xQueueReceive(global_base_esp32cam->queue_get, &fb, portMAX_DELAY);

  if (!fb) {
    ESP_LOGE(TAG, "get_fb(): Camera capture failed!");

    xQueueSend(global_base_esp32cam->queue_return, &fb, 0);

//    esp_camera_deinit();
//    this->init_camera();

//    throw std::runtime_error("Camera capture failed!");

    return NULL;
  }

  ESP_LOGD(TAG, "<<< get_fb");
  return fb;
  //    return esp_camera_fb_get();
}

camera_fb_t *BaseEsp32Cam::get_fb_nowait() {
  ESP_LOGD(TAG, "get_fb_nowait >>>");

  camera_fb_t *fb;
  if (xQueueReceive(global_base_esp32cam->queue_get, &fb, 0L) != pdTRUE) {
    ESP_LOGD(TAG, "get_fb_nowait: null");

    return NULL;
  }

  if (!fb) {
    ESP_LOGE(TAG, "get_fb_nowait(): Camera capture failed!");

    xQueueSend(global_base_esp32cam->queue_return, &fb, 0);

//    esp_camera_deinit();
//    this->init_camera();
    throw std::runtime_error("Camera capture failed!");

//    return NULL;
  }

  ESP_LOGD(TAG, "<<< get_fb_nowait");
  return fb;
  //    return esp_camera_fb_get();
}

void BaseEsp32Cam::return_fb(camera_fb_t *fb) {
  ESP_LOGD(TAG, "return_fb >>>");

  if (fb != NULL) {
    if(xQueueSend(global_base_esp32cam->queue_return, &fb, portMAX_DELAY)!= pdTRUE) {
      ESP_LOGE(TAG, "return_fb(): can't return.");
    }
  }

  ESP_LOGD(TAG, "<<< return_fb");
}

void BaseEsp32Cam::return_fb_nowait(camera_fb_t *fb) {
  ESP_LOGD(TAG, "return_fb_nowait >>>");

  if (fb != NULL) {
    if(xQueueSend(global_base_esp32cam->queue_return, &fb, 0)!= pdTRUE) {
      ESP_LOGW(TAG, "return_fb_nowait(): can't return.");
    }
  }

  ESP_LOGD(TAG, "<<< return_fb_nowait");
}

void BaseEsp32Cam::esp32cam_fb_task(void *pv) {
  while (true) {
    camera_fb_t *fb = esp_camera_fb_get();
    xQueueSend(global_base_esp32cam->queue_get, &fb, portMAX_DELAY);
    // return is no-op for config with 1 fb
    xQueueReceive(global_base_esp32cam->queue_return, &fb, portMAX_DELAY);
    esp_camera_fb_return(fb);
  }
}

}  // namespace base_esp32cam
}  // namespace esphome