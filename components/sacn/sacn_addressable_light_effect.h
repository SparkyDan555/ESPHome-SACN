#pragma once

#ifdef USE_ARDUINO

#include "esphome/core/component.h"
#include "esphome/components/light/addressable_light_effect.h"
#include "sacn_light_effect_base.h"

namespace esphome {
namespace sacn {

class SACNAddressableLightEffect : public SACNLightEffectBase, public light::AddressableLightEffect {
 public:
  SACNAddressableLightEffect(const std::string &name);

  const std::string &get_name() override;

  void start() override;
  void stop() override;

  void apply(light::AddressableLight &it, const Color &current_color) override;

 protected:
  uint16_t process_(const uint8_t *payload, uint16_t size, uint16_t used) override;

  // Store the last received values for each LED
  std::vector<Color> last_colors_;
  bool data_received_{false};
};

}  // namespace sacn
}  // namespace esphome

#endif  // USE_ARDUINO