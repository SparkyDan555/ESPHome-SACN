# ESPHome sACN (E1.31) Component

This component adds support for the sACN (E1.31) protocol to ESPHome, allowing any ESPHome light entity to be controlled via sACN. It supports both addressable and non-addressable lights, with configurable universe, start channel, and channel types.

## Features

- Support for both addressable and non-addressable lights
- Multiple channel types: MONO (1 channel), RGB (3 channels), RGBW (4 channels), RGBWW (5 channels)
- Configurable universe (1-63999)
- Configurable start channel (1-512)
- Unicast and multicast transport modes
- Configurable timeout with fallback to Home Assistant state
- Blank on start option (similar to WLED)
- Clean state transitions between sACN and Home Assistant control
- Efficient logging with verbose DMX value logging option
- Monochromatic (single-channel) lights are fully supported

## Installation

You can install this component either by copying the files locally or by using ESPHome's external components feature with Git.

### Method 1: Git Installation (Recommended)

Add the following to your ESPHome configuration:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/SparkyDan555/ESPHome-SACN
    components: [ sacn ]
    refresh: always

# Enable sACN component
sacn:
```

### Method 2: Local Installation

1. Copy the `components/sacn` directory to your ESPHome `config/components` directory
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

#### Monochromatic (Single-Channel) Light Example

```yaml
light:
  - platform: monochromatic
    name: "Mono Light"
    id: mono_light
    output: output_1
    effects:
      - sacn:
          universe: 1
          start_channel: 1
          channel_type: MONO  # Use 1 DMX channel for brightness only
          transport_mode: unicast
          timeout: 2500ms
          blank_on_start: true
```

#### RGB Light Example

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
          blank_on_start: true
```

#### RGBW Light Example

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
          blank_on_start: true
```

#### Addressable LED Strip Example

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
          blank_on_start: true
```

### Configuration Variables

#### Light Effect Options

- **universe** (*Optional*, int): The DMX universe to listen on. Range: 1-63999. Default: `1`
- **start_channel** (*Optional*, int): The starting DMX channel. Range: 1-512. Default: `1`
- **channel_type** (*Optional*, string): The type of light channels. One of:
  - `MONO`: Single channel (intensity)
  - `RGB`: Three channels (red, green, blue)
  - `RGBW`: Four channels (red, green, blue, white)
  - `RGBWW`: Five channels (red, green, blue, cold white, warm white)
  Default: `RGB`
- **transport_mode** (*Optional*, string): The network transport mode. One of:
  - `unicast`: Direct unicast communication
  - `multicast`: Multicast communication
  Default: `unicast`
- **timeout** (*Optional*, time): Time to wait without sACN data before reverting to Home Assistant control. Default: `2500ms`
- **blank_on_start** (*Optional*, bool): Whether to blank the light when the effect starts. Default: `false`

## Channel Types

- `MONO`: Uses 1 DMX channel for brightness (monochromatic lights)
- `RGB`: Uses 3 DMX channels for red, green, and blue
- `RGBW`: Uses 4 DMX channels for red, green, blue, and white
- `RGBWW`: Uses 5 DMX channels for red, green, blue, cold white, and warm white (if supported)

## State Behavior

The component implements the following state behavior:

1. **Initial State**: When configured with `blank_on_start: true`, the light will appear as white/full brightness in Home Assistant but the physical output will be blank.

2. **Effect Start**: 
   - With `blank_on_start: true`: Light blanks when effect starts
   - With `blank_on_start: false`: Light maintains its current state

3. **During Effect**:
   - Light responds to incoming sACN data
   - DMX values are logged at verbose level
   - Single timeout notification when stream stops

4. **Effect Stop/Timeout**:
   - Light blanks on timeout
   - Clean return to Home Assistant control
   - No recursive state updates

## Example DMX Channel Mapping

- **MONO**: Channel 1 → Brightness
- **RGB**: Channel 1 → Red, Channel 2 → Green, Channel 3 → Blue
- **RGBW**: Channel 1 → Red, Channel 2 → Green, Channel 3 → Blue, Channel 4 → White
- **RGBWW**: Channel 1 → Red, Channel 2 → Green, Channel 3 → Blue, Channel 4 → Cold White, Channel 5 → Warm White

## Notes
- For monochromatic lights, only a single DMX channel is required and supported.
- The effect will blank the light on start if `blank_on_start: true` is set.
- Timeout and fallback to Home Assistant state are supported.

## Compatibility

This component has been tested with:
- ESP32 (all variants)
- ESP8266
- LibreTiny boards

## Troubleshooting

1. Enable verbose logging to see detailed sACN packet and DMX value information:
```yaml
logger:
  level: VERBOSE
  logs:
    sacn: VERBOSE
```

2. Common issues:
   - No data received: Check universe and network settings
   - Wrong colors: Verify channel type matches your configuration
   - Flickering: Check network stability and timeout settings
   - Light not blanking: Verify `blank_on_start` is set to true
   - Multiple timeout messages: Update to latest version

## Known Limitations

### Color Interlock Incompatibility

This component is not compatible with the `color_interlock: true` option in light configurations. The color interlock feature enforces mutually exclusive RGB and COLD_WARM_WHITE color modes, where:
- When RGB mode is active, white LEDs are turned off
- When COLD_WARM_WHITE mode is active, RGB LEDs are turned off

Since the sACN component uses direct channel-to-LED mapping and needs to control all channels independently, it cannot work with this restriction. If you need sACN control, ensure `color_interlock` is set to `false` in your light configuration.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request. For major changes, please open an issue first to discuss what you would like to change.

## License

This component is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.