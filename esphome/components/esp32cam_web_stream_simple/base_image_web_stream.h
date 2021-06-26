#pragma once

#include "esphome.h"

//#include "esphome/components/base_esp32cam/base_esp32cam.h"
#include "base_esp32cam.h"

#include <ESPAsyncWebServer.h>

namespace esphome {

#define PART_BOUNDARY "imgboundary"
static const char *STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *STREAM_CHUNK_BOUNDARY = "--" PART_BOUNDARY "\r\n";
static const char *STREAM_CHUNK_CONTENT_TYPE = "Content-Type: %s\r\n";
static const char *STREAM_CHUNK_CONTENT_LENGTH = "Content-Length: %u\r\n";
static const char *STREAM_CHUNK_NEW_LINE = "\r\n";

static const char *JPG_CONTENT_TYPE = "image/jpeg";

static const char *const TAG_BASE_IMAGE_WEB_STREAM = "base_image_web_stream";

static const int ESP32CAM_WEB_CHUNK_MAX_FPS = 25;

namespace base_image_web_stream {

//const BaseImageWebStream *THIS;

class BaseImageWebStream : public AsyncWebHandler, public Component {
 public:
  String pathStream_;
   String pathStill_;
  const char *contentType_;
   int maxFps_;
  const char *const TAG_;

  BaseImageWebStream(web_server_base::WebServerBase *base,
                     base_esp32cam::BaseEsp32Cam *baseEsp32Cam,

                     String pathStream = "stream",
                       String pathStill = "still",
                     const char *contentType = JPG_CONTENT_TYPE,
                     int maxFps = ESP32CAM_WEB_CHUNK_MAX_FPS,
                      const char *const tag = TAG_BASE_IMAGE_WEB_STREAM)
      : pathStream_(pathStream),
        pathStill_(pathStill),
        contentType_(contentType),
        maxFps_(maxFps),
        TAG_(tag),
        baseEsp32Cam_(baseEsp32Cam),
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

  void dump_config() override;
  //  void loop() override {
  //  }

 protected:
  base_esp32cam::BaseEsp32Cam *baseEsp32Cam_;
  web_server_base::WebServerBase *base_;

  const int maxRate_ = 1000 / this->maxFps_;  // 15 fps

  camera_fb_t *webChunkFb_;

  int webChunkStep_ = 0;
  size_t webChunkSent_ = 0;
  uint32_t webChunkLastUpdate_ = 0;

  void resetSteps() {

    // Clear from old stream.
    if (this->webChunkFb_) {
      this->baseEsp32Cam_->esp_camera_fb_return(this->webChunkFb_);
    }

    this->webChunkSent_ = -1;
    this->webChunkStep_ = 0;
    this->webChunkLastUpdate_ = millis();
  }
};

}  // namespace esp32cam_web_stream_simple
}  // namespace esphome