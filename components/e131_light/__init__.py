import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PORT
from esphome.core import CORE
from esphome.components import light
from esphome.components.light.effects import register_addressable_effect
from esphome.components.light.types import AddressableLightEffect

DEPENDENCIES = ['network']
CODEOWNERS = ['@SparkyDan555']

e131_light_ns = cg.esphome_ns.namespace('e131_light')
E131Component = e131_light_ns.class_('E131Component', cg.Component)
E131LightEffect = e131_light_ns.class_('E131LightEffect', AddressableLightEffect)

CONF_METHOD = 'method'
CONF_UNIVERSE = 'universe'
CONF_START_ADDRESS = 'start_address'
CONF_CHANNELS = 'channels'
CONF_E131_ID = 'e131_id'

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
    cv.GenerateID(): cv.declare_id(E131Component),
    cv.Optional(CONF_METHOD, default='multicast'): cv.enum(METHODS, lower=True),
    cv.Optional(CONF_PORT, default=5568): cv.port,
})

CONFIG_SCHEMA = cv.All(
    cv.ensure_list(E131_CONFIG_SCHEMA),
    cv.Length(min=1, max=1),
)

async def to_code(config):
    var = cg.new_Pvariable(config[0][CONF_ID])
    await cg.register_component(var, config[0])
    
    cg.add(var.set_method(METHODS[config[0][CONF_METHOD]]))
    cg.add(var.set_port(config[0][CONF_PORT]))

# Effect registration
@register_addressable_effect(
    'e131_light',
    E131LightEffect,
    'E1.31 Light',
    {
        cv.GenerateID(CONF_E131_ID): cv.use_id(E131Component),
        cv.Required(CONF_UNIVERSE): cv.int_range(min=1, max=512),
        cv.Optional(CONF_START_ADDRESS, default=1): cv.int_range(min=1, max=512),
        cv.Optional(CONF_CHANNELS, default='RGB'): cv.enum(CHANNEL_TYPES, upper=True),
    },
)
async def e131_light_effect_to_code(config, effect_id):
    parent = await cg.get_variable(config[CONF_E131_ID])
    
    effect = cg.new_Pvariable(effect_id, config[CONF_NAME])
    cg.add(effect.set_universe(config[CONF_UNIVERSE]))
    cg.add(effect.set_start_address(config[CONF_START_ADDRESS]))
    cg.add(effect.set_channels(CHANNEL_TYPES[config[CONF_CHANNELS]]))
    cg.add(effect.set_e131(parent))
    return effect 