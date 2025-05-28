# ESPHome E1.31 Light Component

A custom ESPHome component that provides E1.31 (sACN) lighting control for both addressable and single lights. It's designed to be a more reliable alternative to the built-in E1.31 component.

## Features

* Support for both addressable and single lights
* Multiple channel configurations:
  - RGBW (4 channels)
  - RGB (3 channels)
  - Single channel (dimmer)
* Configurable universe and start address
* Multicast and unicast support
* Compatible with popular E1.31 controllers (xLights, QLC+, etc.)
* Efficient packet handling and processing
* Smart channel mapping and validation

## Installation

### Method 1: Local Components

1. Create a `custom_components` directory in your ESPHome configuration directory if it doesn't already exist.
2. Copy the entire `e131_light` directory into the `custom_components` directory.
3. Add the following to your YAML configuration:

```yaml
external_components:
  - source: 
      type: local
      path: custom_components
```

### Method 2: Direct Git Reference

Add this to your YAML configuration:

```yaml
external_components:
  - source: 
      type: git
      url: https://github.com/yourusername/ESPHome-SACN
      ref: main
```

## Hardware Setup

The E1.31 component works over Ethernet or WiFi, so you'll need either:
* An ESP32 with built-in Ethernet
* An ESP8266/ESP32 with WiFi
* An ESP8266/ESP32 with an Ethernet shield

## YAML Configuration

```yaml
# Configure the e131_light component
e131_light:
  id: e131_light_device
  method: multicast  # or unicast
  port: 5568  # optional, defaults to 5568

# Configure lights
light:
  - platform: neopixelbus  # or any other light platform
    id: my_light
    num_leds: 100
    effects:
      - e131_light:
          e131_light_id: e131_light_device
          universe: 1
          start_address: 1  # optional, defaults to 1
          channels: RGBW  # or RGB or MONO
```

### Configuration Variables

#### Component Configuration
- **id** (Required): The ID of the E1.31 component
- **method** (Optional): The E1.31 listening method. Can be either `multicast` or `unicast`. Defaults to `multicast`
- **port** (Optional): The UDP port to listen on. Defaults to 5568

#### Effect Configuration
- **e131_light_id** (Required): The ID of the E1.31 component to use
- **universe** (Required): The E1.31 universe number (1-512)
- **start_address** (Optional): The starting DMX address (1-512). Defaults to 1
- **channels** (Optional): The channel configuration. Can be:
  - `RGBW`: 4 channels per LED (RGB + White)
  - `RGB`: 3 channels per LED
  - `MONO`: 1 channel per LED (dimmer)
  Defaults to `RGB`

## Example Configurations

### RGBW Strip
```yaml
light:
  - platform: neopixelbus
    num_leds: 100
    effects:
      - e131_light:
          universe: 1
          channels: RGBW
```

### RGB Strip
```yaml
light:
  - platform: neopixelbus
    num_leds: 150
    effects:
      - e131_light:
          universe: 1
          channels: RGB
```

### Single Channel Light
```yaml
light:
  - platform: monochromatic
    effects:
      - e131_light:
          universe: 1
          channels: MONO
```

## Protocol Information

This component implements the E1.31 (sACN) protocol for DMX over Ethernet:

* Uses UDP for packet transmission
* Supports both multicast (239.255.x.x) and unicast addressing
* Implements priority handling for multiple sources
* Validates packet sequence numbers for reliability
* Handles DMX channel data according to the E1.31 specification

### Key Features

* Efficient packet processing with minimal overhead
* Smart channel mapping for different light types
* Automatic validation of universe and channel ranges
* Support for standard DMX channel configurations

## Troubleshooting

* **No response from lights**: Check your network configuration and ensure multicast is enabled if using multicast mode
* **Incorrect colors**: Verify the channel configuration matches your light setup
* **Connection issues**: Ensure your network allows UDP traffic on port 5568
* **Performance issues**: Consider using unicast mode if multicast is causing network congestion

## License

This component is licensed under the MIT License.

## Acknowledgments

* Based on the E1.31 (sACN) protocol specification
* Developed for integration with the ESPHome ecosystem
* Inspired by various open-source E1.31 implementations 