#include "esp32cam_web_stream_rtsp.h"

// using namespace esphome;
namespace esphome {
namespace esp32cam_web_stream_rtsp {

static const char *const TAG = "esp32cam_web_stream_rtsp";

void Esp32CamWebStreamRtsp::setup() {
  ESP_LOGI(TAG, "enter setup");

  base_esp32cam::BaseEsp32Cam *cam = new base_esp32cam::BaseEsp32Cam();
  cam->setup();

  ESP_LOGI(TAG, "Cam.... ok.");

  base_image_web_stream::BaseImageWebStream *web = new base_image_web_stream::BaseImageWebStream(this->base_, cam);
  //  web->set_cam(cam);
  web->setup();

  ESP_LOGI(TAG, "Web.... ok.");

  this->baseImageWebStream_ = web;
  this->baseEsp32Cam_ = cam;
}

float Esp32CamWebStreamRtsp::get_setup_priority() const { return setup_priority::AFTER_WIFI; }

void Esp32CamWebStreamRtsp::dump_config() { this->baseImageWebStream_->dump_config(); }

}  // namespace esp32cam_web_stream_rtsp
}  // namespace esphome