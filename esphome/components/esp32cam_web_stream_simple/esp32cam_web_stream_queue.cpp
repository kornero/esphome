#include "esp32cam_web_stream_simple.h"

// using namespace esphome;
namespace esphome {
namespace esp32cam_web_stream_simple {
void Esp32CamWebStreamSimple::setup() {
//  global_esp32cam_web_stream_simple = this;

//  this->base_->init();

//  initCamera();

//  this->base_->add_handler(this);
this->baseEsp32Cam_->initCamera();
this->baseImageWebStream_->setup();
}

float Esp32CamWebStreamSimple::get_setup_priority() const { return this->baseImageWebStream_->get_setup_priority(); }

void Esp32CamWebStreamSimple::dump_config() {
  this->baseImageWebStream_->dump_config();
}

}  // namespace esp32cam_web_stream_simple
}  // namespace esphome