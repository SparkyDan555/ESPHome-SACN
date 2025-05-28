import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PORT
from esphome.core import CORE
from esphome.components import light

DEPENDENCIES = ['network']
CODEOWNERS = ['@your-github-username']

e131_light_ns = cg.esphome_ns.namespace('e131_light')
E131LightEffect = e131_light_ns.class_('E131LightEffect', light.LightEffect)

CONF_METHOD = 'method'
CONF_UNIVERSE = 'universe'
CONF_START_ADDRESS = 'start_address'
CONF_CHANNELS = 'channels'

METHODS = {
    'multicast': 0,
    'unicast': 1,
}

CHANNEL_TYPES = {
    'MONO': 1,
    'RGB': 3,
    'RGBW': 4,
}

CONFIG_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): cv.declare_id(E131LightEffect),
    cv.Optional(CONF_METHOD, default='multicast'): cv.enum(METHODS, lower=True),
    cv.Optional(CONF_PORT, default=5568): cv.port,
    cv.Required(CONF_UNIVERSE): cv.int_range(min=1, max=512),
    cv.Optional(CONF_START_ADDRESS, default=1): cv.int_range(min=1, max=512),
    cv.Optional(CONF_CHANNELS, default='RGB'): cv.enum(CHANNEL_TYPES, upper=True),
}).extend(light.LIGHT_EFFECT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await light.register_light_effect(var, config)

    cg.add(var.set_method(config[CONF_METHOD]))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_universe(config[CONF_UNIVERSE]))
    cg.add(var.set_start_address(config[CONF_START_ADDRESS]))
    cg.add(var.set_channels(config[CONF_CHANNELS])) 