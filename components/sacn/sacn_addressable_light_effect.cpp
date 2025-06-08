#ifdef USE_ARDUINO

#include "sacn.h"
#include "sacn_addressable_light_effect.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace sacn {

static const char *const TAG = "sacn_addressable_light_effect";

// Gamma correction table for DMX values (gamma = 2.8)
static const uint8_t DMX_GAMMA_TABLE[256] = {
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   1,   1,   1,
    1,   1,   1,   1,   1,   1,   1,   1,   1,   2,   2,   2,   2,   2,   2,   2,
    2,   3,   3,   3,   3,   3,   3,   3,   4,   4,   4,   4,   4,   5,   5,   5,
    5,   6,   6,   6,   6,   7,   7,   7,   7,   8,   8,   8,   9,   9,   9,   10,
    10,  10,  11,  11,  11,  12,  12,  13,  13,  13,  14,  14,  15,  15,  16,  16,
    17,  17,  18,  18,  19,  19,  20,  20,  21,  21,  22,  22,  23,  24,  24,  25,
    25,  26,  27,  27,  28,  29,  29,  30,  31,  32,  32,  33,  34,  35,  35,  36,
    37,  38,  39,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  50,
    51,  52,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  64,  66,  67,  68,
    69,  70,  72,  73,  74,  75,  77,  78,  79,  81,  82,  83,  85,  86,  87,  89,
    90,  92,  93,  95,  96,  98,  99,  101, 102, 104, 105, 107, 109, 110, 112, 114,
    115, 117, 119, 120, 122, 124, 126, 127, 129, 131, 133, 135, 137, 138, 140, 142,
    144, 146, 148, 150, 152, 154, 156, 158, 160, 162, 164, 167, 169, 171, 173, 175,
    177, 180, 182, 184, 186, 189, 191, 193, 196, 198, 200, 203, 205, 208, 210, 213,
    215, 218, 220, 223, 225, 228, 231, 233, 236, 239, 241, 244, 247, 249, 252, 255
};

SACNAddressableLightEffect::SACNAddressableLightEffect(const std::string &name) : AddressableLightEffect(name) {}

const std::string &SACNAddressableLightEffect::get_name() { return AddressableLightEffect::get_name(); }

void SACNAddressableLightEffect::start() {
  auto *it = this->get_addressable_();
  this->last_colors_.resize(it->size(), Color::BLACK);
  this->data_received_ = false;
  
  // Set initial state in Home Assistant - show as white at full brightness
  auto call = this->state_->make_call();
  call.set_color_mode_if_supported(light::ColorMode::RGB);
  call.set_red(1.0f);
  call.set_green(1.0f);
  call.set_blue(1.0f);
  call.set_brightness(1.0f);
  call.set_publish(true);  // Publish initial state
  call.set_save(false);
  call.perform();
  
  AddressableLightEffect::start();
  SACNLightEffectBase::start();
  
  // Not automatically active until data is received
  it->set_effect_active(false);
}

void SACNAddressableLightEffect::stop() {
  this->last_colors_.clear();
  this->data_received_ = false;
  
  SACNLightEffectBase::stop();
  AddressableLightEffect::stop();
}

void SACNAddressableLightEffect::apply(light::AddressableLight &it, const Color &current_color) {
  // If receiving sACN packets times out, reset to Home Assistant color
  if (this->timeout_check()) {
    ESP_LOGD(TAG, "sACN stream for '%s->%s' timed out.", this->state_->get_name().c_str(), this->get_name().c_str());
    
    auto call = this->state_->turn_on();
    call.set_color_mode_if_supported(this->state_->remote_values.get_color_mode());
    call.set_red_if_supported(this->state_->remote_values.get_red());
    call.set_green_if_supported(this->state_->remote_values.get_green());
    call.set_blue_if_supported(this->state_->remote_values.get_blue());
    call.set_white_if_supported(this->state_->remote_values.get_white());
    call.set_brightness_if_supported(this->state_->remote_values.get_brightness());
    call.set_color_brightness_if_supported(this->state_->remote_values.get_color_brightness());
    
    call.set_publish(true);  // Publish state change when effect ends
    call.set_save(false);
    
    // Effect no longer active
    it.set_effect_active(false);
    this->data_received_ = false;
    
    call.perform();
  }
  
  // If we have received data, apply the last known colors
  if (this->data_received_) {
    for (size_t i = 0; i < it.size() && i < this->last_colors_.size(); i++) {
      auto output = it[i];
      output.set(this->last_colors_[i]);
    }
    it.schedule_show();
  }
}

uint16_t SACNAddressableLightEffect::process_(const uint8_t *payload, uint16_t size, uint16_t used) {
  auto *it = this->get_addressable_();
  
  // Calculate number of pixels we can update based on available data and channel type
  uint16_t channels_per_pixel;
  switch (this->channel_type_) {
    case SACN_MONO:
      channels_per_pixel = 1;
      break;
    case SACN_RGB:
      channels_per_pixel = 3;
      break;
    case SACN_RGBW:
      channels_per_pixel = 4;
      break;
    default:
      return 0;
  }
  
  uint16_t num_pixels = std::min((size_t)it->size(), (size_t)((size - used) / channels_per_pixel));
  if (num_pixels < 1) {
    return 0;
  }
  
  ESP_LOGV(TAG, "Applying sACN data for '%s' (size: %d - used: %d - num_pixels: %d - channels_per_pixel: %d)",
           get_name().c_str(), size, used, num_pixels, channels_per_pixel);
  
  this->last_sacn_time_ms_ = millis();
  this->data_received_ = true;
  it->set_effect_active(true);
  
  // Process pixels based on channel type
  for (uint16_t i = 0; i < num_pixels; i++) {
    uint16_t data_offset = used + (i * channels_per_pixel);
    auto output = (*it)[i];
    
    switch (this->channel_type_) {
      case SACN_MONO: {
        uint8_t brightness = DMX_GAMMA_TABLE[payload[data_offset]];
        Color color(brightness, brightness, brightness, 0);
        output.set(color);
        this->last_colors_[i] = color;
        break;
      }
      
      case SACN_RGB: {
        Color color(DMX_GAMMA_TABLE[payload[data_offset]],     // Red
                   DMX_GAMMA_TABLE[payload[data_offset + 1]],  // Green
                   DMX_GAMMA_TABLE[payload[data_offset + 2]],  // Blue
                   0);                                         // White
        output.set(color);
        this->last_colors_[i] = color;
        break;
      }
      
      case SACN_RGBW: {
        Color color(DMX_GAMMA_TABLE[payload[data_offset]],     // Red
                   DMX_GAMMA_TABLE[payload[data_offset + 1]],  // Green
                   DMX_GAMMA_TABLE[payload[data_offset + 2]],  // Blue
                   DMX_GAMMA_TABLE[payload[data_offset + 3]]); // White
        output.set(color);
        this->last_colors_[i] = color;
        break;
      }
    }
  }
  
  it->schedule_show();
  return num_pixels * channels_per_pixel;
}

}  // namespace sacn
}  // namespace esphome

#endif  // USE_ARDUINO 