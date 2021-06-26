#include "base_image_web_stream.h"

namespace esphome {
namespace base_image_web_stream {

void BaseImageWebStream::handleRequest(AsyncWebServerRequest *req) {
  if (millis() - this->webChunkLastUpdate_ < 5000) {
    ESP_LOGE(this->TAG_, "Already streaming!");
    req->send(500, "text/plain", "Already streaming!");
  }

  this->resetSteps();

  digitalWrite(33, LOW);  // Turn on

  AsyncWebServerResponse *response =
      req->beginChunkedResponse(STREAM_CONTENT_TYPE, [this](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
        try {
          if (this->webChunkSent_ == -1) {
            while (millis() - this->webChunkLastUpdate_ < this->maxRate_) {
              delay(10);
            }

            this->webChunkFb_ = this->baseEsp32Cam_->esp_camera_fb_get();
            if (!this->webChunkFb_) {
              ESP_LOGE(TAG_, "Camera capture failed");

//              esp_camera_deinit();
//              initCamera();

              return 0;
            }

            this->webChunkLastUpdate_ = millis();
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

              size_t i = sprintf((char *) buffer, STREAM_CHUNK_CONTENT_LENGTH, this->webChunkFb_->len);

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
              size_t i = this->webChunkFb_->len - this->webChunkSent_;
              size_t m = maxLen;

              if (m < i) {
                memcpy(buffer, this->webChunkFb_->buf + this->webChunkSent_, m);
                this->webChunkSent_ += m;
                return m;
              }

              memcpy(buffer, this->webChunkFb_->buf + this->webChunkSent_, i);

              esp_camera_fb_return(this->webChunkFb_);
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

  response->addHeader("Access-Control-Allow-Origin", "*");
  req->onDisconnect([this]() -> void {
    ESP_LOGI(TAG_, "Disconnected.");

    //    resetSteps();

    digitalWrite(33, HIGH);  // Turn off
  });

  req->send(response);
}

void BaseImageWebStream::setup() {
//  global_esp32cam_web_stream_simple = this;

  this->base_->init();

//  initCamera();

  this->base_->add_handler(this);
}

float BaseImageWebStream::get_setup_priority() const { return setup_priority::AFTER_WIFI; }

void BaseImageWebStream::dump_config() {
  if (psramFound()) {
    ESP_LOGCONFIG(TAG_, "PSRAM");
  } else {
    ESP_LOGCONFIG(TAG_, "PSRAM not found.");
  }

  ESP_LOGCONFIG(TAG_, "Max FPS %d.", ESP32CAM_WEB_CHUNK_MAX_FPS);
}
}
}  // namespace esphome