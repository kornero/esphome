#pragma once

#include "esphome.h"

#include <esp_camera.h>

namespace esphome {
namespace base_esp32cam {

class BaseEsp32Cam {
 public:

  void init_camera();

  camera_fb_t* get_fb();

  void return_fb(camera_fb_t * fb);
};

}
}  // namespace esphome