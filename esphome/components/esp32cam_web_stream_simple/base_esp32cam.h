#pragma once

//#include "esphome.h"

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

  camera_fb_t *get_fb();
  camera_fb_t *get_fb_nowait();

  void return_fb(camera_fb_t *fb);
  void return_fb_nowait(camera_fb_t *fb);

 protected:
  QueueHandle_t queue_get;
  QueueHandle_t queue_return;
  SemaphoreHandle_t lock;

  camera_fb_t *fb_;
  uint32_t last_update_;
  int max_fps_;
  int max_rate_;

  static void esp32cam_fb_task(void *pv);

 private:
  void release_no_lock();
};

extern BaseEsp32Cam *global_base_esp32cam;

}  // namespace base_esp32cam
}  // namespace esphome