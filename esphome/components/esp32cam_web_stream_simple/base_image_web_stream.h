#pragma once

#include "esphome.h"

//#include "esphome/components/base_esp32cam/base_esp32cam.h"
#include "base_esp32cam.h"

#include <ESPAsyncWebServer.h>


#define PART_BOUNDARY "imgboundary"
static const char *STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *STREAM_CHUNK_BOUNDARY = "--" PART_BOUNDARY "\r\n";
static const char *STREAM_CHUNK_CONTENT_TYPE = "Content-Type: %s\r\n";
static const char *STREAM_CHUNK_CONTENT_LENGTH = "Content-Length: %u\r\n";
static const char *STREAM_CHUNK_NEW_LINE = "\r\n";

static const char *JPG_CONTENT_TYPE = "image/jpeg";

static const char *const TAG_BASE_IMAGE_WEB_STREAM = "base_image_web_stream";

static const int ESP32CAM_WEB_CHUNK_MAX_FPS = 25;


namespace esphome {
namespace base_image_web_stream {

class BaseImageWebStream : public AsyncWebHandler, public Component {
 public:
  BaseImageWebStream(web_server_base::WebServerBase *base,
                     base_esp32cam::BaseEsp32Cam *baseEsp32Cam)
      : baseEsp32Cam_(baseEsp32Cam),
        base_(base) {}

  bool canHandle(AsyncWebServerRequest *request) override {
    if (request->method() == HTTP_GET) {
      if (request->url() == this->pathStream_)
        return true;
      if (request->url() == this->pathStill_)
        return true;
    }

    return false;
  }

  void setup() override;

  float get_setup_priority() const override;

  void handleRequest(AsyncWebServerRequest *req) override;


  void reset_steps();

  void dump_config() override;
  //  void loop() override {
  //  }

 protected:
  base_esp32cam::BaseEsp32Cam *baseEsp32Cam_;
  web_server_base::WebServerBase *base_;

  String pathStream_;
  String pathStill_;
  char *contentType_;
  int maxFps_;
  char *TAG_;

  int maxRate_;

  camera_fb_t *webChunkFb_;

  int webChunkStep_ = 0;
  size_t webChunkSent_ = 0;
  uint32_t webChunkLastUpdate_ = 0;
};

}  // namespace esp32cam_web_stream_simple
}  // namespace esphome