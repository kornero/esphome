#include "esp32cam_web_stream_simple.h"

// using namespace esphome;
namespace esphome {
namespace esp32cam_web_stream_simple {
void Esp32CamWebStreamSimple::setup() {
//  global_esp32cam_web_stream_simple = this;

//  this->base_->init();

//  initCamera();
//  this->base_->add_handler(this);

  base_esp32cam::BaseEsp32Cam *cam = new base_esp32cam::BaseEsp32Cam();

  base_image_web_stream::BaseImageWebStream *web = new base_image_web_stream::BaseImageWebStream(base_);
  web->set_cam(cam);

  this->baseImageWebStream_ = web;
  this->baseEsp32Cam_ = cam;

this->baseEsp32Cam_->init_camera();
this->baseImageWebStream_->setup();
}

float Esp32CamWebStreamSimple::get_setup_priority() const {
  return setup_priority::AFTER_WIFI;
}

void Esp32CamWebStreamSimple::dump_config() {
  this->baseImageWebStream_->dump_config();
}

}  // namespace esp32cam_web_stream_simple
}  // namespace esphome