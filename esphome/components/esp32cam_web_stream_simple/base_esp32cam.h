#pragma once

//#include "esphome.h"

#include <esp_camera.h>

namespace esphome {
namespace base_esp32cam {

class BaseEsp32Cam {
 public:
  QueueHandle_t queue_get;
  QueueHandle_t queue_return;

  void setup();
  void init_camera();

  camera_fb_t *get_fb();
  camera_fb_t *get_fb_nowait();

  void return_fb(camera_fb_t *fb);
  void return_fb_nowait(camera_fb_t *fb);

 protected:
  static void esp32cam_fb_task(void *pv);
};

extern BaseEsp32Cam *global_base_esp32cam;

}  // namespace base_esp32cam
}  // namespace esphome