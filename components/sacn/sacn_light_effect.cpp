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

  // Convert DMX values to float (0.0-1.0)
  float red = (float)payload[used] / 255.0f;
  float green = this->channel_type_ >= SACN_RGB ? (float)payload[used + 1] / 255.0f : 0.0f;
  float blue = this->channel_type_ >= SACN_RGB ? (float)payload[used + 2] / 255.0f : 0.0f;
  float white = this->channel_type_ == SACN_RGBW ? (float)payload[used + 3] / 255.0f : 0.0f;

  // Create a new light state call
  auto call = this->state_->turn_on();

  // Handle different channel types
  if (this->channel_type_ == SACN_RGBW) {
    // For RGBW, we want direct control of all channels independently
    call.set_color_mode_if_supported(light::ColorMode::RGB_WHITE);
    call.set_red_if_supported(red);
    call.set_green_if_supported(green);
    call.set_blue_if_supported(blue);
    call.set_white_if_supported(white);
    
    // Set brightness to 1.0 to prevent scaling of any channels
    call.set_brightness_if_supported(1.0f);
    call.set_color_brightness_if_supported(1.0f);
    
    ESP_LOGV(TAG, "Setting RGBW outputs directly: R=%f, G=%f, B=%f, W=%f", red, green, blue, white);
  } else if (this->channel_type_ == SACN_RGB) {
    // For RGB, use max value for brightness to maintain color ratios
    call.set_color_mode_if_supported(light::ColorMode::RGB);
    call.set_red_if_supported(red);
    call.set_green_if_supported(green);
    call.set_blue_if_supported(blue);
    
    // Set brightness to max RGB value for proper scaling
    call.set_brightness_if_supported(std::max(red, std::max(green, blue)));
    call.set_color_brightness_if_supported(1.0f);
    
    // Ensure white channels are off
    call.set_white_if_supported(0.0f);
    call.set_cold_white_if_supported(0.0f);
    call.set_warm_white_if_supported(0.0f);
    
    ESP_LOGV(TAG, "Setting RGB outputs: R=%f, G=%f, B=%f", red, green, blue);
  } else {  // SACN_MONO
    // For mono, use the single channel directly for brightness
    call.set_color_mode_if_supported(light::ColorMode::BRIGHTNESS);
    call.set_brightness_if_supported(red);
    
    ESP_LOGV(TAG, "Setting mono output to %f", red);
  }

  // Configure the light call to be as direct as possible
  call.set_transition_length_if_supported(0);
  call.set_publish(false);
  call.set_save(false);

  // Perform the light call
  call.perform();

  // Manually call loop to ensure immediate update
  this->state_->loop();

  return this->channel_type_;
}

}  // namespace sacn
}  // namespace esphome

#endif  // USE_ARDUINO 