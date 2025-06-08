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
  // Minimum sACN packet size (126 bytes for root layer + framing layer + DMP layer + start code)
  static const uint16_t MIN_PACKET_SIZE = 126;

  if (size < MIN_PACKET_SIZE) {
    ESP_LOGD(TAG, "Packet too small: %d bytes (min: %d)", size, MIN_PACKET_SIZE);
    return false;
  }

  // Root Layer validation
  if (payload[0] != 0x00 || payload[1] != 0x10) {  // Preamble
    ESP_LOGD(TAG, "Invalid preamble: %02X %02X (expected: 00 10)", payload[0], payload[1]);
    return false;
  }

  if (payload[2] != 0x00 || payload[3] != 0x00) {  // Postamble
    ESP_LOGD(TAG, "Invalid postamble: %02X %02X (expected: 00 00)", payload[2], payload[3]);
    return false;
  }

  // Validate ACN packet identifier
  if (memcmp(payload + 4, SACN_PACKET_IDENTIFIER, sizeof(SACN_PACKET_IDENTIFIER)) != 0) {
    ESP_LOGD(TAG, "Invalid ACN packet identifier");
    return false;
  }

  // Framing Layer validation
  uint16_t framing_length = (payload[38] << 8) + payload[39] - 0x7000 + 38;
  if (size < framing_length) {
    ESP_LOGD(TAG, "Invalid framing layer length: %d", framing_length);
    return false;
  }

  // DMP Layer validation
  uint16_t dmp_length = (payload[115] << 8) + payload[116] - 0x7000 + 115;
  if (size < dmp_length) {
    ESP_LOGD(TAG, "Invalid DMP layer length: %d", dmp_length);
    return false;
  }

  // Validate DMP vector - accept both 0x02 and 0x04
  uint8_t dmp_vector = payload[117];  // VECTOR_DMP_SET_PROPERTY_ADDR
  if (dmp_vector != 0x02 && dmp_vector != 0x04) {
    ESP_LOGD(TAG, "Invalid DMP vector: 0x%02X (expected: 0x02 or 0x04)", dmp_vector);
    ESP_LOGV(TAG, "Packet bytes around DMP vector: %02X %02X %02X %02X %02X",
             payload[115], payload[116], payload[117], payload[118], payload[119]);
    return false;
  }

  // Validate DMX start code - accept both 0x00 and 0x04
  uint8_t start_code = payload[125];  // STARTCODE_ADDR
  if (start_code != 0x00 && start_code != 0x04) {
    ESP_LOGW(TAG, "Invalid DMX start code: 0x%02X (expected: 0x00 or 0x04)", start_code);
    ESP_LOGV(TAG, "Packet bytes around start code: %02X %02X %02X %02X %02X",
             payload[123], payload[124], payload[125], payload[126], payload[127]);
    return false;
  }

  // Log full packet structure at verbose level
  ESP_LOGV(TAG, "Valid sACN packet structure:");
  ESP_LOGV(TAG, "  Root Layer:");
  ESP_LOGV(TAG, "    Preamble: 0x%02X%02X", payload[0], payload[1]);
  ESP_LOGV(TAG, "    Postamble: 0x%02X%02X", payload[2], payload[3]);
  ESP_LOGV(TAG, "    ACN Packet ID: %02X %02X %02X %02X...", payload[4], payload[5], payload[6], payload[7]);
  ESP_LOGV(TAG, "  Framing Layer:");
  ESP_LOGV(TAG, "    Length: %d", framing_length);
  ESP_LOGV(TAG, "    Source Name: %.64s", &payload[44]);
  ESP_LOGV(TAG, "    Priority: %d", payload[108]);
  ESP_LOGV(TAG, "    Sequence: %d", payload[111]);
  ESP_LOGV(TAG, "    Universe: %d", (payload[113] << 8) | payload[114]);
  ESP_LOGV(TAG, "  DMP Layer:");
  ESP_LOGV(TAG, "    Length: %d", dmp_length);
  ESP_LOGV(TAG, "    Vector: 0x%02X", dmp_vector);
  ESP_LOGV(TAG, "    Start Code: 0x%02X", start_code);
  ESP_LOGV(TAG, "    First DMX Values: %02X %02X %02X %02X",
           payload[126], payload[127], payload[128], payload[129]);

  return true;
}

