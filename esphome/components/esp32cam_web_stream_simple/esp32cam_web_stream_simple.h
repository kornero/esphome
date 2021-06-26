#pragma once

#include "esphome.h"

//#include "esphome/components/base_esp32cam/base_esp32cam.h"
//#include "esphome/components/base_image_web_stream/base_image_web_stream.h"
//#include <base_esp32cam.h>
//#include <base_image_web_stream.h>
#include "base_esp32cam.h"
#include "base_image_web_stream.h"

namespace esphome {
namespace esp32cam_web_stream_simple {

class Esp32CamWebStreamSimple : public Component {
 public:
  Esp32CamWebStreamSimple(web_server_base::WebServerBase *base)
      :
        base_(base),
        baseEsp32Cam_(new esphome::base_esp32cam::BaseEsp32Cam())
//        baseImageWebStream_(new base_image_web_stream::BaseImageWebStream(
//            base_, baseEsp32Cam_
//            ))
  {
  }

  void setup() override;

  float get_setup_priority() const override;

  void dump_config() override;
  //  void loop() override {
  //  }

 protected:
  web_server_base::WebServerBase *base_;
  base_esp32cam::BaseEsp32Cam *baseEsp32Cam_;
  base_image_web_stream::BaseImageWebStream *baseImageWebStream_;
};

}  // namespace esp32cam_web_stream_simple
}  // namespace esphome