#pragma once

#ifdef USE_ARDUINO

#include "esphome/core/component.h"
#include "esphome/components/light/light_effect.h"
#include "esphome/components/light/light_output.h"

namespace esphome {
namespace sacn {

class SACNComponent;

enum SACNChannelType {
  SACN_MONO = 1,  // Single channel (intensity)
  SACN_RGB = 3,   // RGB channels
  SACN_RGBW = 4   // RGBW channels
};

enum SACNTransportMode {
  SACN_UNICAST = 0,
  SACN_MULTICAST = 1
};

class SACNLightEffectBase {
 public:
  SACNLightEffectBase();

  virtual const std::string &get_name() = 0;

  virtual void start();
  virtual void stop();
  bool timeout_check();

  void set_sacn(SACNComponent *sacn) { this->sacn_ = sacn; }
  void set_timeout(uint32_t timeout) { this->timeout_ = timeout; }
  void set_universe(uint16_t universe) { this->universe_ = universe; }
  void set_start_channel(uint16_t start_channel) { this->start_channel_ = start_channel; }
  void set_channel_type(SACNChannelType channel_type) { this->channel_type_ = channel_type; }
  void set_transport_mode(SACNTransportMode transport_mode) { this->transport_mode_ = transport_mode; }

 protected:
  SACNComponent *sacn_{nullptr};

  uint32_t timeout_{2500};  // Default timeout 2.5s
  uint32_t last_sacn_time_ms_{0};
  uint16_t universe_{1};  // Default universe 1
  uint16_t start_channel_{1};  // Default start channel 1
  SACNChannelType channel_type_{SACN_RGB};  // Default to RGB
  SACNTransportMode transport_mode_{SACN_UNICAST};  // Default to unicast

  virtual uint16_t process_(const uint8_t *payload, uint16_t size, uint16_t used) = 0;

  friend class SACNComponent;
};

}  // namespace sacn
}  // namespace esphome

#endif  // USE_ARDUINO 