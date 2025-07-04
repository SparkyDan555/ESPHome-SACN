#ifdef USE_ARDUINO

#include "sacn.h"
#include "sacn_addressable_light_effect.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sacn {

static const char *const TAG = "sacn_addressable_light_effect";

SACNAddressableLightEffect::SACNAddressableLightEffect(const std::string &name) : AddressableLightEffect(name) {}

const std::string &SACNAddressableLightEffect::get_name() { return AddressableLightEffect::get_name(); }

void SACNAddressableLightEffect::start() {
  ESP_LOGD(TAG, "Starting Addressable sACN effect for '%s'", this->state_->get_name().c_str());
  auto *it = this->get_addressable_();
  this->last_colors_.resize(it->size(), Color::BLACK);
  this->data_received_ = false;

  // Blank the LEDs on start if requested and not already done
  if (this->blank_on_start_ && !this->initial_blank_done_) {
    for (size_t i = 0; i < it->size(); i++) {
      auto output = (*it)[i];
      output.set(Color::BLACK);
    }
    it->schedule_show();
    this->initial_blank_done_ = true;
  }

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
    case SACN_RGBWW:
      channels_per_pixel = 5;
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
        float brightness = payload[data_offset] / 255.0f;
        Color color(brightness * 255, brightness * 255, brightness * 255, 0);
        output.set(color);
        this->last_colors_[i] = color;
        break;
      }
      case SACN_RGB: {
        Color color(payload[data_offset],     // Red
                   payload[data_offset + 1],  // Green
                   payload[data_offset + 2],  // Blue
                   0);                        // White
        output.set(color);
        this->last_colors_[i] = color;
        break;
      }
      case SACN_RGBW: {
        Color color(payload[data_offset],     // Red
                   payload[data_offset + 1],  // Green
                   payload[data_offset + 2],  // Blue
                   payload[data_offset + 3]); // White
        output.set(color);
        this->last_colors_[i] = color;
        break;
      }
      case SACN_RGBWW: {
        // Set RGB using Color, but do NOT set white (leave at 0)
        Color color(payload[data_offset],     // Red
                   payload[data_offset + 1],  // Green
                   payload[data_offset + 2],  // Blue
                   0);                        // White (not used)
        output.set(color);
        // ESPColorView does not support set_cold_white/set_warm_white.
        // As a fallback, set the white channel to cold or warm white, whichever is higher.
        // This is the best possible mapping for addressable strips with only a single white channel.
        float cold_white = payload[data_offset + 3] / 255.0f;
        float warm_white = payload[data_offset + 4] / 255.0f;
        float white = std::max(cold_white, warm_white);
        output.set_white(white * 255);
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