uint16_t SACNComponent::get_universe_(const uint8_t *payload) {
  // Universe number is at offset 113 (0x71)
  return (payload[113] << 8) | payload[114];
}

uint16_t SACNComponent::get_start_address_(const uint8_t *payload) {
  // First property address is at offset 119 (0x77)
  // DMX addresses are 1-based, so add 1 to the result
  uint16_t addr = (payload[119] << 8) | payload[120];
  return addr + 1;
}

uint16_t SACNComponent::get_property_value_count_(const uint8_t *payload) {
  // Property value count is at offset 123 (0x7B)
  // Should be 513 for a full universe (512 DMX channels + 1 start code)
  return (payload[123] << 8) | payload[124];
}

bool SACNComponent::process_(const uint8_t *payload, uint16_t size) {
  uint16_t universe = this->get_universe_(payload);
  uint16_t start_address = this->get_start_address_(payload);
  uint16_t property_value_count = this->get_property_value_count_(payload);

  ESP_LOGV(TAG, "Processing sACN packet - Universe: %d, Start Address: %d, Values: %d",
           universe, start_address, property_value_count);

  // DMX data starts at offset 126 (0x7E)
  const uint16_t DMX_START_OFFSET = 126;  // Fixed offset to DMX data start
  
  // Validate property value count (should be at least 1 for start code + data)
  if (property_value_count < 1) {
    ESP_LOGW(TAG, "Invalid property value count: %d (must be at least 1)", property_value_count);
    return false;
  }

  // Validate we have enough data
  if (size < DMX_START_OFFSET + 3) {
    ESP_LOGW(TAG, "Packet too small for DMX data: %d (need at least %d)", size, DMX_START_OFFSET + 3);
    return false;
  }

  // Debug log more packet details
  ESP_LOGV(TAG, "Packet details:");
  ESP_LOGV(TAG, "  Size: %d", size);
  ESP_LOGV(TAG, "  DMX Start Offset: %d", DMX_START_OFFSET);
  ESP_LOGV(TAG, "  First 10 bytes from DMX start:");
  for (int i = 0; i < 10 && (DMX_START_OFFSET + i) < size; i++) {
    ESP_LOGV(TAG, "    [%d]: 0x%02X", i, payload[DMX_START_OFFSET + i]);
  }

  // Debug log the first few bytes of DMX data
  ESP_LOGV(TAG, "DMX Data [1-3]: %02X %02X %02X", 
           payload[DMX_START_OFFSET],     // First channel
           payload[DMX_START_OFFSET + 1], // Second channel
           payload[DMX_START_OFFSET + 2]); // Third channel

  for (auto *light_effect : this->light_effects_) {
    // Calculate the offset for this light effect based on its start channel
    uint16_t effect_offset = DMX_START_OFFSET + (light_effect->start_channel_ - 1);
    
    // Calculate how many channels we need for this effect
    uint16_t channels_needed = light_effect->channel_type_;
    
    // Check if we have enough data for this effect
    if (effect_offset + channels_needed > size) {
      ESP_LOGW(TAG, "Not enough data for effect: need %d channels starting at %d, but packet only has %d bytes", 
               channels_needed, light_effect->start_channel_, size - DMX_START_OFFSET);
      return false;
    }

    ESP_LOGV(TAG, "Processing effect at channel %d (offset %d)", 
             light_effect->start_channel_, effect_offset);

    // Process the DMX data starting at the effect's offset
    uint16_t values_processed = light_effect->process_(payload + effect_offset, 
                                                     size - effect_offset,  // Remaining size
                                                     0);  // Used is always 0 as we pass the exact start
    if (values_processed == 0) {
      ESP_LOGW(TAG, "Failed to process light effect data");
      return false;
    }

    ESP_LOGV(TAG, "Processed %d values for effect", values_processed);
  }

  return true;
}

}  // namespace sacn
}  // namespace esphome

#endif  // USE_ARDUINO 