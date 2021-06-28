#include "esp32cam_web_stream_queue.h"

namespace esphome {
namespace esp32cam_web_stream_queue {

static const char *const TAG = "esp32cam_web_stream_queue";

void Esp32CamWebStreamQueue::setup() {
  ESP_LOGI(TAG, "enter setup");

  base_esp32cam::BaseEsp32Cam *cam = new base_esp32cam::BaseEsp32Cam();
  cam->setup();

  ESP_LOGI(TAG, "Cam.... ok.");

  base_image_web_stream::BaseImageWebStream *web = new base_image_web_stream::BaseImageWebStream(this->base_, cam);
  web->setup();

  ESP_LOGI(TAG, "Web.... ok.");

  this->baseImageWebStream_ = web;
  this->baseEsp32Cam_ = cam;
}

float Esp32CamWebStreamQueue::get_setup_priority() const { return setup_priority::AFTER_WIFI; }

void Esp32CamWebStreamQueue::dump_config() { this->baseImageWebStream_->dump_config(); }

}  // namespace esp32cam_web_stream_queue
}  // namespace esphome