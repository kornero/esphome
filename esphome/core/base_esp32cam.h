#pragma once

#include "esphome.h"

#include <esp_camera.h>

namespace esphome {
namespace base_esp32cam {

class BaseEsp32Cam {
 public:

  void initCamera();

  camera_fb_t* esp_camera_fb_get();
  void esp_camera_fb_return(camera_fb_t * fb);
};

}
}  // namespace esphome