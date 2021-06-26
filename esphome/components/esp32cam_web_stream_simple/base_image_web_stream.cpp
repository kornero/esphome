#include "esphome.h"

#include "base_image_web_stream.h"

namespace esphome {
namespace base_image_web_stream {

void BaseImageWebStream::handleRequest(AsyncWebServerRequest *req) {
  ESP_LOGI(TAG_, "Handle request.");

  if (req->url() == this->pathStill_) {
    // Clear old.
    if (this->webStillFb_) {
      this->base_esp32cam_->return_fb_nowait(this->webStillFb_);
      this->webStillSent_ = 0;
    }

    this->webStillFb_ = this->base_esp32cam_->get_fb();
    if (this->webStillFb_) {
      ESP_LOGW(TAG_, "Start sending still image.");

      AsyncWebServerResponse *response = this->still(req);
      /*
            response->setCode(200);
            response->setContentType(JPG_CONTENT_TYPE);
            response->setContentLength(this->webStillFb_->len);
      */

      /*response->addHeader("Content-Type", JPG_CONTENT_TYPE);

      char str[256];
      snprintf(str, sizeof str, "%zu", this->webStillFb_->len);
      response->addHeader("Content-Length", str);*/

      response->addHeader("Content-Disposition", "inline; filename=capture.jpg");

      req->send(response);
    } else {
      ESP_LOGW(TAG_, "Can't get image for still.");
    }
    return;
  }

  if (millis() - this->webChunkLastUpdate_ < 5000) {
    ESP_LOGE(this->TAG_, "Already streaming!");
    req->send(500, "text/plain", "Already streaming!");
  }

  this->reset_steps();

  digitalWrite(33, LOW);  // Turn on

  AsyncWebServerResponse *response = this->stream(req);
  response->addHeader("Access-Control-Allow-Origin", "*");
  req->onDisconnect([this]() -> void {
    ESP_LOGI(TAG_, "Disconnected.");

    //    resetSteps();

    digitalWrite(33, HIGH);  // Turn off
  });

  req->send(response);
}

void BaseImageWebStream::setup() {
  ESP_LOGI(TAG_, "enter setup");

  this->base_web_server_->init();

  this->pathStream_ = "/stream";
  this->pathStill_ = "/still";
  this->contentType_ = JPG_CONTENT_TYPE;
  this->maxFps_ = ESP32CAM_WEB_CHUNK_MAX_FPS;
  this->TAG_ = TAG_BASE_IMAGE_WEB_STREAM;

  this->webChunkStep_ = -1;
  this->webChunkSent_ = 0;
  this->webChunkLastUpdate_ = 0;

  this->webStillSent_ = 0;

  this->maxRate_ = 1000 / this->maxFps_;  // 15 fps

  this->base_web_server_->add_handler(this);
}

void BaseImageWebStream::reset_steps() {
  // Clear from old stream.
  if (this->webChunkFb_) {
    this->base_esp32cam_->return_fb_nowait(this->webChunkFb_);
  }

  this->webChunkSent_ = -1;
  this->webChunkStep_ = 0;
  this->webChunkLastUpdate_ = millis();
}

void BaseImageWebStream::dump_config() {
  if (psramFound()) {
    ESP_LOGCONFIG(TAG_, "PSRAM");
  } else {
    ESP_LOGCONFIG(TAG_, "PSRAM not found.");
  }

  ESP_LOGCONFIG(TAG_, "Max FPS %d.", ESP32CAM_WEB_CHUNK_MAX_FPS);
}

AsyncWebServerResponse *BaseImageWebStream::stream(AsyncWebServerRequest *req) {
  AsyncWebServerResponse *response =
      req->beginChunkedResponse(STREAM_CONTENT_TYPE, [this](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
        try {
          if (this->webChunkSent_ == -1) {
            while (millis() - this->webChunkLastUpdate_ < this->maxRate_) {
              delay(10);
            }

            this->webChunkFb_ = this->base_esp32cam_->get_fb_nowait();
            if (!this->webChunkFb_) {
              // no frame ready
              ESP_LOGD(TAG_, "No frame ready");
              return RESPONSE_TRY_AGAIN;
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

              this->base_esp32cam_->return_fb(this->webChunkFb_);
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

AsyncWebServerResponse *BaseImageWebStream::still(AsyncWebServerRequest *req) {
  AsyncWebServerResponse *response = req->beginResponse(
      JPG_CONTENT_TYPE, this->webStillFb_->len, [this](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
        try {
          size_t i = this->webStillFb_->len - this->webStillSent_;
          size_t m = maxLen;

          if (m < i) {
            memcpy(buffer, this->webStillFb_->buf + this->webStillSent_, m);
            this->webStillSent_ += m;
            return m;
          }

          memcpy(buffer, this->webStillFb_->buf + this->webStillSent_, i);

          this->base_esp32cam_->return_fb(this->webStillFb_);
          this->webStillSent_ = 0;

          return i;
        } catch (...) {
          ESP_LOGE(TAG_, "EXCEPTION !");
          return 0;
        }
      });

  return response;
}
}  // namespace base_image_web_stream
}  // namespace esphome