# HUB12 monochrome panels (experimental)

For an ESPHome external component and test YAML, see [ESPHome HUB12](../esphome/README.md).

The original ESP32 I2S backend supports 32×16 monochrome P10 modules with
1/4-scan binary row addressing. This support has not yet been tested on physical
panels. Other ESP32 targets reject HUB12 mode. Existing HUB75 configurations are
unchanged.

Connect one or two horizontal rows of panels. Within each row, daisy-chain the
data output into the next panel's data input. Connect the same data, CLK, A, B,
and OE signals to the first panel of each row, and use a separate latch signal
for each row. The row at the top of the image uses latch 0. Use a common ground,
an appropriate panel supply, and suitable level shifting as required by the
panel. Verify the connector markings and GPIO availability on your own board.

```cpp
Hub75Config config{};
config.panel_width = 32;
config.panel_height = 16;
config.layout_cols = 6;
config.layout_rows = 2;
config.layout = Hub75PanelLayout::HORIZONTAL;
config.hub12.enabled = true;
config.hub12.data = 23;       // Example GPIO, not a universal board pinout
config.hub12.latch[0] = 32;
config.hub12.latch[1] = 33;
config.pins.a = 22;
config.pins.b = 21;
config.pins.oe = 25;
config.pins.clk = 18;
config.hub12.invert_data = true;
config.hub12.invert_oe = true;
config.hub12.invert_latch = false;
config.double_buffer = true;
Hub75Driver display(config);
if (!display.begin()) {
  // Configuration or DMA memory allocation failed.
}
```

This polarity and six-by-two geometry match the referenced Hacker Embassy
application; its clock and data GPIOs above are examples. Different panels can
require different polarity, clock speed, or pixel mapping. RGB drawing uses the
red component as monochrome intensity. Rotation, brightness, and bit depth use
the existing driver settings. `layout_rows` must be 1 or 2, and `layout` must be
`HORIZONTAL`; other geometries and driver chips are rejected.

HUB12 refresh is automatically paused when the visible buffer contains no lit
pixels after gamma/bit-depth conversion, or when brightness or intensity makes
the effective brightness zero. OE is forced inactive, DMA and its clock are
stopped, black data is shifted and latched into every panel row, and control
signals are held idle. Drawing a visible pixel or restoring brightness resumes
refresh from a complete frame. In double-buffer mode, drawing or clearing the
back buffer changes the visible state only when `flip_buffer()` is called.
The driver remains initialized while paused; `is_running()` reports that the
driver is initialized, not whether DMA is currently refreshing. This saves LED
and refresh activity but does not disconnect panel power or eliminate standby
current. Drawing, brightness, and buffer operations must be serialized by the
caller, as with the existing DMA buffer API. HUB12 operations can synchronously
stop or restart I2S and pulse the GPIOs when the visible state changes; call
them from task context, not from an ISR or with the flash cache disabled.

The I2S DMA waveform loads each row while OE is blanked, then displays both
rows together. Each payload has a leading clock, with the latch pulse on its
last word and a trailing clock before the next payload. The alignment matches
a reported panel pattern but requires physical timing and setup/hold validation.
The next payload overwrites the bits shifted between latch pulses. Longer
chains increase DMA memory and reduce refresh rate;
double buffering allocates a second set of DMA buffers and descriptors. The
driver logs its calculated refresh and allocation sizes, and initialization can
fail if contiguous DMA-capable internal RAM is unavailable. Actual refresh,
grayscale quality, LAT/CLK timing, ghosting and maximum practical dimensions
still require measurement on hardware.

The inherited I2S `flip_buffer()` redirects the DMA chain and returns before
the next frame boundary. Drawing into the newly selected back buffer immediately
can therefore overlap the end of the old frame. When testing, avoid immediately
redrawing the old front buffer; measure the transition and synchronize its reuse.
A confirmed DMA completion signal is needed for a general guarantee.

For a six-by-two display at eight-bit depth, the two row-buffer allocations
alone need about 289 KiB with double buffering, before descriptor arrays and
the rest of the application. Start with one panel and a single buffer if memory
is tight; then add the second row, the full chain and double buffering.

For the first hardware test, start at 8 MHz with low brightness. Use single
pixels, row and column stripes, checkerboards, and a grayscale ramp. Check both
panel rows and the boundaries between panels. With a logic analyzer, confirm
that OE is inactive during address changes and both latch pulses, and that LAT
captures the final data clock with adequate setup and hold time for the
panel. Exercise rapid buffer swaps and application load, and record measured
refresh, free DMA memory and any flicker or ghosting. The software model cannot
establish the actual GPIO/clock phase relationship on a physical board.

Host checks can be run by compiling `tests/hub12_mapping_test.cpp` and
`tests/hub12_refresh_test.cpp` with a C++17 compiler.
