#include "esphome.h"

#include "base_image_web_stream.h"
#include "JPEGSamples.h"

namespace esphome {
namespace base_image_web_stream {

class BaseImageWebStillHandler : public AsyncWebHandler {
 public:
  const char *const TAG = "web_still_image_handler";

  BaseImageWebStillHandler(BaseImageWebStream *base) : base_(base) {}

  bool canHandle(AsyncWebServerRequest *request) override {
    if (request->method() == HTTP_GET) {
      if (request->url() == this->base_->pathStill_)
        return true;
    }
    return false;
  }

  void handleRequest(AsyncWebServerRequest *req) override {
    ESP_LOGI(TAG, "Handle request.");

    req->onDisconnect([this]() -> void {
      ESP_LOGI(TAG, "Disconnecting ...");

      this->reset();

      ESP_LOGI(TAG, "... disconnected.");
    });

    if (req->url() == this->base_->pathStill_) {
      if (this->base_->isStill == pdTRUE) {
        ESP_LOGW(TAG, "Already still image!");
        req->send(409, "text/plain", "Already still image!");

        return;
      }

      this->reset();
      this->base_->isStill = pdTRUE;

      ESP_LOGI(TAG, "Start sending still image.");
      base_esp32cam::BaseEsp32Cam *cam = this->base_->get_cam();
      if (cam->current_or_next() == nullptr) {
        ESP_LOGE(TAG, "Can't get image for still.");
        req->send(500, "text/plain", "Can't get image for still");

        return;
      } else {
        AsyncWebServerResponse *response =
            req->beginResponse_P(200, JPG_CONTENT_TYPE, cam->current()->buf, cam->current()->len);

        response->addHeader("Content-Disposition", "inline; filename=capture.jpg");

        req->send(response);
        return;
      }
    }

    ESP_LOGW(TAG, "Unknown request!");
    req->send(404, "text/plain", "Unknown request!");
  }

  void reset() {
    // Clear from old stream.
    if (this->base_->isStream == pdFALSE) {
      this->base_->get_cam()->release();
    }

    this->base_->isStill = pdFALSE;
  }

 protected:
  BaseImageWebStream *base_;
};

class BaseImageWebStreamHandler : public AsyncWebHandler {
 public:
  const char *const TAG = "web_image_stream_handler";

  BaseImageWebStreamHandler(BaseImageWebStream *base) : base_(base) {}

  bool canHandle(AsyncWebServerRequest *request) override {
    if (request->method() == HTTP_GET) {
      if (request->url() == this->base_->pathStream_)
        return true;
    }
    return false;
  }

