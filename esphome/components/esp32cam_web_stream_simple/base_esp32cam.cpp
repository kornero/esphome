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

  this->lock = xSemaphoreCreateMutex();

  global_base_esp32cam = this;
  /*
    this->queue_get = xQueueCreate(1, sizeof(camera_fb_t *));
    this->queue_return = xQueueCreate(1, sizeof(camera_fb_t *));

    xTaskCreate(&BaseEsp32Cam::esp32cam_fb_task,
                "esp32cam_fb_task",                     // name
                1024,                                   // stack size
                nullptr,                                // task pv params
                ESP_TASK_PRIO_MAX | portPRIVILEGE_BIT,  // priority
                nullptr                                 // handle
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
  //  while (millis() - this->last_update_ < this->max_rate_) {
  //    yield();
  //  }
  if (millis() - this->last_update_ < this->max_rate_) {
    return nullptr;
  }

  xSemaphoreTake(this->lock, portMAX_DELAY);
  this->release_no_lock();

  this->fb_ = esp_camera_fb_get();
  if (this->fb_ == nullptr) {
    ESP_LOGE(TAG, "Camera error! Can't get FB.");
  } else {
    ESP_LOGD(TAG, "Got FB ( %d x %d ) = [ %d ].", this->fb_->width, this->fb_->height, this->fb_->len);

    if (this->fb_->len == 0) {
      ESP_LOGE(TAG, "Camera error! Got corrupted FB ( %d x %d ) = [ %d ].", this->fb_->width, this->fb_->height,
               this->fb_->len);

      release_no_lock();
    }
  }

  this->last_update_ = millis();
  xSemaphoreGive(this->lock);
  return this->fb_;
}

void BaseEsp32Cam::release() {
  //  xSemaphoreTake(this->lock, 0);
  xSemaphoreTake(this->lock, portMAX_DELAY);
  this->release_no_lock();
  xSemaphoreGive(this->lock);
}

void BaseEsp32Cam::release_no_lock() {
  //  xSemaphoreTake(this->lock, 0);
  if (this->fb_ != nullptr) {
    ESP_LOGD(TAG, "Released FB ( %d x %d ) = [ %d ].", this->fb_->width, this->fb_->height, this->fb_->len);
    esp_camera_fb_return(this->fb_);
    this->fb_ = nullptr;
  }
}

camera_fb_t *BaseEsp32Cam::get_fb() {
  return esp_camera_fb_get();
  /*
  camera_fb_t *fb;
  xQueueReceive(global_base_esp32cam->queue_get, &fb, portMAX_DELAY);

  if (!fb) {
    ESP_LOGE(TAG, "get_fb(): Camera capture failed!");

    xQueueSend(global_base_esp32cam->queue_return, &fb, 0);

    //    esp_camera_deinit();
    //    this->init_camera();

    //    throw std::runtime_error("Camera capture failed!");

    return nullptr;
  }

  return fb;
   */
}

camera_fb_t *BaseEsp32Cam::get_fb_nowait() {
  return this->get_fb();
  /*
  camera_fb_t *fb;
  if (xQueueReceive(global_base_esp32cam->queue_get, &fb, 0L) != pdTRUE) {
    ESP_LOGV(TAG, "get_fb_nowait: null");

    return nullptr;
  }

  if (!fb) {
    ESP_LOGE(TAG, "get_fb_nowait(): Camera capture failed!");

    xQueueSend(global_base_esp32cam->queue_return, &fb, 0);

    //    esp_camera_deinit();
    //    this->init_camera();
    throw std::runtime_error("Camera capture failed!");

    //    return NULL;
  }

  return fb;
   */
}

void BaseEsp32Cam::return_fb(camera_fb_t *fb) {
  if (fb != nullptr) {
    esp_camera_fb_return(fb);
    /*
    if (xQueueSend(global_base_esp32cam->queue_return, &fb, portMAX_DELAY) != pdTRUE) {
      ESP_LOGE(TAG, "return_fb(): can't return.");
    }*/
  } else {
    ESP_LOGW(TAG, "return_fb_nowait(): fb is null.");
  }
}

void BaseEsp32Cam::return_fb_nowait(camera_fb_t *fb) {
  this->return_fb(fb);
  /*
  if (fb != nullptr) {
    if (xQueueSend(global_base_esp32cam->queue_return, &fb, 0) != pdTRUE) {
      ESP_LOGW(TAG, "return_fb_nowait(): can't return.");
    }
  } else {
    ESP_LOGI(TAG, "return_fb_nowait(): fb is null.");
  }*/
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