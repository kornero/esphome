#pragma once

#include <ESPAsyncWebServer.h>
#include <esp_camera.h>

#include "base_esp32cam.h"

namespace esphome {
namespace base_image_web_stream {

#define PART_BOUNDARY "imgboundary"
static const char *STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *STREAM_CHUNK_BOUNDARY = "--" PART_BOUNDARY "\r\n";
static const char *STREAM_CHUNK_CONTENT_TYPE = "Content-Type: %s\r\n";
static const char *STREAM_CHUNK_CONTENT_LENGTH = "Content-Length: %u\r\n";
static const char *STREAM_CHUNK_NEW_LINE = "\r\n";

static const char *JPG_CONTENT_TYPE = "image/jpeg";

static const char *const TAG_BASE_IMAGE_WEB_STREAM = "base_image_web_stream";

class BaseImageWebStream {
 public:
  String pathStream_;
  String pathStill_;
  const char *contentType_;

  BaseType_t isStream;
  BaseType_t isStreamPaused;
  BaseType_t isStill;

  BaseImageWebStream(web_server_base::WebServerBase *base, base_esp32cam::BaseEsp32Cam *base_esp32cam)
      : base_web_server_(base), base_esp32cam_(base_esp32cam) {}

  void setup();

  void dump_config();

  base_esp32cam::BaseEsp32Cam *get_cam();

 protected:
  web_server_base::WebServerBase *base_web_server_;
  base_esp32cam::BaseEsp32Cam *base_esp32cam_;

  const char *TAG_;
};

}  // namespace base_image_web_stream
}  // namespace esphome