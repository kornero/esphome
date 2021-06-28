#include "esphome.h"

#include "base_esp32cam.h"

namespace esphome {
namespace base_esp32cam {

static const char *const TAG = "base_esp32cam";

BaseEsp32Cam *global_base_esp32cam;

void BaseEsp32Cam::setup() {
  this->init_camera();

  this->fb_ = nullptr;
  this->last_update_ = millis();
  this->max_fps_ = ESP32CAM_MAX_FPS;
  this->max_rate_ = 1000 / this->max_fps_;  // 15 fps

  ESP_LOGCONFIG(TAG, "Max FPS %d.", this->max_fps_);

  this->lock_ = xSemaphoreCreateMutex();

  global_base_esp32cam = this;

  this->queue_get_ = xQueueCreate(1, sizeof(camera_fb_t *));
  this->queue_return_ = xQueueCreate(1, sizeof(camera_fb_t *));

  xTaskCreate(&BaseEsp32Cam::esp32cam_fb_task,
              "esp32cam_fb_task",    // name
              ESP_TASK_TCPIP_STACK,  // stack size
              nullptr,               // task pv params
              ESP_TASK_TCPIP_PRIO,   // priority
              nullptr                // handle
  );

  /*
  xTaskCreatePinnedToCore(&BaseEsp32Cam::esp32cam_fb_task,
                          "esp32cam_fb_task",  // name
                          1024,                // stack size
                          nullptr,             // task pv params
                          0,                   // priority
                          nullptr,             // handle
                          1                    // core
  );
   */
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

  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Camera init failed!");
    // Here:
    //    this->mark_failed();

    // In dump config:
    //    if (this->is_failed()) {
    //      ESP_LOGE(TAG, "  Setup Failed: %s", esp_err_to_name(this->init_error_));
    //      return;
    //    }
    return;
  }
}

camera_fb_t *BaseEsp32Cam::current() { return this->fb_; }

camera_fb_t *BaseEsp32Cam::next() {
  if (millis() - this->last_update_ < this->max_rate_) {
    return nullptr;
  }

  xSemaphoreTake(this->lock_, portMAX_DELAY);
  this->release_no_lock_();

  xQueueReceive(global_base_esp32cam->queue_get_, &this->fb_, portMAX_DELAY);
  if (this->fb_ == nullptr) {
    ESP_LOGE(TAG, "Camera error! Can't get FB.");
  } else if (this->fb_->len == 0) {
    ESP_LOGE(TAG, "Camera error! Got corrupted FB ( %d x %d ) = [ %d ].", this->fb_->width, this->fb_->height,
             this->fb_->len);

    release_no_lock_();
  }

  this->last_update_ = millis();
  xSemaphoreGive(this->lock_);
  return this->fb_;
}

void BaseEsp32Cam::release() {
  xSemaphoreTake(this->lock_, portMAX_DELAY);
  this->release_no_lock_();
  xSemaphoreGive(this->lock_);
}

void BaseEsp32Cam::release_no_lock_() {
  if (this->fb_ != nullptr) {
    xQueueSend(global_base_esp32cam->queue_return_, &this->fb_, portMAX_DELAY);
    this->fb_ = nullptr;
  }
}

void BaseEsp32Cam::esp32cam_fb_task(void *pv) {
  while (true) {
    camera_fb_t *fb = esp_camera_fb_get();
    xQueueSend(global_base_esp32cam->queue_get_, &fb, portMAX_DELAY);
    // return is no-op for config with 1 fb
    xQueueReceive(global_base_esp32cam->queue_return_, &fb, portMAX_DELAY);
    esp_camera_fb_return(fb);
  }
}

}  // namespace base_esp32cam
}  // namespace esphome