#pragma once

#ifdef USE_ARDUINO

#include "esphome/core/component.h"
#include "esphome/components/light/light_effect.h"
#include "esphome/components/light/light_output.h"
#include "sacn_light_effect_base.h"

namespace esphome {
namespace sacn {

class SACNLightEffect : public SACNLightEffectBase, public light::LightEffect {
 public:
  SACNLightEffect(const std::string &name) : LightEffect(name), timeout_logged_(false) {}

  const std::string &get_name() override;

  void start() override;
  void stop() override;
  void apply() override;

 protected:
  uint16_t process_(const uint8_t *payload, uint16_t size, uint16_t used) override;
  
  // Store the last received values for each channel
  float last_values_[4] = {0.0f, 0.0f, 0.0f, 0.0f};  // RGBW values
  
  // Store the last time we logged (for rate limiting logs)
  uint32_t last_log_time_ms_{0};

  bool timeout_logged_{false};  // Track if timeout has been logged
};

}  // namespace sacn
}  // namespace esphome

#endif  // USE_ARDUINO 