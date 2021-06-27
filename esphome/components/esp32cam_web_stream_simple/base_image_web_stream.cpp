#include "esphome.h"

#include "base_image_web_stream.h"

namespace esphome {
namespace base_image_web_stream {
/**/
class BaseImageWebStillHandler : public AsyncWebHandler {
 public:
  const char *const TAG = "STILL";

  BaseImageWebStillHandler(BaseImageWebStream *base) : base_(base) {}

  bool canHandle(AsyncWebServerRequest *request) override {
    ESP_LOGI(TAG, "Can handle?");

    if (request->method() == HTTP_GET) {
      if (request->url() == this->base_->pathStill_)
        return true;
    }

    ESP_LOGI(TAG, "Can handle?.... No!");

    return false;
  }

  void handleRequest(AsyncWebServerRequest *req) override {
    ESP_LOGI(TAG, "Handle request.");

    req->onDisconnect([this]() -> void {
      ESP_LOGI(TAG, "Disconnected still image.");

      this->base_->reset_still();
    });

    if (req->url() == this->base_->pathStill_) {
      if (this->base_->isStill == pdTRUE) {
        ESP_LOGW(TAG, "Already still image!");
        req->send(409, "text/plain", "Already still image!");

        return;
      }

      ESP_LOGD(TAG, "Call reset");
      this->base_->reset_still();

      ESP_LOGD(TAG, "Turn on flag");
      this->base_->isStill = pdTRUE;

      if (this->base_->isStream == pdTRUE) {
        ESP_LOGD(TAG, "Try to pause stream");

        uint32_t now = millis();
        while (this->base_->isStreamPaused.load(std::memory_order_acquire) == pdFALSE && millis() - now < 100) {
          yield();
        }

        ESP_LOGD(TAG, "Check pause result.");

        if (this->base_->isStreamPaused.load(std::memory_order_acquire) == pdFALSE) {
          ESP_LOGI(TAG, "Can't pause streaming, continue anyway.");
        }
      }

      base_esp32cam::BaseEsp32Cam *cam = this->base_->get_cam();

      if (cam->current_or_next() == nullptr) {
        ESP_LOGE(TAG, "Can't get image for still.");
        req->send(500, "text/plain", "Can't get image for still.");

        this->base_->isStill = pdFALSE;

        return;
      }

      ESP_LOGI(TAG, "Start sending still image.");

      AsyncWebServerResponse *response = req->beginResponse(
          JPG_CONTENT_TYPE, cam->current()->len, [this, cam](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
            try {
              if (this->base_->isStill == pdFALSE) {
                ESP_LOGE(TAG, "Not in still mode.");
                return 0;
              }

              size_t i = cam->current()->len - index;
              size_t m = maxLen;

              if (i <= 0) {
                ESP_LOGE(TAG, "[STILL] Content can't be zero length: %d", i);
                                   return 0;
                                 }

                                 if (i > m) {
                                   memcpy(buffer, cam->current()->buf + index, m);

                                   return m;
                                 }

                                 memcpy(buffer, cam->current()->buf + index, i);

                                 this->base_->reset_still();

                                 return i;
                               } catch (...) {
                                 ESP_LOGE(TAG, "[STILL] EXCEPTION !");
                                 return 0;
                               }
                             });

      response->addHeader("Content-Disposition", "inline; filename=capture.jpg");

      req->send(response);
      return;
    }

    ESP_LOGW(TAG, "Unknown request!");
    req->send(404, "text/plain", "Unknown request!");
  }

