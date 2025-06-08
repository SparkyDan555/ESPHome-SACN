# sACN Component for ESPHome

This component implements sACN (Streaming ACN / E1.31) support for ESPHome, allowing you to control lights using the sACN protocol.

## Configuration

```yaml
# Example configuration entry
sacn:
  universe: 1  # sACN universe to listen on
  start_channel: 1  # First DMX channel to use (1-512)
  channel_count: 4  # Number of DMX channels to use

light:
  - platform: monochromatic
    # ... other light configuration ...
    effects:
      - sacn:
          channel_type: MONO  # Use 1 DMX channel
          
  - platform: rgb
    # ... other light configuration ...
    effects:
      - sacn:
          channel_type: RGB  # Use 3 DMX channels
          
  - platform: rgbw
    # ... other light configuration ...
    effects:
      - sacn:
          channel_type: RGBW  # Use 4 DMX channels
```

## Channel Types

The component supports different channel types depending on your light configuration:

- `MONO`: Uses 1 DMX channel for brightness
- `RGB`: Uses 3 DMX channels for red, green, and blue
- `RGBW`: Uses 4 DMX channels for red, green, blue, and white

## Direct Channel Control

The sACN component provides direct 1:1 mapping between DMX channels and LED outputs. Each DMX channel directly controls the brightness of its corresponding LED output:

- Channel 1 → Red LED (or mono brightness)
- Channel 2 → Green LED (RGB/RGBW only)
- Channel 3 → Blue LED (RGB/RGBW only)
- Channel 4 → White LED (RGBW only)

## Known Limitations

### Color Interlock Incompatibility

This component is not compatible with the `color_interlock: true` option in light configurations. The color interlock feature enforces mutually exclusive RGB and COLD_WARM_WHITE color modes, where:
- When RGB mode is active, white LEDs are turned off
- When COLD_WARM_WHITE mode is active, RGB LEDs are turned off

Since the sACN component uses direct channel-to-LED mapping and needs to control all channels independently, it cannot work with this restriction. If you need sACN control, ensure `color_interlock` is set to `false` in your light configuration. 