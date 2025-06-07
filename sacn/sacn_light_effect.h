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
  SACNLightEffect(const std::string &name) : LightEffect(name) {}

  const std::string &get_name() override;

  void start() override;
  void stop() override;
  void apply() override;

  // Store the effect ID for later use
  void set_effect_id(uint32_t effect_id) { this->effect_id_ = effect_id; }
  uint32_t get_effect_id() const { return this->effect_id_; }

 protected:
  uint16_t process_(const uint8_t *payload, uint16_t size, uint16_t used) override;
  
  // Store the last received values for each channel
  float last_values_[4] = {0.0f, 0.0f, 0.0f, 0.0f};  // RGBW values

  // Store the effect ID assigned by the light state
  uint32_t effect_id_{0};
};

}  // namespace sacn
}  // namespace esphome

#endif  // USE_ARDUINO 