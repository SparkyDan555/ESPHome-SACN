#pragma once

#include "esphome/components/light/light.h"
#include "esphome/components/light/effects.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <WiFiUdp.h>

namespace esphome {
namespace e131_light {

class E131Component : public Component {
 public:
  void set_method(uint8_t method) { this->method_ = method; }
  void set_port(uint16_t port) { this->port_ = port; }

  void setup() override {
    this->udp_.begin(this->port_);
    if (this->method_ == 0) {  // multicast
      this->udp_.beginMulticast(IPAddress(239, 255, 0, 1), this->port_);
    }
  }

  void loop() override {
    // Global component doesn't need to do anything in loop
  }

  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  WiFiUDP udp_;
  uint8_t method_{0};  // 0 = multicast, 1 = unicast
  uint16_t port_{5568};
};

// Base class for common E1.31 functionality
class E131BaseEffect : public light::LightEffect {
 public:
  E131BaseEffect(const std::string &name) : LightEffect(name) {}

  void set_universe(uint16_t universe) { this->universe_ = universe; }
  void set_start_address(uint16_t start_address) { this->start_address_ = start_address; }
  void set_channels(uint8_t channels) { this->channels_ = channels; }
  void set_e131(E131Component *e131) { this->e131_ = e131; }

  void start() override {
    this->last_update_ = 0;
  }

  void stop() override {
    // Nothing to do on stop
  }

 protected:
  bool process_packet(uint8_t *buffer, int len) {
    if (len < 126)  // Minimum E1.31 packet size
      return false;

    // Check for E1.31 packet
    if (buffer[0] != 0x00 || buffer[1] != 0x10 || buffer[2] != 0x00 || buffer[3] != 0x00)
      return false;

    // Check for DMX data
    if (buffer[4] != 0x02 || buffer[5] != 0x00)
      return false;

    // Get universe
    uint16_t packet_universe = (buffer[113] << 8) | buffer[114];
    if (packet_universe != this->universe_)
      return false;

    // Get DMX data
    uint16_t dmx_length = (buffer[123] << 8) | buffer[124];
    if (dmx_length > 512)
      return false;

    return true;
  }

  uint16_t universe_{1};
  uint16_t start_address_{1};
  uint8_t channels_{3};  // 1 = MONO, 3 = RGB, 4 = RGBW
  uint32_t last_update_{0};
  E131Component *e131_{nullptr};
};

// Effect for regular lights
class E131LightEffect : public E131BaseEffect {
 public:
  E131LightEffect(const std::string &name) : E131BaseEffect(name) {}

  void apply() override {
    if (this->e131_ == nullptr)
      return;

    if (this->e131_->udp_.parsePacket() == 0)
      return;

    uint8_t buffer[512];
    int len = this->e131_->udp_.read(buffer, sizeof(buffer));
    if (!this->process_packet(buffer, len))
      return;

    // Process DMX data
    uint16_t dmx_start = 126;  // Start of DMX data
    uint16_t dmx_length = (buffer[123] << 8) | buffer[124];
    this->process_light(buffer + dmx_start, dmx_length);
  }

 protected:
  void process_light(const uint8_t *data, uint16_t length) {
    if (length < this->start_address_)
      return;

    uint16_t base_addr = this->start_address_ - 1;
    if (base_addr + this->channels_ > length)
      return;

    switch (this->channels_) {
      case 4:  // RGBW
        this->state_->current_values.set_red(data[base_addr] / 255.0f);
        this->state_->current_values.set_green(data[base_addr + 1] / 255.0f);
        this->state_->current_values.set_blue(data[base_addr + 2] / 255.0f);
        this->state_->current_values.set_white(data[base_addr + 3] / 255.0f);
        break;
      case 3:  // RGB
        this->state_->current_values.set_red(data[base_addr] / 255.0f);
        this->state_->current_values.set_green(data[base_addr + 1] / 255.0f);
        this->state_->current_values.set_blue(data[base_addr + 2] / 255.0f);
        break;
      case 1:  // MONO
        this->state_->current_values.set_brightness(data[base_addr] / 255.0f);
        break;
    }
    this->state_->start_transition();
  }
};

// Effect for addressable lights
class E131AddressableLightEffect : public E131BaseEffect {
 public:
  E131AddressableLightEffect(const std::string &name) : E131BaseEffect(name) {}

  void apply() override {
    if (this->e131_ == nullptr)
      return;

    if (this->e131_->udp_.parsePacket() == 0)
      return;

    uint8_t buffer[512];
    int len = this->e131_->udp_.read(buffer, sizeof(buffer));
    if (!this->process_packet(buffer, len))
      return;

    // Process DMX data
    uint16_t dmx_start = 126;  // Start of DMX data
    uint16_t dmx_length = (buffer[123] << 8) | buffer[124];
    this->process_light(buffer + dmx_start, dmx_length);
  }

 protected:
  void process_light(const uint8_t *data, uint16_t length) {
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
};

}  // namespace e131_light
}  // namespace esphome 