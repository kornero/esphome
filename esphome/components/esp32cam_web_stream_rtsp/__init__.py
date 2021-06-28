import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
from esphome.components import web_server_base

AUTO_LOAD = ["web_server_base"]

esp32cam_web_stream_rtsp_ns = cg.esphome_ns.namespace("esp32cam_web_stream_rtsp")
Esp32CamWebStreamRtsp = esp32cam_web_stream_rtsp_ns.class_("Esp32CamWebStreamRtsp", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Esp32CamWebStreamRtsp),
        cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(
            web_server_base.WebServerBase
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])

    # cg.add_define("USE_ESP32CAM_WEB_STREAM_RTSP")

    var = cg.new_Pvariable(config[CONF_ID], paren)
    await cg.register_component(var, config)

    cg.add_build_flag("-DBOARD_HAS_PSRAM")
