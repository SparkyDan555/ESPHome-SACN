#pragma once

#ifdef USE_ARDUINO

#include "esphome/core/component.h"

#ifdef USE_ESP32
#include <WiFi.h>
#endif

#ifdef USE_ESP8266
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#endif

#ifdef USE_LIBRETINY
#include <WiFi.h>
#include <WiFiUdp.h>
#endif

#include <map>
#include <memory>
#include <set>
#include <vector>

namespace esphome {
namespace sacn {

class SACNLightEffectBase;

class SACNComponent : public esphome::Component {
 public:
  SACNComponent();
  ~SACNComponent();

  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void add_effect(SACNLightEffectBase *light_effect);
  void remove_effect(SACNLightEffectBase *light_effect);

 protected:
  std::unique_ptr<WiFiUDP> udp_;
  std::set<SACNLightEffectBase *> light_effects_;
  
  // sACN specific members
  static const uint16_t SACN_PORT = 5568;  // Standard sACN port
  static const uint8_t SACN_PACKET_IDENTIFIER[12];  // Root layer protocol identifier
  static const uint8_t SACN_FRAMING_LAYER_IDENTIFIER[4];  // Framing layer identifier
  static const uint8_t SACN_DMP_LAYER_IDENTIFIER[4];  // DMP layer identifier
  
  // State tracking
  bool receiving_data_;  // Whether we're currently receiving sACN data
  uint32_t last_packet_time_;  // Time of last received packet
  
  bool validate_sacn_packet_(const uint8_t *payload, uint16_t size);
  uint16_t get_universe_(const uint8_t *payload);
  uint16_t get_start_address_(const uint8_t *payload);
  uint16_t get_property_value_count_(const uint8_t *payload);
  bool process_(const uint8_t *payload, uint16_t size);
};

}  // namespace sacn
}  // namespace esphome

#endif  // USE_ARDUINO 