#ifdef USE_ARDUINO

#include "sacn.h"
#include "sacn_light_effect.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sacn {

static const char *const TAG = "sacn_light_effect";

const std::string &SACNLightEffect::get_name() { return LightEffect::get_name(); }

void SACNLightEffect::start() {
  ESP_LOGD(TAG, "Starting sACN effect for '%s' (logging only mode)", this->state_->get_name().c_str());
  
  // Initialize last values
  for (int i = 0; i < 4; i++) {
    this->last_values_[i] = 0.0f;
  }
  
  LightEffect::start();
  SACNLightEffectBase::start();
}

void SACNLightEffect::stop() {
  ESP_LOGD(TAG, "Stopping sACN effect for '%s'", this->state_->get_name().c_str());
  
  SACNLightEffectBase::stop();
  LightEffect::stop();
}

void SACNLightEffect::apply() {
  // If receiving sACN packets times out, log it
  if (this->timeout_check()) {
    ESP_LOGD(TAG, "sACN stream for '%s->%s' timed out.", this->state_->get_name().c_str(), this->get_name().c_str());
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
      ESP_LOGD(TAG, "[%u] Received sACN MONO data for '%s': Raw=%02X (%d), Scaled=%d%%", 
               millis(), this->state_->get_name().c_str(), 
               raw_red, raw_red, (int)(red * 100));
      break;
    }
      
    case SACN_RGB: {
      raw_red = payload[used];
      raw_green = payload[used + 1];
      raw_blue = payload[used + 2];
      red = (float)raw_red / 255.0f;
      green = (float)raw_green / 255.0f;
      blue = (float)raw_blue / 255.0f;
      this->last_values_[0] = red;
      this->last_values_[1] = green;
      this->last_values_[2] = blue;
      ESP_LOGD(TAG, "[%u] Received sACN RGB data for '%s': Raw=[%02X %02X %02X] (%d,%d,%d), Scaled=[%d%% %d%% %d%%]", 
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
      red = (float)raw_red / 255.0f;
      green = (float)raw_green / 255.0f;
      blue = (float)raw_blue / 255.0f;
      white = (float)raw_white / 255.0f;
      this->last_values_[0] = red;
      this->last_values_[1] = green;
      this->last_values_[2] = blue;
      this->last_values_[3] = white;
      ESP_LOGD(TAG, "[%u] Received sACN RGBW data for '%s': Raw=[%02X %02X %02X %02X] (%d,%d,%d,%d), Scaled=[%d%% %d%% %d%% %d%%]", 
               millis(), this->state_->get_name().c_str(), 
               raw_red, raw_green, raw_blue, raw_white,
               raw_red, raw_green, raw_blue, raw_white,
               (int)(red * 100), (int)(green * 100), (int)(blue * 100), (int)(white * 100));
      break;
    }
  }

  return this->channel_type_;
}

}  // namespace sacn
}  // namespace esphome

#endif  // USE_ARDUINO 