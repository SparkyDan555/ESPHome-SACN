#pragma once

#include "esphome/components/light/light.h"
#include "esphome/components/light/effects.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <WiFiUdp.h>

namespace esphome {
namespace e131_light {

class E131LightEffect : public light::Effect {
 public:
  E131LightEffect(const std::string &name) : Effect(name) {}

  void set_method(uint8_t method) { this->method_ = method; }
  void set_port(uint16_t port) { this->port_ = port; }
  void set_universe(uint16_t universe) { this->universe_ = universe; }
  void set_start_address(uint16_t start_address) { this->start_address_ = start_address; }
  void set_channels(uint8_t channels) { this->channels_ = channels; }

  void start() override {
    this->udp_.begin(this->port_);
    if (this->method_ == 0) {  // multicast
      this->udp_.beginMulticast(IPAddress(239, 255, 0, 1), this->port_);
    }
    this->last_update_ = 0;
  }

  void stop() override {
    this->udp_.stop();
  }

  void apply() override {
    if (this->udp_.parsePacket() == 0)
      return;

    uint8_t buffer[512];
    int len = this->udp_.read(buffer, sizeof(buffer));
    if (len < 126)  // Minimum E1.31 packet size
      return;

    // Check for E1.31 packet
    if (buffer[0] != 0x00 || buffer[1] != 0x10 || buffer[2] != 0x00 || buffer[3] != 0x00)
      return;

    // Check for DMX data
    if (buffer[4] != 0x02 || buffer[5] != 0x00)
      return;

    // Get universe
    uint16_t packet_universe = (buffer[113] << 8) | buffer[114];
    if (packet_universe != this->universe_)
      return;

    // Get DMX data
    uint16_t dmx_length = (buffer[123] << 8) | buffer[124];
    if (dmx_length > 512)
      return;

    // Process DMX data
    uint16_t dmx_start = 126;  // Start of DMX data
    uint16_t dmx_end = dmx_start + dmx_length;

    if (this->state_->is_addressable()) {
      this->process_addressable_light(buffer + dmx_start, dmx_length);
    } else {
      this->process_single_light(buffer + dmx_start, dmx_length);
    }
  }

 protected:
  void process_addressable_light(const uint8_t *data, uint16_t length) {
    auto *addr_light = (light::AddressableLight *) this->state_;
    uint16_t num_leds = addr_light->size();
    uint16_t channels_per_led = this->channels_;
    uint16_t max_leds = length / channels_per_led;

    for (uint16_t i = 0; i < num_leds && i < max_leds; i++) {
      uint16_t base_addr = i * channels_per_led;
      if (base_addr + channels_per_led > length)
        break;

      light::ESPColor color;
      switch (this->channels_) {
        case 4:  // RGBW
          color.set_r(data[base_addr]);
          color.set_g(data[base_addr + 1]);
          color.set_b(data[base_addr + 2]);
          color.set_w(data[base_addr + 3]);
          break;
        case 3:  // RGB
          color.set_r(data[base_addr]);
          color.set_g(data[base_addr + 1]);
          color.set_b(data[base_addr + 2]);
          break;
        case 1:  // MONO
          color.set_r(data[base_addr]);
          color.set_g(data[base_addr]);
          color.set_b(data[base_addr]);
          break;
      }
      addr_light[i] = color;
    }
    addr_light->show();
  }

  void process_single_light(const uint8_t *data, uint16_t length) {
    if (length < this->start_address_)
      return;

    uint8_t value = data[this->start_address_ - 1];
    this->state_->current_values.set_brightness(value / 255.0f);
    this->state_->start_transition();
  }

  WiFiUDP udp_;
  uint8_t method_{0};  // 0 = multicast, 1 = unicast
  uint16_t port_{5568};
  uint16_t universe_{1};
  uint16_t start_address_{1};
  uint8_t channels_{3};  // 1 = MONO, 3 = RGB, 4 = RGBW
  uint32_t last_update_{0};
};

}  // namespace e131_light
}  // namespace esphome 