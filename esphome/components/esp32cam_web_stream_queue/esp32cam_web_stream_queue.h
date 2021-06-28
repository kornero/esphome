#pragma once

#include "esphome.h"

#include "base_image_web_stream.h"

namespace esphome {
namespace esp32cam_web_stream_queue {

class Esp32CamWebStreamQueue : public Component {
 public:
  Esp32CamWebStreamQueue(web_server_base::WebServerBase *base) : base_(base) {}

  void setup() override;

  float get_setup_priority() const override;

  void dump_config() override;

 protected:
  web_server_base::WebServerBase *base_;
  base_esp32cam::BaseEsp32Cam *baseEsp32Cam_;
  base_image_web_stream::BaseImageWebStream *baseImageWebStream_;
};

}  // namespace esp32cam_web_stream_queue
}  // namespace esphome