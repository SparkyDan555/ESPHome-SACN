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
  0x04, 0x00, 0x00, 0x00
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

    // Debug packet structure
    ESP_LOGV(TAG, "Packet structure analysis (size: %d):", packet_size);
    ESP_LOGV(TAG, "  Root Layer:");
    ESP_LOGV(TAG, "    Preamble Size: 0x%02X%02X", payload[0], payload[1]);
    ESP_LOGV(TAG, "    Post-amble Size: 0x%02X%02X", payload[2], payload[3]);
    ESP_LOGV(TAG, "    ACN Packet ID: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
             payload[4], payload[5], payload[6], payload[7], payload[8], payload[9],
             payload[10], payload[11], payload[12], payload[13], payload[14], payload[15]);
    
    ESP_LOGV(TAG, "  Framing Layer:");
    ESP_LOGV(TAG, "    Source Name: %.64s", &payload[44]);  // Source name is at offset 44
    ESP_LOGV(TAG, "    Universe: %d", payload[0x7A] | (payload[0x7B] << 8));
    ESP_LOGV(TAG, "    Priority: %d", payload[0x70]);  // Priority at offset 0x70
    ESP_LOGV(TAG, "    Sequence Number: %d", payload[0x75]);  // Sequence number at offset 0x75
    
    ESP_LOGV(TAG, "  DMP Layer:");
    ESP_LOGV(TAG, "    Vector: 0x%02X", payload[0x7C]);  // DMP Vector at offset 0x7C
    ESP_LOGV(TAG, "    Start Code: 0x%02X", payload[0x7D]);  // Start code at offset 0x7D
    ESP_LOGV(TAG, "    First DMX Values: %02X %02X %02X %02X",
             payload[0x7E], payload[0x7F], payload[0x80], payload[0x81]);  // DMX data starts at 0x7E

    if (!this->validate_sacn_packet_(&payload[0], payload.size())) {
      continue;  // Validation function now logs specific issues
    }

    if (!this->process_(&payload[0], payload.size())) {
      ESP_LOGW(TAG, "Failed to process sACN packet");
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
  // Minimum sACN packet size (126 bytes for root layer + framing layer + DMP layer)
  static const uint16_t MIN_PACKET_SIZE = 126;

  if (size < MIN_PACKET_SIZE) {
    ESP_LOGD(TAG, "Packet too small: %d bytes (min: %d)", size, MIN_PACKET_SIZE);
    return false;
  }

  // xLights sACN packets start with 0x00 0x10
  if (payload[0] != 0x00 || payload[1] != 0x10) {
    ESP_LOGD(TAG, "Invalid packet start: %02X %02X (expected: 00 10)", payload[0], payload[1]);
    return false;
  }

  // Validate ACN packet identifier
  if (memcmp(payload + 4, SACN_PACKET_IDENTIFIER, sizeof(SACN_PACKET_IDENTIFIER)) != 0) {
    ESP_LOGD(TAG, "Invalid ACN packet identifier");
    return false;
  }

  // Validate DMP vector - accept both 0x02 (standard) and 0x04 (xLights)
  uint8_t dmp_vector = payload[0x7C];
  if (dmp_vector != 0x02 && dmp_vector != 0x04) {
    ESP_LOGD(TAG, "Invalid DMP vector: 0x%02X (expected: 0x02 or 0x04)", dmp_vector);
    ESP_LOGV(TAG, "Packet bytes around DMP vector: %02X %02X %02X %02X %02X",
             payload[0x7A], payload[0x7B], payload[0x7C], payload[0x7D], payload[0x7E]);
    return false;
  }

  // Validate DMX start code - accept both 0x00 (Null Start Code) and 0x04
  uint8_t start_code = payload[0x7D];
  if (start_code != 0x00 && start_code != 0x04) {
    ESP_LOGW(TAG, "Invalid DMX start code: 0x%02X (expected: 0x00 or 0x04)", start_code);
    ESP_LOGV(TAG, "Packet bytes around start code: %02X %02X %02X %02X %02X",
             payload[0x7B], payload[0x7C], payload[0x7D], payload[0x7E], payload[0x7F]);
    return false;
  }

  return true;
}

uint16_t SACNComponent::get_universe_(const uint8_t *payload) {
  // Universe number is little-endian at offset 0x7A
  return payload[0x7A] | (payload[0x7B] << 8);
}

uint16_t SACNComponent::get_start_address_(const uint8_t *payload) {
  // Start code
  return payload[0x9D];
}

uint16_t SACNComponent::get_property_value_count_(const uint8_t *payload) {
  // Property value count is little-endian at offset 0x123
  return payload[0x123] | (payload[0x124] << 8);
}

bool SACNComponent::process_(const uint8_t *payload, uint16_t size) {
  uint16_t universe = this->get_universe_(payload);
  uint16_t start_address = this->get_start_address_(payload);
  uint16_t property_value_count = this->get_property_value_count_(payload);

  ESP_LOGD(TAG, "Processing sACN packet - Universe: %d, Start Address: %d, Values: %d",
           universe, start_address, property_value_count);

  // Accept both 0x00 (Null Start Code) and 0x04 start codes
  uint8_t start_code = payload[0x7D];
  if (start_code != 0x00 && start_code != 0x04) {
    ESP_LOGW(TAG, "Invalid start code: 0x%02X", start_code);
    ESP_LOGV(TAG, "Packet bytes around start code: %02X %02X %02X %02X %02X",
             payload[0x7B], payload[0x7C], payload[0x7D], payload[0x7E], payload[0x7F]);
    return false;
  }

  // DMX data starts at offset 0x7E
  uint16_t data_offset = 0x7E;
  
  // Debug log the first few bytes of DMX data
  if (size >= data_offset + 3) {
    ESP_LOGD(TAG, "DMX Data [1-3]: %02X %02X %02X", 
             payload[data_offset],     // First channel
             payload[data_offset + 1], // Second channel
             payload[data_offset + 2]); // Third channel
  }

  for (auto *light_effect : this->light_effects_) {
    if (data_offset >= size) {
      ESP_LOGW(TAG, "Packet data truncated");
      return false;
    }

    // Process the DMX data
    uint16_t values_processed = light_effect->process_(payload + data_offset, size - data_offset, 0);
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