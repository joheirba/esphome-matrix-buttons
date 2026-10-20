from logging import config

from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import key_provider, script
from esphome.components.const import CONF_ROWS
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_ON_KEY, CONF_ON_STATE, CONF_PIN, CONF_TRIGGER_ID

CODEOWNERS = ["@ssieb"]

AUTO_LOAD = ["key_provider"]

MULTI_CONF = True

matrix_buttons_ns = cg.esphome_ns.namespace("matrix_buttons")
MatrixButtons = matrix_buttons_ns.class_(
    "MatrixButtons", key_provider.KeyProvider, cg.Component
)
MatrixKeyTrigger = matrix_buttons_ns.class_(
    "MatrixKeyTrigger", automation.Trigger.template(cg.uint8)
)
MatrixStateTrigger = matrix_buttons_ns.class_(
    "MatrixStateTrigger", automation.Trigger.template(cg.bool_, cg.int_, cg.int_,cg.int_, cg.int_, cg.int_, cg.uint8)
)

CONF_KEYPAD_ID = "keypad_id"
CONF_COLUMNS = "columns"
CONF_KEYS = "keys"
CONF_DEBOUNCE_TIME = "debounce_time"
CONF_SCAN_DELAY = "scan_delay"
CONF_PASSIVE_ROWS = "passive_rows"  # Set to true if outputs aren't protected (by diodes or equivalent): non-protected outputs will be switched to input mode when not active to prevent damage from back-feeding current
CONF_ACTIVE_HIGH = "active_high"    # Set to true if the rows AND columns are active HIGH or LOW
CONF_MULTI_KEYS = "multi_keys"      # Set to the number of simultaneous key presses to allow (if hardware supports it), otherwise only one key press will be allowed at a time (default: 1)
CONF_SCRIPT_ID = "script_id"        # Script to call on state change

def check_keys(obj):
    if CONF_KEYS in obj and len(obj[CONF_KEYS]) != len(obj[CONF_ROWS]) * len(
        obj[CONF_COLUMNS]
    ):
        raise cv.Invalid("The number of key codes must equal the number of buttons")
    return obj

CONFIG_SCHEMA = cv.All(
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(MatrixButtons),
            cv.Required(CONF_ROWS): cv.All(
                cv.ensure_list({cv.Required(CONF_PIN): pins.gpio_output_pin_schema}),
                cv.Length(min=1),
            ),
            cv.Required(CONF_COLUMNS): cv.All(
                cv.ensure_list({cv.Required(CONF_PIN): pins.gpio_input_pin_schema}),
                cv.Length(min=1),
            ),
            cv.Optional(CONF_KEYS): cv.string,
            cv.Optional(CONF_DEBOUNCE_TIME, default=1): cv.int_range(min=1, max=100),
            cv.Optional(CONF_SCAN_DELAY, default=0): cv.int_range(min=0, max=500),      # delay in µs for rows to stabilize
            cv.Optional(CONF_PASSIVE_ROWS, default=1): cv.boolean,
            cv.Optional(CONF_ACTIVE_HIGH, default=0): cv.boolean,
            cv.Optional(CONF_MULTI_KEYS, default=1): cv.int_,
            cv.Optional(CONF_ON_KEY): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(MatrixKeyTrigger),
                }
            ),
            cv.Optional(CONF_ON_STATE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(MatrixStateTrigger),
                }
            ),
            cv.Optional(CONF_SCRIPT_ID): cv.use_id(script.Script),
        }
    ),
    check_keys,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    row_pins = []
    for conf in config[CONF_ROWS]:
        pin = await cg.gpio_pin_expression(conf[CONF_PIN])
        row_pins.append(pin)
    cg.add(var.set_rows(row_pins))
    col_pins = []
    for conf in config[CONF_COLUMNS]:
        pin = await cg.gpio_pin_expression(conf[CONF_PIN])
        col_pins.append(pin)
    cg.add(var.set_columns(col_pins))
    if CONF_KEYS in config:
        cg.add(var.set_keys(config[CONF_KEYS]))
    cg.add(var.set_debounce_time(config[CONF_DEBOUNCE_TIME]))
    if CONF_PASSIVE_ROWS in config:
        cg.add(var.set_passive_rows(config[CONF_PASSIVE_ROWS]))
    if CONF_ACTIVE_HIGH in config:
        cg.add(var.set_active_high(config[CONF_ACTIVE_HIGH]))
    if CONF_MULTI_KEYS in config:
        cg.add(var.set_multi_keys(config[CONF_MULTI_KEYS]))
    if CONF_SCAN_DELAY in config:
        cg.add(var.set_scan_delay(config[CONF_SCAN_DELAY]))
    for conf in config.get(CONF_ON_KEY, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        cg.add(var.register_key_trigger(trigger))
        await automation.build_automation(trigger, [(cg.uint8, "x")], conf)
    for conf in config.get(CONF_ON_STATE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        cg.add(var.register_state_trigger(trigger))
        await automation.build_automation(trigger, 
            [(cg.bool_, "state"), (cg.int_, "pos"), (cg.int_, "row"), (cg.int_, "col"), 
             (cg.int_, "rowcnt"), (cg.int_, "colcnt"), (cg.uint8, "key")], conf)
    if CONF_SCRIPT_ID in config:
        script_var = await cg.get_variable(config[CONF_SCRIPT_ID])
        cg.add(var.set_script_id(script_var))
