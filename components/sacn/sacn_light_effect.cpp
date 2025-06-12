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

  // Get raw DMX values
  uint8_t raw_mono = payload[used];
  uint8_t raw_red = payload[used];
  uint8_t raw_green = this->channel_type_ >= SACN_RGB ? payload[used + 1] : 0;
  uint8_t raw_blue = this->channel_type_ >= SACN_RGB ? payload[used + 2] : 0;
  uint8_t raw_white = this->channel_type_ == SACN_RGBW ? payload[used + 3] : 0;
  uint8_t raw_cold_white = this->channel_type_ == SACN_RGBWW ? payload[used + 3] : 0;
  uint8_t raw_warm_white = this->channel_type_ == SACN_RGBWW ? payload[used + 4] : 0;

  // Convert DMX values to float (0.0-1.0)
  float mono = (float)raw_mono / 255.0f;
  float red = (float)raw_red / 255.0f;
  float green = (float)raw_green / 255.0f;
  float blue = (float)raw_blue / 255.0f;
  float white = (float)raw_white / 255.0f;
  float cold_white = (float)raw_cold_white / 255.0f;
  float warm_white = (float)raw_warm_white / 255.0f;

  // Create a new light state call
  auto call = this->state_->turn_on();

  if (this->channel_type_ == SACN_RGBWW) {
    //call.set_color_mode(light::ColorMode::RGB_COLD_WARM_WHITE);
    call.set_red_if_supported(red);
    call.set_green_if_supported(green);
    call.set_blue_if_supported(blue);
    call.set_cold_white_if_supported(cold_white);
    call.set_warm_white_if_supported(warm_white);
    float max_brightness = std::max({red, green, blue, cold_white, warm_white});
    call.set_brightness(max_brightness);
    float max_rgb = std::max({red, green, blue});
    if( max_rgb > 0.0f) { // Only set color brightness if there is any RGB value
      call.set_color_brightness(max_rgb);
    } else {
      call.set_color_brightness(0.0f);
    }


  } else if (this->channel_type_ == SACN_RGBW) {
    call.set_red_if_supported(red);
    call.set_green_if_supported(green);
    call.set_blue_if_supported(blue);
    if (white > 0) {
      call.set_white_if_supported(white); // For RGBW lights
      call.set_cold_white_if_supported(white); // For RGBWW lights
      call.set_warm_white_if_supported(white);
    } else {
      call.set_white_if_supported(white);
      call.set_cold_white_if_supported(0.0f);
      call.set_warm_white_if_supported(0.0f);
    }
    float max_brightness = std::max({red, green, blue, white});
    call.set_brightness(max_brightness);
    float max_rgb = std::max({red, green, blue});
    if( max_rgb > 0.0f) { // Only set color brightness if there is any RGB value
      call.set_color_brightness(max_rgb);
    } else {
      call.set_color_brightness(0.0f);
    }

  } else if (this->channel_type_ == SACN_RGB) {
    call.set_red_if_supported(red);
    call.set_green_if_supported(green);
    call.set_blue_if_supported(blue);
    float max_rgb = std::max({red, green, blue});
    call.set_color_brightness(max_rgb); // Set color brightness based on RGB values

  } else if (this->channel_type_ == SACN_MONO) {
    // For MONO modes, use standard BRIGHTNESS mode
    call.set_color_mode(light::ColorMode::BRIGHTNESS);
    call.set_red_if_supported(1.0f); // Set all supported channels to 1.0 to show white mono light
    call.set_green_if_supported(1.0f);
    call.set_blue_if_supported(1.0f);
    call.set_white_if_supported(1.0f);
    call.set_cold_white_if_supported(1.0f);
    call.set_warm_white_if_supported(1.0f);
    call.set_brightness(mono);  // Use mono value for brightness
  }


  // Configure the light call to be as direct as possible
  call.set_transition_length(0);
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