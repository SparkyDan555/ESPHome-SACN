#ifdef USE_ARDUINO

#include "sacn.h"
#include "sacn_light_effect_base.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sacn {

static const char *const TAG = "sacn";

// E1.31 protocol constants
const uint8_t SACNComponent::SACN_PACKET_IDENTIFIER[12] = {
  0x41, 0x53, 0x43, 0x2d, 0x45, 0x31, 0x2e, 0x31, 0x37, 0x00, 0x00, 0x00
}; // "ASC-E1.17"

const uint8_t SACNComponent::SACN_FRAMING_LAYER_IDENTIFIER[4] = {
  0x00, 0x00, 0x00, 0x02
};

const uint8_t SACNComponent::SACN_DMP_LAYER_IDENTIFIER[4] = {
  0x02, 0x00, 0x00, 0x00
};

SACNComponent::SACNComponent() : receiving_data_(false), last_packet_time_(0) {}
SACNComponent::~SACNComponent() {}

void SACNComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up sACN component...");
}

void SACNComponent::loop() {
  if (!this->udp_) {
    return;
  }

  std::vector<uint8_t> payload;
  uint32_t now = millis();

  while (uint16_t packet_size = this->udp_->parsePacket()) {
    // Log remote endpoint on first packet
    if (!this->receiving_data_) {
      IPAddress remote = this->udp_->remoteIP();
      ESP_LOGI(TAG, "Started receiving sACN data from %d.%d.%d.%d", remote[0], remote[1], remote[2], remote[3]);
      this->receiving_data_ = true;
    }
    
    this->last_packet_time_ = now;
    payload.resize(packet_size);

    if (!this->udp_->read(&payload[0], payload.size())) {
      ESP_LOGW(TAG, "Failed to read UDP packet");
      continue;
    }

    ESP_LOGV(TAG, "Received sACN packet of size %d", packet_size);

    if (!this->validate_sacn_packet_(&payload[0], payload.size())) {
      ESP_LOGV(TAG, "Invalid sACN packet received");
      continue;
    }

    if (!this->process_(&payload[0], payload.size())) {
      ESP_LOGV(TAG, "Failed to process sACN packet");
      continue;
    }
  }

  // Check if we've stopped receiving data (timeout after 5 seconds)
  if (this->receiving_data_ && (now - this->last_packet_time_ > 5000)) {
    ESP_LOGI(TAG, "Stopped receiving sACN data");
    this->receiving_data_ = false;
  }
}

void SACNComponent::add_effect(SACNLightEffectBase *light_effect) {
  if (light_effects_.count(light_effect)) {
    return;
  }

  // Only the first effect added needs to start UDP listening
  if (this->light_effects_.empty()) {
    if (!this->udp_) {
      this->udp_ = make_unique<WiFiUDP>();
    }

    ESP_LOGI(TAG, "Starting UDP listening for sACN on port %d", SACN_PORT);
    if (!this->udp_->begin(SACN_PORT)) {
      ESP_LOGE(TAG, "Cannot bind sACN to port %d", SACN_PORT);
      mark_failed();
      return;
    }
  }

  this->light_effects_.insert(light_effect);
  ESP_LOGD(TAG, "Added sACN effect, total effects: %d", this->light_effects_.size());
}

void SACNComponent::remove_effect(SACNLightEffectBase *light_effect) {
  if (!this->light_effects_.count(light_effect)) {
    return;
  }

  this->light_effects_.erase(light_effect);

  // If no more effects left, stop UDP listening
  if (this->light_effects_.empty() && this->udp_) {
    ESP_LOGI(TAG, "Stopping UDP listening for sACN");
    this->udp_->stop();
  }
}

bool SACNComponent::validate_sacn_packet_(const uint8_t *payload, uint16_t size) {
  // Minimum sACN packet size (Root Layer + Framing Layer + DMP Layer + 1 byte DMX data)
  static const uint16_t MIN_PACKET_SIZE = 126;

  if (size < MIN_PACKET_SIZE) {
    ESP_LOGV(TAG, "Packet too small: %d bytes", size);
    return false;
  }

  // Check preamble size (0x0010)
  if (payload[0] != 0x00 || payload[1] != 0x10) {
    ESP_LOGV(TAG, "Invalid preamble size: %02x%02x", payload[0], payload[1]);
    return false;
  }

  // Check postamble size (0x0000)
  if (payload[2] != 0x00 || payload[3] != 0x00) {
    ESP_LOGV(TAG, "Invalid postamble size: %02x%02x", payload[2], payload[3]);
    return false;
  }

  // Validate ACN packet identifier
  if (payload[4] != 0x41 || payload[5] != 0x53 || payload[6] != 0x43 || payload[7] != 0x2d ||
      payload[8] != 0x45 || payload[9] != 0x31 || payload[10] != 0x2e || payload[11] != 0x31 ||
      payload[12] != 0x37) {
    ESP_LOGV(TAG, "Invalid ACN packet identifier");
    return false;
  }

  // Check root flags and length
  uint16_t root_length = (payload[16] << 8) | payload[17];
  if (root_length > size) {
    ESP_LOGV(TAG, "Invalid root layer length: %d", root_length);
    return false;
  }

  // Check framing flags and length
  uint16_t framing_length = (payload[38] << 8) | payload[39];
  if (framing_length > size - 38) {
    ESP_LOGV(TAG, "Invalid framing layer length: %d", framing_length);
    return false;
  }

  // Check DMP flags and length
  uint16_t dmp_length = (payload[115] << 8) | payload[116];
  if (dmp_length > size - 115) {
    ESP_LOGV(TAG, "Invalid DMP layer length: %d", dmp_length);
    return false;
  }

  return true;
}

uint16_t SACNComponent::get_universe_(const uint8_t *payload) {
  // Universe is at offset 113-114 in the framing layer
  return (payload[113] << 8) | payload[114];
}

uint16_t SACNComponent::get_start_address_(const uint8_t *payload) {
  // Start code is at offset 126
  return payload[126];
}

uint16_t SACNComponent::get_property_value_count_(const uint8_t *payload) {
  // Property value count is at offset 123-124
  return ((payload[123] << 8) | payload[124]) - 1;  // Subtract 1 for start code
}

bool SACNComponent::process_(const uint8_t *payload, uint16_t size) {
  uint16_t universe = this->get_universe_(payload);
  uint16_t start_address = this->get_start_address_(payload);
  uint16_t property_value_count = this->get_property_value_count_(payload);

  ESP_LOGV(TAG, "Processing sACN packet - Universe: %d, Start Address: %d, Values: %d",
           universe, start_address, property_value_count);

  // DMX start code must be 0x00 for dimming data
  if (start_address != 0x00) {
    ESP_LOGV(TAG, "Invalid start code: 0x%02X", start_address);
    return false;
  }

  // Process data for each effect
  uint16_t data_offset = 127;  // Start of DMX data
  for (auto *light_effect : this->light_effects_) {
    if (data_offset >= size) {
      ESP_LOGW(TAG, "Packet data truncated");
      return false;
    }

    uint16_t values_processed = light_effect->process_(payload, size, data_offset);
    if (values_processed == 0) {
      ESP_LOGW(TAG, "Failed to process light effect data");
      return false;
    }

    data_offset += values_processed;
  }

  return true;
}

}  // namespace sacn
}  // namespace esphome

#endif  // USE_ARDUINO 