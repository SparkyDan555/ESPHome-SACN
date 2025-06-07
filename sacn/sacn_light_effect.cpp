#ifdef USE_ARDUINO

#include "sacn.h"
#include "sacn_light_effect.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sacn {

static const char *const TAG = "sacn_light_effect";

const std::string &SACNLightEffect::get_name() { return LightEffect::get_name(); }

void SACNLightEffect::start() {
  ESP_LOGD(TAG, "Starting sACN effect for '%s'", this->state_->get_name().c_str());
  
  // Initialize last values
  for (int i = 0; i < 4; i++) {
    this->last_values_[i] = 0.0f;
  }
  
  // Take control of the light state
  auto call = this->state_->make_call();
  call.set_effect(this->effect_id_);
  call.perform();
  
  LightEffect::start();
  SACNLightEffectBase::start();
}

void SACNLightEffect::stop() {
  ESP_LOGD(TAG, "Stopping sACN effect for '%s'", this->state_->get_name().c_str());
  
  // Clear effect and restore previous state
  auto call = this->state_->make_call();
  call.set_effect(0);  // 0 = no effect
  call.set_color_mode_if_supported(this->state_->remote_values.get_color_mode());
  call.set_red_if_supported(this->state_->remote_values.get_red());
  call.set_green_if_supported(this->state_->remote_values.get_green());
  call.set_blue_if_supported(this->state_->remote_values.get_blue());
  call.set_white_if_supported(this->state_->remote_values.get_white());
  call.set_brightness_if_supported(this->state_->remote_values.get_brightness());
  call.set_color_brightness_if_supported(this->state_->remote_values.get_color_brightness());
  call.perform();
  
  SACNLightEffectBase::stop();
  LightEffect::stop();
}

void SACNLightEffect::apply() {
  // If receiving sACN packets times out, reset to Home Assistant color
  if (this->timeout_check()) {
    ESP_LOGD(TAG, "sACN stream for '%s->%s' timed out.", this->state_->get_name().c_str(), this->get_name().c_str());
    
    // Clear effect and restore previous state
    auto call = this->state_->make_call();
    call.set_effect(0);  // 0 = no effect
    call.set_color_mode_if_supported(this->state_->remote_values.get_color_mode());
    call.set_red_if_supported(this->state_->remote_values.get_red());
    call.set_green_if_supported(this->state_->remote_values.get_green());
    call.set_blue_if_supported(this->state_->remote_values.get_blue());
    call.set_white_if_supported(this->state_->remote_values.get_white());
    call.set_brightness_if_supported(this->state_->remote_values.get_brightness());
    call.set_color_brightness_if_supported(this->state_->remote_values.get_color_brightness());
    call.perform();
  }
}

uint16_t SACNLightEffect::process_(const uint8_t *payload, uint16_t size, uint16_t used) {
  // Check if we have enough data based on channel type
  if (size < (used + this->channel_type_)) {
    return 0;
  }

  this->last_sacn_time_ms_ = millis();

  // Process based on channel type
  float red = 0.0f, green = 0.0f, blue = 0.0f, white = 0.0f;
  
  switch (this->channel_type_) {
    case SACN_MONO:
      red = green = blue = (float)payload[used] / 255.0f;
      this->last_values_[0] = red;
      ESP_LOGD(TAG, "Applying sACN MONO data for '%s': %d%%", 
               this->state_->get_name().c_str(), (int)(red * 100));
      break;
      
    case SACN_RGB:
      red = (float)payload[used] / 255.0f;
      green = (float)payload[used + 1] / 255.0f;
      blue = (float)payload[used + 2] / 255.0f;
      this->last_values_[0] = red;
      this->last_values_[1] = green;
      this->last_values_[2] = blue;
      ESP_LOGD(TAG, "Applying sACN RGB data for '%s': R=%d%% G=%d%% B=%d%%", 
               this->state_->get_name().c_str(), 
               (int)(red * 100), (int)(green * 100), (int)(blue * 100));
      break;
      
    case SACN_RGBW:
      red = (float)payload[used] / 255.0f;
      green = (float)payload[used + 1] / 255.0f;
      blue = (float)payload[used + 2] / 255.0f;
      white = (float)payload[used + 3] / 255.0f;
      this->last_values_[0] = red;
      this->last_values_[1] = green;
      this->last_values_[2] = blue;
      this->last_values_[3] = white;
      ESP_LOGD(TAG, "Applying sACN RGBW data for '%s': R=%d%% G=%d%% B=%d%% W=%d%%", 
               this->state_->get_name().c_str(), 
               (int)(red * 100), (int)(green * 100), (int)(blue * 100), (int)(white * 100));
      break;
  }

  auto call = this->state_->make_call();

  // Set color mode based on channel type
  switch (this->channel_type_) {
    case SACN_MONO:
      call.set_color_mode_if_supported(light::ColorMode::BRIGHTNESS);
      call.set_brightness(red);
      break;
      
    case SACN_RGB:
      call.set_color_mode_if_supported(light::ColorMode::RGB);
      call.set_red(red);
      call.set_green(green);
      call.set_blue(blue);
      call.set_brightness(1.0f);  // Full brightness, let RGB values control intensity
      break;
      
    case SACN_RGBW:
      call.set_color_mode_if_supported(light::ColorMode::RGB_WHITE);
      call.set_red(red);
      call.set_green(green);
      call.set_blue(blue);
      call.set_white(white);
      call.set_brightness(1.0f);  // Full brightness, let RGBW values control intensity
      break;
  }

  call.set_transition_length(0);  // Instant transitions for DMX-like control
  call.set_publish(true);  // Let HA know about state changes
  call.set_save(false);  // Don't save state
  call.perform();

  return this->channel_type_;
}

}  // namespace sacn
}  // namespace esphome

#endif  // USE_ARDUINO 