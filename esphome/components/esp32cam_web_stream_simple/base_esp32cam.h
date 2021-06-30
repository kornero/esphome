#pragma once

#include <esp_camera.h>

namespace esphome {
namespace base_esp32cam {

static const int ESP32CAM_MAX_FPS = 25;

class BaseEsp32Cam {
 public:
  void setup();
  void init_camera();

  camera_fb_t *current();
  camera_fb_t *next();
  void release();

  camera_fb_t *current_or_next() {
    if (this->current() == nullptr) {
      return this->next();
    } else {
      return this->current();
    }
  }

 protected:
  SemaphoreHandle_t lock_;

  camera_fb_t *fb_;
  uint32_t last_update_;
  int max_fps_;
  int max_rate_;

 private:
  void release_no_lock_();
};

extern BaseEsp32Cam *global_base_esp32cam;

}  // namespace base_esp32cam
}  // namespace esphome