 protected:
  BaseImageWebStream *base_;
};
/**/
void BaseImageWebStream::handleRequest(AsyncWebServerRequest *req) {
  ESP_LOGI(TAG_, "Handle request.");

  req->onDisconnect([this]() -> void {
    ESP_LOGI(TAG_, "Disconnecting ...");

    this->reset_stream();

    digitalWrite(33, HIGH);  // Turn off

    ESP_LOGI(TAG_, "... disconnected.");
  });

  if (req->url() == this->pathStream_) {
    if (this->isStream == pdTRUE) {
      ESP_LOGW(this->TAG_, "Already streaming!");
      req->send(409, "text/plain", "Already streaming!");

      return;
    }

    ESP_LOGD(TAG_, "Turn on LED.");
    digitalWrite(33, LOW);  // Turn on

    this->reset_stream();

    ESP_LOGD(TAG_, "Set stream.");

    this->isStream = pdTRUE;

    AsyncWebServerResponse *response = this->stream(req);
    response->addHeader("Access-Control-Allow-Origin", "*");

    req->send(response);

    return;
  }

  ESP_LOGW(this->TAG_, "Unknown request!");
  req->send(404, "text/plain", "Unknown request!");
}

void BaseImageWebStream::setup() {
  ESP_LOGI(TAG_, "enter setup");

  this->base_web_server_->init();

  this->pathStream_ = "/stream";
  this->pathStill_ = "/still";
  this->contentType_ = JPG_CONTENT_TYPE;
  this->TAG_ = TAG_BASE_IMAGE_WEB_STREAM;

  this->webChunkStep_ = -1;
  this->webChunkSent_ = 0;

  this->isStream = pdFALSE;
  this->isStreamPaused.store(pdFALSE, std::memory_order_release);
  this->isStill = pdFALSE;

  this->base_web_server_->add_handler(this);
  //  this->base_web_server_->add_handler(new BaseImageWebStillHandler(this));
}

void BaseImageWebStream::reset_stream() {
  // Clear from old stream.
  this->get_cam()->release();

  this->webChunkSent_ = -1;
  this->webChunkStep_ = 0;

  this->isStream = pdFALSE;
  this->isStreamPaused.store(pdFALSE, std::memory_order_release);
}

void BaseImageWebStream::reset_still() {
  // Clear from old stream.
  if (this->isStream == pdFALSE) {
    this->get_cam()->release();
  }

  this->isStill = pdFALSE;
}

void BaseImageWebStream::dump_config() {
  if (psramFound()) {
    ESP_LOGCONFIG(TAG_, "PSRAM");
  } else {
    ESP_LOGCONFIG(TAG_, "PSRAM not found.");
  }
}

base_esp32cam::BaseEsp32Cam *BaseImageWebStream::get_cam() { return this->base_esp32cam_; }

AsyncWebServerResponse *BaseImageWebStream::stream(AsyncWebServerRequest *req) {
  ESP_LOGD(TAG_, "Starting stream.");

  AsyncWebServerResponse *response =
      req->beginChunkedResponse(STREAM_CONTENT_TYPE, [this](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
        try {
          // Wait for still image.
          if (this->isStill == pdTRUE) {
            this->isStreamPaused.store(pdTRUE, std::memory_order_release);

            ESP_LOGD(TAG_, "Paused value: %d", this->isStreamPaused.load(std::memory_order_acquire));

            return RESPONSE_TRY_AGAIN;
          } else {
            this->isStreamPaused.store(pdFALSE, std::memory_order_release);
          }

          if (this->isStream == pdFALSE) {
            ESP_LOGE(TAG_, "Not in stream mode.");
            return 0;
          }

          base_esp32cam::BaseEsp32Cam *cam = this->get_cam();

          if (this->webChunkSent_ == -1) {
            if (cam->next() == nullptr) {
              // no frame ready
              ESP_LOGD(TAG_, "No frame ready");
              return RESPONSE_TRY_AGAIN;
            }

            this->webChunkSent_ = 0;
          }

          switch (this->webChunkStep_) {
            case 0: {
              size_t i = strlen(STREAM_CHUNK_BOUNDARY);

              if (maxLen < i) {
                ESP_LOGW(TAG_, "Not enough space for boundary");
                return RESPONSE_TRY_AGAIN;
              }

              memcpy(buffer, STREAM_CHUNK_BOUNDARY, i);

              this->webChunkStep_++;

              return i;
            }

            case 1: {
              if (maxLen < (strlen(STREAM_CHUNK_CONTENT_TYPE) + strlen(JPG_CONTENT_TYPE))) {
                ESP_LOGW(TAG_, "Not enough space for content type");
                return RESPONSE_TRY_AGAIN;
              }

              size_t i = sprintf((char *) buffer, STREAM_CHUNK_CONTENT_TYPE, JPG_CONTENT_TYPE);

              this->webChunkStep_++;
              this->webChunkStep_++;  // Skip content length.

              return i;
            }

            case 2: {
              if (maxLen < (strlen(STREAM_CHUNK_CONTENT_LENGTH) + 8)) {
                ESP_LOGW(TAG_, "Not enough space for content length");
                return RESPONSE_TRY_AGAIN;
              }

              size_t i = sprintf((char *) buffer, STREAM_CHUNK_CONTENT_LENGTH, cam->current()->len);

              this->webChunkStep_++;

              return i;
            }

            case 3: {
              size_t i = strlen(STREAM_CHUNK_NEW_LINE);
              if (maxLen < i) {
                ESP_LOGW(TAG_, "Not enough space for new line");
                return RESPONSE_TRY_AGAIN;
              }

              memcpy(buffer, STREAM_CHUNK_NEW_LINE, i);

              this->webChunkStep_++;

              return i;
            }

            case 4: {
              size_t i = cam->current()->len - this->webChunkSent_;
              size_t m = maxLen;

              if (i <= 0) {
                ESP_LOGD(TAG_, "Image size = %d , sent = %d", cam->current()->len, this->webChunkSent_);

                ESP_LOGE(TAG_, "Content can't be zero length: %d", i);

                return 0;
              }

              if (i > m) {
                memcpy(buffer, cam->current()->buf + this->webChunkSent_, m);
                this->webChunkSent_ += m;
                return m;
              }

              memcpy(buffer, cam->current()->buf + this->webChunkSent_, i);

              cam->release();
              this->webChunkSent_ = -1;

              this->webChunkStep_++;

              return i;
            }

            case 5: {
              size_t i = strlen(STREAM_CHUNK_NEW_LINE);
              if (maxLen < i) {
                ESP_LOGW(TAG_, "Not enough space for new line");
                return RESPONSE_TRY_AGAIN;
              }

              memcpy(buffer, STREAM_CHUNK_NEW_LINE, i);

              this->webChunkStep_ = 0;

              return i;
            }

            default:
              ESP_LOGE(TAG_, "Wrong step %d", this->webChunkStep_);

              return 0;
          }
        } catch (...) {
          ESP_LOGE(TAG_, "EXCEPTION !");
          return 0;
        }
      });

  return response;
}

}  // namespace base_image_web_stream
}  // namespace esphome