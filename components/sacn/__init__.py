import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.light.types import AddressableLightEffect, LightEffect
from esphome.components.light.effects import (
    register_addressable_effect,
    register_rgb_effect,
    register_monochromatic_effect,
)
from esphome.const import CONF_ID, CONF_NAME

DEPENDENCIES = ["network"]

sacn_ns = cg.esphome_ns.namespace("sacn")
SACNLightEffect = sacn_ns.class_("SACNLightEffect", LightEffect)
SACNAddressableLightEffect = sacn_ns.class_(
    "SACNAddressableLightEffect", AddressableLightEffect
)
SACNComponent = sacn_ns.class_("SACNComponent", cg.Component)

SACN_CHANNEL_TYPE = {
    "MONO": sacn_ns.SACN_MONO,
    "RGB": sacn_ns.SACN_RGB,
    "RGBW": sacn_ns.SACN_RGBW,
    "RGBWW": sacn_ns.SACN_RGBWW
}

SACN_TRANSPORT_MODE = {
    "UNICAST": sacn_ns.SACN_UNICAST,
    "MULTICAST": sacn_ns.SACN_MULTICAST
}

CONF_SACN_ID = "sacn_id"
CONF_SACN_UNIVERSE = "universe"
CONF_SACN_START_CHANNEL = "start_channel"
CONF_SACN_CHANNEL_TYPE = "channel_type"
CONF_SACN_TRANSPORT_MODE = "transport_mode"
CONF_SACN_TIMEOUT = "timeout"
CONF_SACN_BLANK_ON_START = "blank_on_start"

CHANNEL_MONO = "MONO"
CHANNEL_RGB = "RGB"
CHANNEL_RGBW = "RGBW"
CHANNEL_RGBWW = "RGBWW"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SACNComponent),
        }
    ),
    cv.only_with_arduino,
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

@register_rgb_effect(
    "sacn",
    SACNLightEffect,
    "sACN",
    {
        cv.GenerateID(CONF_SACN_ID): cv.use_id(SACNComponent),
        cv.Optional(CONF_SACN_UNIVERSE, default=1): cv.int_range(min=1, max=63999),
        cv.Optional(CONF_SACN_START_CHANNEL, default=1): cv.int_range(min=1, max=512),
        cv.Optional(CONF_SACN_CHANNEL_TYPE, default=CHANNEL_RGB): cv.one_of(CHANNEL_MONO, CHANNEL_RGB, CHANNEL_RGBW, CHANNEL_RGBWW, upper=True),
        cv.Optional(CONF_SACN_TRANSPORT_MODE, default="UNICAST"): cv.one_of(*SACN_TRANSPORT_MODE, upper=True),
        cv.Optional(CONF_SACN_TIMEOUT, default="2500ms"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_SACN_BLANK_ON_START, default=True): cv.boolean,
    },
)
@register_monochromatic_effect(
    "sacn",
    SACNLightEffect,
    "sACN",
    {
        cv.GenerateID(CONF_SACN_ID): cv.use_id(SACNComponent),
        cv.Optional(CONF_SACN_UNIVERSE, default=1): cv.int_range(min=1, max=63999),
        cv.Optional(CONF_SACN_START_CHANNEL, default=1): cv.int_range(min=1, max=512),
        cv.Optional(CONF_SACN_CHANNEL_TYPE, default=CHANNEL_MONO): cv.one_of(CHANNEL_MONO, upper=True),
        cv.Optional(CONF_SACN_TRANSPORT_MODE, default="UNICAST"): cv.one_of(*SACN_TRANSPORT_MODE, upper=True),
        cv.Optional(CONF_SACN_TIMEOUT, default="2500ms"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_SACN_BLANK_ON_START, default=True): cv.boolean,
    },
)
@register_addressable_effect(
    "addressable_sacn",
    SACNAddressableLightEffect,
    "Addressable sACN",
    {
        cv.GenerateID(CONF_SACN_ID): cv.use_id(SACNComponent),
        cv.Optional(CONF_SACN_UNIVERSE, default=1): cv.int_range(min=1, max=63999),
        cv.Optional(CONF_SACN_START_CHANNEL, default=1): cv.int_range(min=1, max=512),
        cv.Optional(CONF_SACN_CHANNEL_TYPE, default="RGB"): cv.one_of(CHANNEL_MONO, CHANNEL_RGB, CHANNEL_RGBW, CHANNEL_RGBWW, upper=True),
        cv.Optional(CONF_SACN_TRANSPORT_MODE, default="UNICAST"): cv.one_of(*SACN_TRANSPORT_MODE, upper=True),
        cv.Optional(CONF_SACN_TIMEOUT, default="2500ms"): cv.positive_time_period_milliseconds,
    },
)
async def sacn_light_effect_to_code(config, effect_id):
    parent = await cg.get_variable(config[CONF_SACN_ID])
    var = cg.new_Pvariable(effect_id, config[CONF_NAME])

    cg.add(var.set_sacn(parent))
    cg.add(var.set_universe(config[CONF_SACN_UNIVERSE]))
    cg.add(var.set_start_channel(config[CONF_SACN_START_CHANNEL]))
    cg.add(var.set_channel_type(SACN_CHANNEL_TYPE[config[CONF_SACN_CHANNEL_TYPE]]))
    cg.add(var.set_transport_mode(SACN_TRANSPORT_MODE[config[CONF_SACN_TRANSPORT_MODE]]))
    cg.add(var.set_timeout(config[CONF_SACN_TIMEOUT]))
    cg.add(var.set_blank_on_start(config[CONF_SACN_BLANK_ON_START]))

    return var