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
  
  // Initialize base classes first
  LightEffect::start();
  SACNLightEffectBase::start();
  
  // Set initial state in Home Assistant - show as white at full brightness
  {
    auto call = this->state_->make_call();
    call.set_state(true);  // Show as on in HA
    call.set_color_mode(light::ColorMode::RGB);
    call.set_red(1.0f);
    call.set_green(1.0f);
    call.set_blue(1.0f);
    call.set_brightness(1.0f);
    call.set_transition_length(0);
    call.set_publish(true);  // Publish initial state to HA
    call.set_save(false);
    call.perform();
  }

  // Reset flags
  this->initial_blank_done_ = false;
  this->timeout_logged_ = false;
}

void SACNLightEffect::stop() {
  // Prevent recursive calls
  static bool stopping = false;
  if (stopping) {
    return;
  }
  stopping = true;

  ESP_LOGD(TAG, "Stopping sACN effect for '%s'", this->state_->get_name().c_str());
  
  // First remove from sACN handling to stop receiving packets
  SACNLightEffectBase::stop();
  
  // Then stop the light effect without any state changes
  LightEffect::stop();
  
  // Reset stopping flag
  stopping = false;
}

void SACNLightEffect::apply() {
  // If receiving sACN packets times out, blank the output
  if (this->timeout_check()) {
    // Only log timeout once when stream stops
    if (!this->timeout_logged_) {
      ESP_LOGD(TAG, "sACN stream for '%s->%s' timed out.", this->state_->get_name().c_str(), this->get_name().c_str());
      this->timeout_logged_ = true;
      
      // Blank the light output
      auto call = this->state_->make_call();
      call.set_state(true);  // Keep light "on" but...
      call.set_color_mode(light::ColorMode::RGB);
      call.set_red(0.0f);    // Set all channels
      call.set_green(0.0f);  // to zero for
      call.set_blue(0.0f);   // blank output
      call.set_white(0.0f);
      call.set_brightness(0.0f);
      call.set_transition_length(0);
      call.set_publish(false);  // Don't publish this state to HA
      call.set_save(false);
      call.perform();
    }
  } else {
    // Reset timeout log flag when receiving packets
    this->timeout_logged_ = false;
  }

  // Handle initial blanking if enabled
  if (this->blank_on_start_ && !this->initial_blank_done_) {
    // Blank the light output
    auto call = this->state_->make_call();
    call.set_state(true);  // Keep light "on" but...
    call.set_color_mode(light::ColorMode::RGB);
    call.set_red(0.0f);    // Set all channels
    call.set_green(0.0f);  // to zero for
    call.set_blue(0.0f);   // blank output
    call.set_white(0.0f);
    call.set_brightness(0.0f);
    call.set_transition_length(0);
    call.set_publish(false);  // Don't publish this state to HA
    call.set_save(false);
    call.perform();
    
    this->initial_blank_done_ = true;
  }
}

uint16_t SACNLightEffect::process_(const uint8_t *payload, uint16_t size, uint16_t used) {
  // Check if we have enough data based on channel type
  if (size < (used + this->channel_type_)) {
    return 0;
  }

  this->last_sacn_time_ms_ = millis();

  // Process based on channel type and directly set output values
  switch (this->channel_type_) {
    case SACN_MONO: {
      float value = (float)payload[used] / 255.0f;
      auto call = this->state_->make_call();
      call.set_state(true);
      call.set_brightness(value);
      call.set_transition_length(0);
      call.set_publish(false);
      call.perform();
      ESP_LOGV(TAG, "Setting mono output to %f", value);
      break;
    }
      
    case SACN_RGB: {
      float red = (float)payload[used] / 255.0f;
      float green = (float)payload[used + 1] / 255.0f;
      float blue = (float)payload[used + 2] / 255.0f;
      
      auto call = this->state_->make_call();
      call.set_state(true);
      call.set_color_mode(light::ColorMode::RGB);
      call.set_red(red);
      call.set_green(green);
      call.set_blue(blue);
      call.set_brightness(1.0f);  // Set full brightness to prevent scaling
      call.set_transition_length(0);
      call.set_publish(false);
      call.perform();
      
      ESP_LOGV(TAG, "Setting RGB outputs to R=%f, G=%f, B=%f", red, green, blue);
      break;
    }
      
    case SACN_RGBW: {
      float red = (float)payload[used] / 255.0f;
      float green = (float)payload[used + 1] / 255.0f;
      float blue = (float)payload[used + 2] / 255.0f;
      float white = (float)payload[used + 3] / 255.0f;
      
      auto call = this->state_->make_call();
      call.set_state(true);
      call.set_color_mode(light::ColorMode::RGB_WHITE);
      call.set_red(red);
      call.set_green(green);
      call.set_blue(blue);
      call.set_white(white);
      call.set_brightness(1.0f);  // Set full brightness to prevent scaling
      call.set_transition_length(0);
      call.set_publish(false);
      call.perform();
      
      ESP_LOGV(TAG, "Setting RGBW outputs to R=%f, G=%f, B=%f, W=%f", red, green, blue, white);
      break;
    }
  }

  // Call loop to ensure immediate update
  this->state_->loop();

  return this->channel_type_;
}

}  // namespace sacn
}  // namespace esphome

#endif  // USE_ARDUINO 