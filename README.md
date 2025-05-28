# ESPHome E1.31 Light Component

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![ESPHome](https://img.shields.io/badge/ESPHome-2024.5.1-blue)](https://esphome.io/)
[![GitHub stars](https://img.shields.io/github/stars/yourusername/esphome-e131-light?style=social)](https://github.com/yourusername/esphome-e131-light)

This is a custom ESPHome component that provides E1.31 (sACN) lighting control for both addressable and single lights. It's designed to be a more reliable alternative to the built-in E1.31 component.

## Features

- Support for both addressable and single lights
- Multiple channel configurations:
  - RGBW (4 channels)
  - RGB (3 channels)
  - Single channel (dimmer)
- Configurable universe and start address
- Multicast and unicast support
- Compatible with popular E1.31 controllers (xLights, QLC+, etc.)

## Installation

### Method 1: Manual Installation

1. Create a `custom_components` folder in your ESPHome configuration directory if it doesn't exist
2. Copy the `e131_light` folder into the `custom_components` directory
3. Add the component to your ESPHome configuration

### Method 2: Using External Components

Add the following to your ESPHome configuration:

```yaml
external_components:
  - source: github://yourusername/esphome-e131-light
    components: [ e131_light ]
```

## Configuration

```yaml
# Example configuration entry
e131_light:
  method: multicast  # or unicast
  port: 5568  # optional, defaults to 5568

light:
  - platform: neopixelbus  # or any other light platform
    # ... other light configuration ...
    effects:
      - e131_light:
          universe: 1
          start_address: 1  # optional, defaults to 1
          channels: RGBW  # or RGB or MONO
```

### Configuration Variables

- **method** (Optional): The E1.31 listening method. Can be either `multicast` or `unicast`. Defaults to `multicast`.
- **port** (Optional): The UDP port to listen on. Defaults to 5568.

### Effect Configuration Variables

- **universe** (Required): The E1.31 universe number (1-512).
- **start_address** (Optional): The starting DMX address (1-512). Defaults to 1.
- **channels** (Optional): The channel configuration. Can be:
  - `RGBW`: 4 channels per LED (RGB + White)
  - `RGB`: 3 channels per LED
  - `MONO`: 1 channel per LED (dimmer)
  Defaults to `RGB`.

## Limitations

- Maximum of 170 RGB LEDs per universe (510 channels)
- Maximum of 128 RGBW LEDs per universe (512 channels)
- Maximum of 512 single-channel LEDs per universe

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

## Troubleshooting

If you experience issues:
1. Ensure your E1.31 controller is configured to use the correct universe
2. Check that the start address matches your controller's configuration
3. Verify that the channel configuration matches your light setup
4. For multicast issues, ensure your network supports multicast traffic

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- ESPHome team for the amazing framework
- The E1.31 (sACN) protocol specification
- All contributors and users of this component 