#pragma once

//#include "esphome.h"

//#include "esphome/components/base_esp32cam/base_esp32cam.h"

#include <esp_camera.h>
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

class BaseImageWebStream : public AsyncWebHandler {
 public:
  BaseImageWebStream(web_server_base::WebServerBase *base, base_esp32cam::BaseEsp32Cam *base_esp32cam)
      : base_web_server_(base), base_esp32cam_(base_esp32cam) {}

  bool canHandle(AsyncWebServerRequest *request) override {
    ESP_LOGI(TAG_, "Can handle?");

    if (request->method() == HTTP_GET) {
      if (request->url() == this->pathStream_)
        return true;
      if (request->url() == this->pathStill_)
        return true;
    }

    ESP_LOGI(TAG_, "Can handle?.... No!");

    return false;
  }

  void setup();

  void handleRequest(AsyncWebServerRequest *req) override;

  void reset_stream();
  void reset_still();

  void dump_config();

 protected:
  web_server_base::WebServerBase *base_web_server_;
  base_esp32cam::BaseEsp32Cam *base_esp32cam_;

  String pathStream_;
  String pathStill_;
  const char *contentType_;
  int maxFps_;
  const char *TAG_;

  int maxRate_;

  camera_fb_t *webChunkFb_;
  int webChunkStep_;
  size_t webChunkSent_;
  uint32_t webChunkLastUpdate_;

  BaseType_t isStream;
  BaseType_t isStreamPaused;
  BaseType_t isStill;

  AsyncWebServerResponse *stream(AsyncWebServerRequest *request);
  AsyncWebServerResponse *still(AsyncWebServerRequest *request);
};

}  // namespace base_image_web_stream
}  // namespace esphome