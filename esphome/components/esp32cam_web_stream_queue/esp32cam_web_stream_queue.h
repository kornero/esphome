#pragma once

#include "esphome.h"

#include <esp_camera.h>
#include <ESPAsyncWebServer.h>

//#include "esphome/components/web_server_base/web_server_base.h"
//#include "esphome/core/controller.h"
//#include "esphome/core/component.h"
//#include "cam.h"

namespace esphome {
namespace esp32cam_web_stream_queue {

class Esp32CamWebStreamQueue : public AsyncWebHandler, public Component {
 public:
  QueueHandle_t ESP32CAM_WEB_FB_GET_QUEUE;
  QueueHandle_t ESP32CAM_WEB_FB_RETURN_QUEUE;

  Esp32CamWebStreamQueue(web_server_base::WebServerBase *base) : base_(base) {}

  bool canHandle(AsyncWebServerRequest *request) override {
    if (request->method() == HTTP_GET) {
      if (request->url() == "/stream_q")
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

  static void esp32cam_web_fb_task(void *pv);

  static void initCamera();
  static void resetSteps();

};

extern Esp32CamWebStreamQueue *global_esp32cam_web_stream_queue;

}  // namespace esp32cam_web_stream_queue
}  // namespace esphome