#ifdef USE_ARDUINO

#include "sacn.h"
#include "sacn_light_effect.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace sacn {

static const char *const TAG = "sacn_light_effect";

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
      // Apply gamma correction and scale to 0-1
      red = green = blue = DMX_GAMMA_TABLE[raw_red] / 255.0f;
      this->last_values_[0] = red;
      ESP_LOGV(TAG, "[%u] Received sACN MONO data for '%s': Raw=%02X (%d), Scaled=%d%%", 
               millis(), this->state_->get_name().c_str(), 
               raw_red, raw_red, (int)(red * 100));
      break;
    }
      
    case SACN_RGB: {
      raw_red = payload[used];
      raw_green = payload[used + 1];
      raw_blue = payload[used + 2];
      // Apply gamma correction and scale to 0-1
      red = DMX_GAMMA_TABLE[raw_red] / 255.0f;
      green = DMX_GAMMA_TABLE[raw_green] / 255.0f;
      blue = DMX_GAMMA_TABLE[raw_blue] / 255.0f;
      this->last_values_[0] = red;
      this->last_values_[1] = green;
      this->last_values_[2] = blue;
      ESP_LOGV(TAG, "[%u] Received sACN RGB data for '%s': Raw=[%02X %02X %02X] (%d,%d,%d), Scaled=[%d%% %d%% %d%%]", 
               millis(), this->state_->get_name().c_str(), 
               raw_red, raw_green, raw_blue,
               raw_red, raw_green, raw_blue,
               (int)(red * 100), (int)(green * 100), (int)(blue * 100));
      break;
    }
      
    case SACN_RGBW: {
      raw_red = payload[used];
      raw_green = payload[used + 1];
      raw_blue = payload[used + 2];
      raw_white = payload[used + 3];
      // Apply gamma correction and scale to 0-1
      red = DMX_GAMMA_TABLE[raw_red] / 255.0f;
      green = DMX_GAMMA_TABLE[raw_green] / 255.0f;
      blue = DMX_GAMMA_TABLE[raw_blue] / 255.0f;
      white = DMX_GAMMA_TABLE[raw_white] / 255.0f;
      this->last_values_[0] = red;
      this->last_values_[1] = green;
      this->last_values_[2] = blue;
      this->last_values_[3] = white;
      ESP_LOGV(TAG, "[%u] Received sACN RGBW data for '%s': Raw=[%02X %02X %02X %02X] (%d,%d,%d,%d), Scaled=[%d%% %d%% %d%% %d%%]", 
               millis(), this->state_->get_name().c_str(), 
               raw_red, raw_green, raw_blue, raw_white,
               raw_red, raw_green, raw_blue, raw_white,
               (int)(red * 100), (int)(green * 100), (int)(blue * 100), (int)(white * 100));
      break;
    }
  }

  // Create a new light state call
  auto call = this->state_->turn_on();
  
  // Set color mode and values based on channel type
  switch (this->channel_type_) {
    case SACN_MONO:
      call.set_state(true);  // Ensure light is on
      call.set_color_mode(light::ColorMode::BRIGHTNESS);
      call.set_brightness(red);
      break;
      
    case SACN_RGB:
      call.set_state(true);  // Ensure light is on
      call.set_color_mode(light::ColorMode::RGB);
      call.set_red(red);
      call.set_green(green);
      call.set_blue(blue);
      call.set_brightness(1.0f);  // Full brightness, let RGB values control intensity
      break;
      
    case SACN_RGBW:
      call.set_state(true);  // Ensure light is on
      call.set_color_mode(light::ColorMode::RGB_WHITE);
      call.set_red(red);
      call.set_green(green);
      call.set_blue(blue);
      call.set_white(white);
      call.set_brightness(1.0f);  // Full brightness, let RGBW values control intensity
      break;
  }

  // Configure the light call
  call.set_transition_length(0);  // Instant transitions for DMX-like control
  call.set_publish(false);  // Don't publish state changes to HA during effect
  call.set_save(false);  // Don't save state

  // Apply the state change
  call.perform();

  // Manually call loop to ensure the light state is updated immediately
  this->state_->loop();

  return this->channel_type_;
}

}  // namespace sacn
}  // namespace esphome

#endif  // USE_ARDUINO 