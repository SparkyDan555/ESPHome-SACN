#include "e131_light.h"

namespace esphome {
namespace e131_light {

static const char *const TAG = "e131_light";

void E131LightEffect::start() {
  ESP_LOGD(TAG, "Starting E1.31 light effect");
  this->udp_.begin(this->port_);
  if (this->method_ == 0) {  // multicast
    this->udp_.beginMulticast(IPAddress(239, 255, 0, 1), this->port_);
    ESP_LOGD(TAG, "Started multicast on port %d", this->port_);
  } else {
    ESP_LOGD(TAG, "Started unicast on port %d", this->port_);
  }
  this->last_update_ = 0;
}

void E131LightEffect::stop() {
  ESP_LOGD(TAG, "Stopping E1.31 light effect");
  this->udp_.stop();
}

void E131LightEffect::apply() {
  if (this->udp_.parsePacket() == 0)
    return;

  uint8_t buffer[512];
  int len = this->udp_.read(buffer, sizeof(buffer));
  if (len < 126) {  // Minimum E1.31 packet size
    ESP_LOGV(TAG, "Packet too small: %d bytes", len);
    return;
  }

  // Check for E1.31 packet
  if (buffer[0] != 0x00 || buffer[1] != 0x10 || buffer[2] != 0x00 || buffer[3] != 0x00) {
    ESP_LOGV(TAG, "Invalid E1.31 packet header");
    return;
  }

  // Check for DMX data
  if (buffer[4] != 0x02 || buffer[5] != 0x00) {
    ESP_LOGV(TAG, "Not a DMX data packet");
    return;
  }

  // Get universe
  uint16_t packet_universe = (buffer[113] << 8) | buffer[114];
  if (packet_universe != this->universe_) {
    ESP_LOGV(TAG, "Wrong universe: got %d, expected %d", packet_universe, this->universe_);
    return;
  }

  // Get DMX data
  uint16_t dmx_length = (buffer[123] << 8) | buffer[124];
  if (dmx_length > 512) {
    ESP_LOGW(TAG, "DMX length too large: %d", dmx_length);
    return;
  }

  // Process DMX data
  uint16_t dmx_start = 126;  // Start of DMX data
  uint16_t dmx_end = dmx_start + dmx_length;

  if (this->light_->is_addressable()) {
    this->process_addressable_light(buffer + dmx_start, dmx_length);
  } else {
    this->process_single_light(buffer + dmx_start, dmx_length);
  }
}

void E131LightEffect::process_addressable_light(const uint8_t *data, uint16_t length) {
  auto *addr_light = (light::AddressableLight *) this->light_;
  uint16_t num_leds = addr_light->size();
  uint16_t channels_per_led = this->channels_;
  uint16_t max_leds = length / channels_per_led;

  ESP_LOGV(TAG, "Processing %d LEDs with %d channels per LED", num_leds, channels_per_led);

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

void E131LightEffect::process_single_light(const uint8_t *data, uint16_t length) {
  if (length < this->start_address_) {
    ESP_LOGV(TAG, "DMX data too short for start address %d", this->start_address_);
    return;
  }

  uint8_t value = data[this->start_address_ - 1];
  this->light_->current_values.set_brightness(value / 255.0f);
  this->light_->start_transition();
}

}  // namespace e131_light
}  // namespace esphome 