  void handleRequest(AsyncWebServerRequest *req) override {
    ESP_LOGI(TAG, "Handle request.");

    req->onDisconnect([this]() -> void {
      ESP_LOGI(TAG, "Disconnecting ...");

      this->reset();

      digitalWrite(33, HIGH);  // Turn off

      ESP_LOGI(TAG, "... disconnected.");
    });

    if (req->url() == this->base_->pathStream_) {
      if (this->base_->isStream == pdTRUE) {
        ESP_LOGW(this->TAG, "Already streaming!");
        req->send(409, "text/plain", "Already streaming!");

        return;
      }

      ESP_LOGD(TAG, "Turn on LED.");
      digitalWrite(33, LOW);  // Turn on

      this->reset();

      ESP_LOGD(TAG, "Set stream.");

      this->base_->isStream = pdTRUE;

      ESP_LOGD(TAG, "Starting stream.");
      AsyncWebServerResponse *response = this->response(req);

      response->addHeader("Access-Control-Allow-Origin", "*");

      req->send(response);

      return;
    }

    ESP_LOGW(this->TAG, "Unknown request!");
    req->send(404, "text/plain", "Unknown request!");
  }

#pragma clang diagnostic push
#pragma ide diagnostic ignored "readability-function-cognitive-complexity"
  AsyncWebServerResponse *response(AsyncWebServerRequest *req) {
    return req->beginChunkedResponse(
        STREAM_CONTENT_TYPE, [this](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
          try {
            // Wait for still image.
            if (this->base_->isStill == pdTRUE) {
              if (this->base_->isStreamPaused == pdTRUE) {
                ESP_LOGD(TAG, "Stream is paused, waiting.");

                return RESPONSE_TRY_AGAIN;
              }
            } else {
              if (this->base_->isStreamPaused == pdTRUE) {
                this->base_->isStreamPaused = pdFALSE;
                ESP_LOGI(TAG, "Stream unpause.");
              }
            }

            if (this->base_->isStream == pdFALSE) {
              ESP_LOGE(TAG, "Not in stream mode.");
              return 0;
            }

            base_esp32cam::BaseEsp32Cam *cam = this->base_->get_cam();

            if (this->webChunkSent_ == -1) {
              if (cam->next() == nullptr) {
                // no frame ready
                //              ESP_LOGD(TAG_, "No frame ready");
                return RESPONSE_TRY_AGAIN;
              }

              this->webChunkSent_ = 0;
            }

            switch (this->webChunkStep_) {
              case 0: {
                size_t i = strlen(STREAM_CHUNK_BOUNDARY);

                if (maxLen < i) {
                  ESP_LOGW(TAG, "Not enough space for boundary");
                  return RESPONSE_TRY_AGAIN;
                }

                memcpy(buffer, STREAM_CHUNK_BOUNDARY, i);

                this->webChunkStep_++;

                return i;
              }

              case 1: {
                if (maxLen < (strlen(STREAM_CHUNK_CONTENT_TYPE) + strlen(JPG_CONTENT_TYPE))) {
                  ESP_LOGW(TAG, "Not enough space for content type");
                  return RESPONSE_TRY_AGAIN;
                }

                size_t i = sprintf((char *) buffer, STREAM_CHUNK_CONTENT_TYPE, JPG_CONTENT_TYPE);

                this->webChunkStep_++;
                this->webChunkStep_++;  // Skip content length.

                return i;
              }

              case 2: {
                if (maxLen < (strlen(STREAM_CHUNK_CONTENT_LENGTH) + 8)) {
                  ESP_LOGW(TAG, "Not enough space for content length");
                  return RESPONSE_TRY_AGAIN;
                }

                size_t i = sprintf((char *) buffer, STREAM_CHUNK_CONTENT_LENGTH, cam->current()->len);

                this->webChunkStep_++;

                return i;
              }

              case 3: {
                size_t i = strlen(STREAM_CHUNK_NEW_LINE);
                if (maxLen < i) {
                  ESP_LOGW(TAG, "Not enough space for new line");
                  return RESPONSE_TRY_AGAIN;
                }

                memcpy(buffer, STREAM_CHUNK_NEW_LINE, i);

                this->webChunkStep_++;

                return i;
              }

              case 4: {
                camera_fb_t *current = cam->current();

                //                uint8_t * buf = current->buf;
                //                size_t len = current->len;
                //                const unsigned char *buf = capture_jpg;
                //                size_t len = capture_jpg_len;
                unsigned const char *buf = octo_jpg;
                size_t len = octo_jpg_len;

                size_t i = len - this->webChunkSent_;
                size_t m = maxLen;

                if (i <= 0) {
                  ESP_LOGD(TAG, "Image size = %d , sent = %d", len, this->webChunkSent_);

                  ESP_LOGE(TAG, "Content can't be zero length: %d", i);

                  return 0;
                }

                if (i > m) {
                  memcpy(buffer, buf + this->webChunkSent_, m);
                  this->webChunkSent_ += m;
                  return m;
                }

                memcpy(buffer, buf + this->webChunkSent_, i);

                cam->release();
                this->webChunkSent_ = -1;

                this->webChunkStep_++;

                return i;
              }

              case 5: {
                size_t i = strlen(STREAM_CHUNK_NEW_LINE);
                if (maxLen < i) {
                  ESP_LOGW(TAG, "Not enough space for new line");
                  return RESPONSE_TRY_AGAIN;
                }

                memcpy(buffer, STREAM_CHUNK_NEW_LINE, i);

                this->webChunkStep_ = 0;

                if (this->base_->isStill == pdTRUE) {
                  this->base_->isStreamPaused = pdTRUE;
                  ESP_LOGI(TAG, "Stream is set on pause.");
                }

                return i;
              }

              default:
                ESP_LOGE(TAG, "Wrong step %d", this->webChunkStep_);

                return 0;
            }
          } catch (...) {
            ESP_LOGE(TAG, "EXCEPTION !");
            return 0;
          }
        });
  }
#pragma clang diagnostic pop

  void reset() {
    // Clear from old stream.
    this->base_->get_cam()->release();

    this->base_->isStream = pdFALSE;
    this->base_->isStreamPaused = pdFALSE;

    this->webChunkSent_ = -1;
    this->webChunkStep_ = 0;
  }

 protected:
  BaseImageWebStream *base_;

 private:
  int webChunkStep_;
  size_t webChunkSent_;
};

void BaseImageWebStream::setup() {
  ESP_LOGI(TAG_, "enter setup");

  this->base_web_server_->init();

  this->pathStream_ = "/stream";
  this->pathStill_ = "/still";
  this->contentType_ = JPG_CONTENT_TYPE;
  this->TAG_ = TAG_BASE_IMAGE_WEB_STREAM;

  this->isStream = pdFALSE;
  this->isStreamPaused = pdFALSE;
  this->isStill = pdFALSE;

  this->base_web_server_->add_handler(new BaseImageWebStreamHandler(this));
  this->base_web_server_->add_handler(new BaseImageWebStillHandler(this));
}

void BaseImageWebStream::dump_config() {
  if (psramFound()) {
    ESP_LOGCONFIG(TAG_, "PSRAM");
  } else {
    ESP_LOGCONFIG(TAG_, "PSRAM not found.");
  }
}

base_esp32cam::BaseEsp32Cam *BaseImageWebStream::get_cam() { return this->base_esp32cam_; }

}  // namespace base_image_web_stream
}  // namespace esphome