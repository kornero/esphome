#pragma once

#include "esphome.h"

//#include "esphome/components/base_esp32cam/base_esp32cam.h"
//#include "esphome/components/base_image_web_stream/base_image_web_stream.h"
//#include <base_esp32cam.h>
//#include <base_image_web_stream.h>
//#include "base_esp32cam.h"
#include "base_image_web_stream.h"
#include "AsyncRTSP.h"

namespace esphome {
namespace esp32cam_web_stream_rtsp {

class Esp32CamWebStreamRtsp : public Component {
 public:
  Esp32CamWebStreamRtsp(web_server_base::WebServerBase *base) : base_(base) {}

  void setup() override;

  float get_setup_priority() const override;

  void dump_config() override;

  void loop();
  //  void loop() override {
  //  }

 protected:
  web_server_base::WebServerBase *base_;
  AsyncRTSPServer *server;
  base_esp32cam::BaseEsp32Cam *baseEsp32Cam_;
  base_image_web_stream::BaseImageWebStream *baseImageWebStream_;

 private:
  dimensions parse_camera_dimensions_(camera_config_t config);
};

}  // namespace esp32cam_web_stream_rtsp
}  // namespace esphome