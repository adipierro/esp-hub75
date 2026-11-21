"""HUB12 configuration for the existing HUB75 display implementation."""

from pathlib import Path

from esphome import automation, pins
import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components import display
from esphome.components.esp32 import add_idf_component, get_esp32_variant
from esphome.components.esp32.const import VARIANT_ESP32
from esphome.components.hub75.display import (
    CLOCK_SPEEDS,
    HUB75Display,
    ROTATIONS,
    _calculate_min_refresh_rate,
    _final_validate,
)
from esphome.const import (
    CONF_BIT_DEPTH,
    CONF_BRIGHTNESS,
    CONF_CLK_PIN,
    CONF_DISPLAY,
    CONF_GAMMA_CORRECT,
    CONF_ID,
    CONF_OE_PIN,
    CONF_PLATFORM,
    CONF_ROTATION,
)
from esphome.core import CORE

DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["hub75"]

CONF_LIBRARY_PATH = "library_path"
CONF_DATA_PIN = "data_pin"
CONF_A_PIN = "a_pin"
CONF_B_PIN = "b_pin"
CONF_LATCH_PINS = "latch_pins"
CONF_LAYOUT_ROWS = "layout_rows"
CONF_LAYOUT_COLS = "layout_cols"
CONF_DOUBLE_BUFFER = "double_buffer"
CONF_CLOCK_SPEED = "clock_speed"
CONF_CLOCK_PHASE = "clock_phase"
CONF_LATCH_BLANKING = "latch_blanking"
CONF_MIN_REFRESH_RATE = "min_refresh_rate"
CONF_INVERT_DATA = "invert_data"
CONF_INVERT_OE = "invert_oe"
CONF_INVERT_LATCH = "invert_latch"

Hub75Config = cg.global_ns.struct("Hub75Config")
Hub75Pins = cg.global_ns.struct("Hub75Pins")
Hub12Config = cg.global_ns.struct("Hub12Config")


def _library_path(config):
    if CONF_LIBRARY_PATH in config:
        return Path(CORE.relative_config_path(config[CONF_LIBRARY_PATH])).resolve()
    # ESPHome loads this file directly from the external component checkout.
    # Use that same checkout for the IDF component, keeping both revisions
    # together for local sources and sources fetched from Git.
    return Path(__file__).resolve().parents[3] / "components" / "hub75"


def _validate_hub12(config):
    rows = config[CONF_LAYOUT_ROWS]
    columns = config[CONF_LAYOUT_COLS]
    if len(config[CONF_LATCH_PINS]) != rows:
        raise cv.Invalid("latch_pins must contain one GPIO per panel row", path=[CONF_LATCH_PINS])
    if (128 * columns + 2) * (rows + 1) > 65535:
        raise cv.Invalid("panel layout exceeds the HUB12 DMA buffer limit")
    if config[CONF_LATCH_BLANKING] + 1 >= (128 * columns + 2) // 2:
        raise cv.Invalid("latch_blanking leaves no display time", path=[CONF_LATCH_BLANKING])
    used = [config[key] for key in (CONF_DATA_PIN, CONF_A_PIN, CONF_B_PIN, CONF_OE_PIN, CONF_CLK_PIN)]
    used.extend(config[CONF_LATCH_PINS])
    if len(used) != len(set(used)):
        raise cv.Invalid("HUB12 GPIOs must be distinct")
    path = _library_path(config)
    if not (path / "CMakeLists.txt").is_file() or not (path / "include" / "hub75.h").is_file():
        raise cv.Invalid("library_path must point to this checkout's components/hub75 directory", path=[CONF_LIBRARY_PATH])
    types = path / "include" / "hub75_types.h"
    if not types.is_file() or "struct Hub12Config" not in types.read_text(encoding="utf-8"):
        raise cv.Invalid("library_path does not contain the HUB12-enabled library", path=[CONF_LIBRARY_PATH])
    return config


def _final_validate_hub12(config):
    if get_esp32_variant() != VARIANT_ESP32:
        raise cv.Invalid("HUB12 currently requires an original ESP32")
    displays = fv.full_config.get().get(CONF_DISPLAY, [])
    if sum(item.get(CONF_PLATFORM) in ("hub12", "hub75") for item in displays) > 1:
        raise cv.Invalid("only one HUB12 or HUB75 display may use the I2S DMA engine")
    _final_validate(config)


FINAL_VALIDATE_SCHEMA = cv.Schema(_final_validate_hub12)

CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(HUB75Display),
            cv.Optional(CONF_LIBRARY_PATH): cv.string_strict,
            cv.Required(CONF_DATA_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_A_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_B_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_OE_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_CLK_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_LATCH_PINS): cv.All(
                cv.ensure_list(pins.internal_gpio_output_pin_number), cv.Length(min=1, max=2)
            ),
            cv.Optional(CONF_LAYOUT_ROWS, default=1): cv.int_range(min=1, max=2),
            cv.Optional(CONF_LAYOUT_COLS, default=1): cv.int_range(min=1, max=65535),
            cv.Optional(CONF_INVERT_DATA, default=False): cv.boolean,
            cv.Optional(CONF_INVERT_OE, default=False): cv.boolean,
            cv.Optional(CONF_INVERT_LATCH, default=False): cv.boolean,
            cv.Optional(CONF_DOUBLE_BUFFER, default=False): cv.boolean,
            cv.Optional(CONF_BRIGHTNESS, default=128): cv.uint8_t,
            cv.Optional(CONF_BIT_DEPTH, default=8): cv.int_range(min=4, max=12),
            cv.Optional(CONF_GAMMA_CORRECT, default="CIE1931"): cv.one_of("LINEAR", "CIE1931", "GAMMA_2_2", upper=True),
            cv.Optional(CONF_CLOCK_SPEED, default="8MHZ"): cv.enum(CLOCK_SPEEDS, upper=True),
            cv.Optional(CONF_CLOCK_PHASE, default=False): cv.boolean,
            cv.Optional(CONF_LATCH_BLANKING, default=1): cv.int_range(min=0, max=255),
            cv.Optional(CONF_MIN_REFRESH_RATE): cv.int_range(min=40, max=200),
            cv.Optional(CONF_ROTATION): cv.enum(ROTATIONS, int=True),
        }
    ),
    _validate_hub12,
)


async def to_code(config):
    path = str(_library_path(config))
    # A local IDF path is registered under its directory name, not the
    # registry's namespaced esphome__esp-hub75 CMake target.
    add_idf_component(name="hub75", path=path)
    cg.add_build_flag(f"-DHUB75_BIT_DEPTH={config[CONF_BIT_DEPTH]}")
    gamma = {"LINEAR": 0, "CIE1931": 1, "GAMMA_2_2": 2}
    cg.add_build_flag(f"-DHUB75_GAMMA_MODE={gamma[config[CONF_GAMMA_CORRECT]]}")

    gpio = cg.StructInitializer(
        Hub75Pins,
        ("a", config[CONF_A_PIN]),
        ("b", config[CONF_B_PIN]),
        ("oe", config[CONF_OE_PIN]),
        ("clk", config[CONF_CLK_PIN]),
    )
    latches = config[CONF_LATCH_PINS] + [-1] * (2 - len(config[CONF_LATCH_PINS]))
    hub12 = cg.StructInitializer(
        Hub12Config,
        ("enabled", True),
        ("data", config[CONF_DATA_PIN]),
        ("latch", cg.ArrayInitializer(*latches)),
        ("invert_data", config[CONF_INVERT_DATA]),
        ("invert_latch", config[CONF_INVERT_LATCH]),
        ("invert_oe", config[CONF_INVERT_OE]),
    )
    fields = [
        ("panel_width", 32),
        ("panel_height", 16),
        ("layout_rows", config[CONF_LAYOUT_ROWS]),
        ("layout_cols", config[CONF_LAYOUT_COLS]),
    ]
    if CONF_ROTATION in config:
        fields.append(("rotation", config[CONF_ROTATION]))
    fields.extend(
        [
            ("pins", gpio),
            ("output_clock_speed", config[CONF_CLOCK_SPEED]),
            ("min_refresh_rate", _calculate_min_refresh_rate(config)),
            ("latch_blanking", config[CONF_LATCH_BLANKING]),
            ("double_buffer", config[CONF_DOUBLE_BUFFER]),
            ("clk_phase_inverted", config[CONF_CLOCK_PHASE]),
            ("brightness", config[CONF_BRIGHTNESS]),
            ("hub12", hub12),
        ]
    )
    config.pop(CONF_ROTATION, None)
    var = cg.new_Pvariable(config[CONF_ID], cg.StructInitializer(Hub75Config, *fields))
    await display.register_display(var, config)
    await display.add_writer(var, config)


automation.register_apply_action(
    "hub12.set_brightness",
    cv.maybe_simple_value(
        {cv.GenerateID(): cv.use_id(HUB75Display), cv.Required(CONF_BRIGHTNESS): cv.templatable(cv.uint8_t)},
        key=CONF_BRIGHTNESS,
    ),
    automation.ApplyField(CONF_BRIGHTNESS, "set_brightness", cg.uint8),
)
