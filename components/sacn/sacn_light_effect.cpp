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

  // Process based on channel type
  float red = 0.0f, green = 0.0f, blue = 0.0f, white = 0.0f;
  uint8_t raw_red = 0, raw_green = 0, raw_blue = 0, raw_white = 0;
  
  switch (this->channel_type_) {
    case SACN_MONO: {
      raw_red = payload[used];
      red = green = blue = (float)raw_red / 255.0f;
      this->last_values_[0] = red;
      ESP_LOGD(TAG, "[MONO] Raw=%d, Scaled=%f", raw_red, red);
      break;
    }
      
    case SACN_RGB: {
      raw_red = payload[used];
      raw_green = payload[used + 1];
      raw_blue = payload[used + 2];
      
      ESP_LOGD(TAG, "[RGB-RAW] Input values: R=%d, G=%d, B=%d", raw_red, raw_green, raw_blue);
      
      // Simple linear scaling from 0-255 to 0.0-1.0
      red = (float)raw_red / 255.0f;
      green = (float)raw_green / 255.0f;
      blue = (float)raw_blue / 255.0f;
      
      ESP_LOGD(TAG, "[RGB-SCALED] After 0-255 to 0.0-1.0 scaling: R=%f, G=%f, B=%f", red, green, blue);
      
      this->last_values_[0] = red;
      this->last_values_[1] = green;
      this->last_values_[2] = blue;
      
      ESP_LOGD(TAG, "[RGB-STORED] Values stored in last_values_: R=%f, G=%f, B=%f", 
               this->last_values_[0], this->last_values_[1], this->last_values_[2]);
      break;
    }
      
    case SACN_RGBW: {
      raw_red = payload[used];
      raw_green = payload[used + 1];
      raw_blue = payload[used + 2];
      raw_white = payload[used + 3];
      
      ESP_LOGD(TAG, "[RGBW-RAW] Input values: R=%d, G=%d, B=%d, W=%d", 
               raw_red, raw_green, raw_blue, raw_white);
      
      // Simple linear scaling from 0-255 to 0.0-1.0
      red = (float)raw_red / 255.0f;
      green = (float)raw_green / 255.0f;
      blue = (float)raw_blue / 255.0f;
      white = (float)raw_white / 255.0f;
      
      ESP_LOGD(TAG, "[RGBW-SCALED] After 0-255 to 0.0-1.0 scaling: R=%f, G=%f, B=%f, W=%f", 
               red, green, blue, white);
      
      this->last_values_[0] = red;
      this->last_values_[1] = green;
      this->last_values_[2] = blue;
      this->last_values_[3] = white;
      
      ESP_LOGD(TAG, "[RGBW-STORED] Values stored in last_values_: R=%f, G=%f, B=%f, W=%f", 
               this->last_values_[0], this->last_values_[1], this->last_values_[2], this->last_values_[3]);
      break;
    }
  }

  // Create a new light state call
  auto call = this->state_->turn_on();

  // Set color mode in order of preference (most capable to least capable)
  call.set_color_mode_if_supported(light::ColorMode::RGB_COLD_WARM_WHITE);
  call.set_color_mode_if_supported(light::ColorMode::RGB_COLOR_TEMPERATURE);
  call.set_color_mode_if_supported(light::ColorMode::RGB_WHITE);
  call.set_color_mode_if_supported(light::ColorMode::RGB);

  // Set RGB values directly without any scaling
  call.set_red_if_supported(red);
  call.set_green_if_supported(green);
  call.set_blue_if_supported(blue);

  // Set brightness to the maximum RGB value to prevent auto-scaling
  call.set_brightness_if_supported(std::max(red, std::max(green, blue)));
  call.set_color_brightness_if_supported(1.0f);

  // For RGBW mode, set white channel
  if (this->channel_type_ == SACN_RGBW) {
    call.set_white_if_supported(white);
  } else {
    // Disable white channels for RGB/MONO modes
    call.set_white_if_supported(0.0f);
    call.set_cold_white_if_supported(0.0f);
    call.set_warm_white_if_supported(0.0f);
  }

  // Configure the light call to be as direct as possible
  call.set_transition_length_if_supported(0);
  call.set_publish(false);
  call.set_save(false);

  // Apply the state change
  call.perform();
  ESP_LOGD(TAG, "[CALL] Light call performed");

  // Manually call loop to ensure immediate update
  this->state_->loop();

  return this->channel_type_;
}

}  // namespace sacn
}  // namespace esphome

#endif  // USE_ARDUINO 