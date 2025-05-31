import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PORT
from esphome.core import CORE
from esphome.components import light
from esphome.components.light import Effect

DEPENDENCIES = ['network']
CODEOWNERS = ['@your-github-username']

e131_light_ns = cg.esphome_ns.namespace('e131_light')
E131Component = e131_light_ns.class_('E131Component', cg.Component)
E131LightEffect = e131_light_ns.class_('E131LightEffect', Effect)

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

# Global component configuration
E131_CONFIG_SCHEMA = cv.Schema({
    cv.Optional(CONF_METHOD, default='multicast'): cv.enum(METHODS, lower=True),
    cv.Optional(CONF_PORT, default=5568): cv.port,
})

# Effect configuration
E131_EFFECT_SCHEMA = cv.Schema({
    cv.Required(CONF_UNIVERSE): cv.int_range(min=1, max=512),
    cv.Optional(CONF_START_ADDRESS, default=1): cv.int_range(min=1, max=512),
    cv.Optional(CONF_CHANNELS, default='RGB'): cv.enum(CHANNEL_TYPES, upper=True),
}).extend(light.EFFECT_SCHEMA)

CONFIG_SCHEMA = cv.All(
    cv.ensure_list(cv.Schema({
        cv.GenerateID(): cv.declare_id(E131Component),
    }).extend(E131_CONFIG_SCHEMA)),
    cv.Length(min=1, max=1),
)

async def to_code(config):
    var = cg.new_Pvariable(config[0][CONF_ID])
    await cg.register_component(var, config[0])
    
    cg.add(var.set_method(config[0][CONF_METHOD]))
    cg.add(var.set_port(config[0][CONF_PORT]))

# Effect registration
@light.register_effect('e131_light', E131LightEffect, E131_EFFECT_SCHEMA)
async def e131_light_effect_to_code(config, effect_id):
    var = cg.new_Pvariable(effect_id)
    await light.register_effect(var, config)
    
    cg.add(var.set_universe(config[CONF_UNIVERSE]))
    cg.add(var.set_start_address(config[CONF_START_ADDRESS]))
    cg.add(var.set_channels(config[CONF_CHANNELS])) 