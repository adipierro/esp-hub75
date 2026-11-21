# ESPHome HUB12 display (experimental)

`components/hub12` is an ESPHome external display platform that reuses
ESPHome's built-in `hub75` display implementation and this checkout's HUB12
DMA driver. It currently targets the original ESP32 and 32×16, 1/4-scan
monochrome P10 panels. ESP32-S2/S3 and other variants are not supported.
Configuration and the sample firmware have been checked with ESPHome 2026.11.0
and ESP-IDF 5.5.2. Panel timing and the latest mapping require further hardware testing.

## Use the test branch from Git

ESPHome fetches both the external component and its matching DMA library from
the same checkout. No manual download or `library_path` is needed:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/adipierro/esp-hub75.git
      ref: codex/hub12-test
      path: esphome/components
    components: [hub12]
    refresh: 0s
```

Add a `display:` entry as in the [example](../examples/esphome/hub12-test.yaml),
omitting `library_path`, and adapt the panel count, pins and polarity to the
physical wiring. `refresh: 0s` checks the branch for updates on each build;
use a commit hash for reproducible builds. The build machine needs Git access
to the repository. This branch is experimental and is not an upstream release.

## Test locally

1. On the machine running ESPHome, keep this repository together; the
   [test YAML](../examples/esphome/hub12-test.yaml) uses relative paths to both
   the external component and the modified library. The paths must exist inside
   the build container if you use ESPHome Device Builder or a container.
2. Check the sample GPIO assignments, power and common ground against your
   wiring. The sample starts with one panel at moderate brightness and 8 MHz; its
   polarity settings match the reference project's hardware, not every HUB12
   panel. For the reference 6×2 array, set `layout_cols: 6`, `layout_rows: 2`
   and `latch_pins: [GPIO32, GPIO33]`, after checking the wiring.
3. Run `esphome config examples/esphome/hub12-test.yaml` to validate and
   `esphome run examples/esphome/hub12-test.yaml` to build and flash via USB.
   The sample is a serial-logger test and does not enable Wi-Fi or OTA.

The sample cycles every five seconds through black, an 8×8 square on black,
and a brighter 4×4 square on a dimmer full panel. Linear color correction keeps
the dim fill distinguishable at the sample's four-bit depth. After 100 ms of
black, the library blanks OE, stops refresh and latches black data; a logic
analyzer and current measurement are needed to confirm actual panel behavior.
`Color` drawing uses the red channel as grayscale intensity. Use
`auto_clear_enabled: false` when drawing incrementally.

## Configuration

- `library_path` is optional. If omitted, the component uses `components/hub75`
  from its own checkout; an explicit local path can override it. A released
  esp-hub75 package does not yet have the required configuration.
- `data_pin`, `clk_pin`, `a_pin`, `b_pin`, `oe_pin`, and `latch_pins` are
  required internal output GPIO numbers. Provide one latch per panel row.
  Configure electrical signal inversion with `invert_data`, `invert_oe`, and
  `invert_latch`, rather than with inverted GPIO schemas.
- `layout_cols` defaults to 1; `layout_rows` is 1 or 2 and defaults to 1.
  Rows use shared data, clock, addresses and OE, with independent latches.
  Only one HUB12 or HUB75 display can use the I2S DMA engine at a time.
- `brightness`, `bit_depth`, `gamma_correct`, `double_buffer`, `clock_speed`,
  `clock_phase`, `latch_blanking`, `min_refresh_rate`, and standard display
  options such as `lambda`, `pages`, `rotation`, and `update_interval` are
  supported. `clock_speed` defaults to `8MHZ`, `bit_depth` to 8, and
  `double_buffer` to false.
- HUB12 `bit_depth` supports 1–12. At 1 bit pixels are black or lit; at 2 or 3 bits grayscale is coarse;
  an aggressive minimum refresh rate can reduce the number of distinct levels.
  With `gamma_correct: LINEAR` at 1 bit, use full red (255) or `COLOR_ON` for
  lit pixels and adjust the overall `brightness` separately.
- `idle_timeout` sets how long the visible buffer must remain black before DMA
  stops. It defaults to `100ms`; `0ms` stops immediately. Brightness zero always
  blanks immediately.
- Use `hub12.set_brightness` with a display `id` and a brightness in the range
  0–255 to change brightness from an ESPHome automation.

For example, these scripts blank and restore a display named `matrix`:

```yaml
script:
  - id: panel_off
    then:
      - hub12.turn_off: matrix
  - id: panel_on
    then:
      - hub12.turn_on:
          id: matrix
          brightness: 32
```

`hub12.turn_on` requires the desired brightness (1–255); it does not remember
the previous value. Both actions use the existing brightness control. The
display's update loop is disabled while its brightness is zero, and resumes
when it is turned on.

The underlying ESPHome display is the built-in HUB75 implementation, so setup
logs identify it as `HUB75` and may list unused RGB pins as `-1`.

The full 6×2 array can require more internal DMA memory than is available with
double buffering. This code and its off/on transitions are unverified on panel
hardware. See [HUB12 details](../docs/HUB12.md) for timing, memory, caller
serialization, and the existing double-buffer handoff limitation.
