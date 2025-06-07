# ESPHome sACN (E1.31) Component

This component adds support for the sACN (E1.31) protocol to ESPHome, allowing any ESPHome light entity to be controlled via sACN. It supports both addressable and non-addressable lights, with configurable universe, start channel, and channel types.

## Features

- Support for both addressable and non-addressable lights
- Multiple channel types: MONO (1 channel), RGB (3 channels), RGBW (4 channels)
- Configurable universe (1-63999)
- Configurable start channel (1-512)
- Unicast and multicast transport modes
- 2.5s timeout with fallback to Home Assistant state
- Proper packet validation and error handling

## Installation

1. Copy the `components/sacn` directory to your ESPHome `custom_components` directory
2. Add the following to your ESPHome configuration:

```yaml
external_components:
  - source:
      type: local
      path: components

# Enable sACN component
sacn:
```

## Configuration

### Basic Configuration

Here's a basic example for an RGB light:

```yaml
light:
  - platform: rgb
    name: "RGB Light"
    red: red_output
    green: green_output
    blue: blue_output
    
    effects:
      - sacn:
          universe: 1
          start_channel: 1
          channel_type: RGB
          transport_mode: unicast
          timeout: 2500ms
```

### Configuration Variables

#### Light Effect Options

- **universe** (*Optional*, int): The DMX universe to listen on. Range: 1-63999. Default: `1`
- **start_channel** (*Optional*, int): The starting DMX channel. Range: 1-512. Default: `1`
- **channel_type** (*Optional*, string): The type of light channels. One of:
  - `MONO`: Single channel (intensity)
  - `RGB`: Three channels (red, green, blue)
  - `RGBW`: Four channels (red, green, blue, white)
  Default: `RGB`
- **transport_mode** (*Optional*, string): The network transport mode. One of:
  - `unicast`: Direct unicast communication
  - `multicast`: Multicast communication
  Default: `unicast`
- **timeout** (*Optional*, time): Time to wait without sACN data before reverting to Home Assistant control. Default: `2500ms`

## Example Configurations

### RGB Light

```yaml
output:
  - platform: ledc
    pin: GPIO4
    id: red_output
    frequency: 1000Hz
    
  - platform: ledc
    pin: GPIO12
    id: green_output
    frequency: 1000Hz
    
  - platform: ledc
    pin: GPIO14
    id: blue_output
    frequency: 1000Hz

light:
  - platform: rgb
    name: "RGB Light"
    id: main_light
    
    red: red_output
    green: green_output
    blue: blue_output
    
    effects:
      - sacn:
          universe: 1
          start_channel: 1
          channel_type: RGB
          transport_mode: unicast
          timeout: 2500ms
```

### Addressable LED Strip

```yaml
light:
  - platform: neopixelbus
    type: GRB
    pin: GPIO2
    num_leds: 60
    name: "LED Strip"
    
    effects:
      - addressable_sacn:
          universe: 1
          start_channel: 1
          channel_type: RGB
          transport_mode: unicast
          timeout: 2500ms
```

### RGBW Light

```yaml
light:
  - platform: rgbw
    name: "RGBW Light"
    red: red_output
    green: green_output
    blue: blue_output
    white: white_output
    
    effects:
      - sacn:
          universe: 1
          start_channel: 1
          channel_type: RGBW
          transport_mode: unicast
          timeout: 2500ms
```

## Compatibility

This component has been tested with:
- ESP32 (all variants)
- ESP8266
- LibreTiny boards

## Troubleshooting

1. Enable debug logging to see detailed sACN packet information:
```yaml
logger:
  level: DEBUG
  logs:
    sacn: DEBUG
```

2. Common issues:
   - No data received: Check universe and network settings
   - Wrong colors: Verify channel type matches your configuration
   - Flickering: Check network stability and timeout settings

## License

This component is licensed under the MIT License. 