#ifdef USE_ARDUINO

#include "sacn.h"
#include "sacn_light_effect_base.h"

namespace esphome {
namespace sacn {

SACNLightEffectBase::SACNLightEffectBase() {}

void SACNLightEffectBase::start() {
  if (this->sacn_) {
    this->sacn_->add_effect(this);
  }
}

void SACNLightEffectBase::stop() {
  if (this->sacn_) {
    this->sacn_->remove_effect(this);
  }
}

// Returns true if this effect has timed out (no sACN data received within timeout period)
bool SACNLightEffectBase::timeout_check() {
  // Don't timeout if timeout is disabled
  if (this->timeout_ == 0) {
    return false;
  }

  // Don't timeout if no sACN data was ever received
  if (this->last_sacn_time_ms_ == 0) {
    return false;
  }

  // Check if we've exceeded the timeout period
  if ((millis() - this->last_sacn_time_ms_) <= this->timeout_) {
    return false;
  }

  return true;
}

}  // namespace sacn
}  // namespace esphome

#endif  // USE_ARDUINO 