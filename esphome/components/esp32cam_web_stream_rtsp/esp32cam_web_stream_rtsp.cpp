#include "esp32cam_web_stream_rtsp.h"

// using namespace esphome;
namespace esphome {
namespace esp32cam_web_stream_rtsp {

static const char *const TAG = "esp32cam_web_stream_rtsp";

void Esp32CamWebStreamRtsp::setup() {
  ESP_LOGI(TAG, "enter setup");

  base_esp32cam::BaseEsp32Cam *cam = new base_esp32cam::BaseEsp32Cam();
  cam->setup();

  ESP_LOGI(TAG, "Cam.... ok.");

  //  camera_config_t cameraConfig =  cam->get_camera_config();
  //  this->dim = this->parse_camera_dimensions_(cameraConfig);

  struct dimensions dim = {0, 0};
  dim.width = 640;
  dim.height = 480;

  ESP_LOGD(TAG, "Beginning to set up RTSP server listener");

  //  url = "rtsp://" + WiFi.localIP().toString() + ":554/mjpeg/1";
  this->server = new AsyncRTSPServer(554, dim);

  this->server->onClient([this](void *s) { ESP_LOGD(TAG, "Received RTSP connection"); }, this);
  this->server->setLogFunction([](String s) { ESP_LOGD(TAG, s.c_str()); }, this);

  ESP_LOGD(TAG, "Set up RTSP server listener, starting");
  try {
    this->server->begin();
    //    ESP_LOGI(TAG, "Started RTSP server listener");
    ESP_LOGCONFIG(TAG, "Started RTSP server listener");
  } catch (...) {
    ESP_LOGCONFIG(TAG, "Failed to start RTSP server listener");
    //    ESP_LOGE(TAG, "Failed to start RTSP server listener");
    this->mark_failed();
  }

  //  base_image_web_stream::BaseImageWebStream *web = new base_image_web_stream::BaseImageWebStream(this->base_, cam);
  //  web->set_cam(cam);
  //  web->setup();

  ESP_LOGI(TAG, "Web.... ok.");

  //  this->baseImageWebStream_ = web;
  this->baseEsp32Cam_ = cam;
}

void Esp32CamWebStreamRtsp::loop() {
  if (this->server->hasClients()) {
    camera_fb_t *fb_ = esp_camera_fb_get();
    if (fb_ != nullptr) {
      this->server->pushFrame(fb_->buf, fb_->len);
      esp_camera_fb_return(fb_);
      fb_ = nullptr;
    }
    /*
    base_esp32cam::BaseEsp32Cam *cam = this->baseEsp32Cam_;
    cam->release();
    if (cam->next() == nullptr) {
      //      ESP_LOGE(TAG, "Can't get image.");
      //      this->mark_failed();
      //      return;
    } else {
      this->server->pushFrame(cam->current()->buf, cam->current()->len);
      cam->release();
    }*/
  }
}

float Esp32CamWebStreamRtsp::get_setup_priority() const { return setup_priority::LATE; }

void Esp32CamWebStreamRtsp::dump_config() {
  ESP_LOGCONFIG(TAG, "RTSP Server:");
  ESP_LOGCONFIG(TAG, "  Address: %s:%u", network_get_address().c_str(), 554);
  ESP_LOGCONFIG(TAG, "  Camera Object: %p", this->baseEsp32Cam_);
  // TODO:!!!
  //  this->baseImageWebStream_->dump_config();
}

dimensions Esp32CamWebStreamRtsp::parse_camera_dimensions_(camera_config_t config) {
  struct dimensions dim = {0, 0};
  switch (config.frame_size) {
    case FRAMESIZE_QQVGA:
      dim.width = 160;
      dim.height = 120;
      break;
    case FRAMESIZE_QCIF:
      dim.width = 176;
      dim.height = 144;
      break;
    case FRAMESIZE_HQVGA:
      dim.width = 240;
      dim.height = 176;
      break;
    case FRAMESIZE_QVGA:
      dim.width = 320;
      dim.height = 240;
      break;
    case FRAMESIZE_CIF:
      dim.width = 400;
      dim.height = 296;
      break;
    case FRAMESIZE_VGA:
      dim.width = 640;
      dim.height = 480;
      break;
    case FRAMESIZE_SVGA:
      dim.width = 800;
      dim.height = 600;
      break;
    case FRAMESIZE_XGA:
      dim.width = 1024;
      dim.height = 768;
      break;
    case FRAMESIZE_SXGA:
      dim.width = 1280;
      dim.height = 1024;
      break;
    case FRAMESIZE_UXGA:
      dim.width = 1600;
      dim.height = 1200;
      break;
  }
  return dim;
}

}  // namespace esp32cam_web_stream_rtsp
}  // namespace esphome