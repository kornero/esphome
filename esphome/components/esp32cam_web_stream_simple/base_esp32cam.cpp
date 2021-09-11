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

  this->l_ = xSemaphoreCreateMutex();

  global_base_esp32cam = this;
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
  config.jpeg_quality = 63;

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

  this->lock_();

  // Second check after getting lock.
  if (millis() - this->last_update_ < this->max_rate_) {
    return nullptr;
  }

  // Alternative to critical section.
  // Prevent the real time kernel swapping out the task.
  //  vTaskSuspendAll();

  // Mutex for the critical section of switching the active frames around
  //  portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
  //  Do not allow interrupts while switching the current frame
  //  portENTER_CRITICAL(&mux);

  this->release_no_lock_();

  this->fb_ = esp_camera_fb_get();
  if (this->fb_ == nullptr) {
    ESP_LOGE(TAG, "Camera error! Can't get FB.");
  } else if (this->fb_->len == 0) {
    ESP_LOGE(TAG, "Camera error! Got corrupted FB ( %d x %d ) = [ %d ].", this->fb_->width, this->fb_->height,
             this->fb_->len);

    this->release_no_lock_();
  }

  this->last_update_ = millis();

  // Alternative to critical section.
  // The operation is complete. Restart the kernel.
  //  xTaskResumeAll();

  //  portEXIT_CRITICAL(&mux);

  this->unlock_();

  return this->fb_;
}

void BaseEsp32Cam::release() {
  this->lock_();
  this->release_no_lock_();
  this->unlock_();
}

void BaseEsp32Cam::release_no_lock_() {
  if (this->fb_ != nullptr) {
    esp_camera_fb_return(this->fb_);
    this->fb_ = nullptr;
  }
}

void BaseEsp32Cam::lock_() { xSemaphoreTake(this->l_, portMAX_DELAY); }

void BaseEsp32Cam::unlock_() { xSemaphoreGive(this->l_); }

}  // namespace base_esp32cam
}  // namespace esphome