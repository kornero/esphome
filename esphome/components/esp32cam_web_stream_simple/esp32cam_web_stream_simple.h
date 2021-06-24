#pragma once

#include "esphome.h"

#include <esp_camera.h>
#include <ESPAsyncWebServer.h>

namespace esphome {
namespace esp32cam_web_stream_simple {

class Esp32CamWebStreamSimple : public AsyncWebHandler, public Component {
 public:
  Esp32CamWebStreamSimple(web_server_base::WebServerBase *base) : base_(base) {}

  bool canHandle(AsyncWebServerRequest *request) override {
    if (request->method() == HTTP_GET) {
      if (request->url() == "/stream")
        return true;
    }

    return false;
  }

  void setup() override;

  float get_setup_priority() const override;

  void handleRequest(AsyncWebServerRequest *req) override;

  void dump_config() override;
  //  void loop() override {
  //  }

 protected:
  web_server_base::WebServerBase *base_;

  static void initCamera();
  static void resetSteps();
};

extern Esp32CamWebStreamSimple *global_esp32cam_web_stream_simple;

}  // namespace esp32cam_web_stream_simple
}  // namespace esphome