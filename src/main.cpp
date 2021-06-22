#include "Arduino.h"
#include <avr/pgmspace.h>
#include <EEPROM.h>
#include <stdlib.h>
#include <stdint.h>
#include <homectrl.h>
#include <MultiButton.h>

#define FADE_SPEED 5

#define DEBUG
#ifdef DEBUG
 #define DEBUG_PRINT(x)     Serial.print (x)
 #define DEBUG_PRINTDEC(x)     Serial.print (x, DEC)
 #define DEBUG_PRINTLN(x)  Serial.println (x)
#else
 #define DEBUG_PRINT(x)
 #define DEBUG_PRINTDEC(x)
 #define DEBUG_PRINTLN(x)
#endif 

Modbus slave(Serial, DEFAULT_SLAVE_ID, RS485_CTRL_PIN);

const uint8_t digital_pins_numbers[NR_OF_DIGITAL_PINS] =
  {10, 11, 12, 13, 14, 15, 16, 17, 9, 6, 5, 3};

digital_pin_counters_t counters[NR_OF_DIGITAL_PINS];
MultiButton buttons[NR_OF_DIGITAL_PINS];
uint8_t current_values[NR_OF_DIGITAL_PINS];
output_state_t output_states[NR_OF_DIGITAL_PINS];

void read_settings_from_eeprom(settings_t &settings) {
  EEPROM.get(SETTINGS_OFFSET, settings);
}

void write_settings_to_eeprom(const settings_t &settings) {
  EEPROM.put(SETTINGS_OFFSET, settings);
}

inline uint8_t pin_setting_start_address(uint8_t index) {
  return SETTINGS_OFFSET + sizeof(settings_t) + index* sizeof(digital_pin_setting_t);
}

void read_digital_pin_settings_from_eeprom(uint8_t index, digital_pin_setting_t &setting) {
  EEPROM.get(pin_setting_start_address(index), setting);
}

void write_digital_pin_settings_to_eeprom(uint8_t index, const digital_pin_setting_t &setting) {
  EEPROM.put(pin_setting_start_address(index), setting);
}

uint8_t pin_mode(digital_pin_setting_t *setting) {
  return (setting->mode & DIGITAL_PIN_MODE_MASK);
}

uint8_t is_output(digital_pin_setting_t *setting) {
  return pin_mode(setting)  == DIGITAL_PIN_MODE_OUTPUT;
}

uint8_t has_pwm(digital_pin_setting_t *setting) {
  return setting->mode & DIGITAL_PIN_PWM_MASK;
}

uint8_t has_button(digital_pin_setting_t *setting) {
  return (setting->mode & DIGITAL_PIN_BUTTON_MASK) > 0;
}

void set_pin_output(digital_pin_context_t *ctx, uint8_t value) {
  *ctx->current_value = value;
  if (has_pwm(&ctx->settings)) {
    analogWrite(ctx->pin_number, value);
  } else {
    digitalWrite(ctx->pin_number, value);
  }
}

state_t handle_output_init_state(digital_pin_context_t *ctx, uint8_t command) {
  uint8_t output = ctx->settings.output_value;
  set_pin_output(ctx, output);
  return OUTPUT_STATE_IDLE;
}

state_t handle_output_idle_state(digital_pin_context_t *ctx, uint8_t command) {
  uint8_t pwm = has_pwm(&ctx->settings);
  uint8_t on_value = pwm ? ctx->settings.pwm_on_value : 0x1;
  uint8_t new_value = *ctx->current_value;
  state_t new_state = OUTPUT_STATE_IDLE;
  switch (command) {
  case COMMAND_NONE:
    break;
  case COMMAND_ON:
    new_value = on_value;
    break;
  case COMMAND_OFF:
    new_value = 0x0;
    break;
  case COMMAND_TOGGLE:
    new_value = *ctx->current_value > 0 ? 0 : on_value;
    break;
  case COMMAND_PWM_INCREASE:
    if (pwm && *ctx->current_value < 0xFF) {
      new_value = *ctx->current_value + 1;
    }
    break;
  case COMMAND_PWM_DECREASE:
    if (pwm && *ctx->current_value > 0x00) {
      new_value = *ctx->current_value - 1;
    }
    break;
  case COMMAND_FADE_TO_ON:
    if (pwm) {
      new_state = OUTPUT_STATE_FADE_TO_ON;
    }
    break;
  case COMMAND_FADE_TO_OFF:
    if (pwm) {
      new_state = OUTPUT_STATE_FADE_TO_OFF;
    }
    break;
  case COMMAND_FADE_TOGGLE:
    if (pwm) {
      if( *ctx->current_value > 0) {
        new_state = OUTPUT_STATE_FADE_TO_OFF;
      } else {
        new_state = OUTPUT_STATE_FADE_TO_ON;
      }
    }
    break;
  case COMMAND_TRIGGER_PWM_UP_DOWN_START:
    if (pwm) {
      new_state = OUTPUT_STATE_PWM_UP;
    }
    break;
  default:
    break;
  }
  if (new_value != *ctx->current_value) {
    set_pin_output(ctx, new_value);
  }
  if (ctx->settings.output_value != new_value) {
    ctx->settings.output_value = new_value;
    write_digital_pin_settings_to_eeprom(ctx->index, ctx->settings);
  }
  return new_state;
}

void save_pwm_on_value(digital_pin_context_t *ctx) {
  ctx->settings.pwm_on_value = *ctx->current_value;
  write_digital_pin_settings_to_eeprom(ctx->index, ctx->settings);
}

state_t handle_output_fade_to_on(digital_pin_context_t *ctx, uint8_t command) {
  uint8_t on_value = ctx->settings.pwm_on_value;
  if (*ctx->current_value >= on_value) {
    return OUTPUT_STATE_IDLE;
  }
  uint8_t new_value = (*ctx->current_value) + 1;
  set_pin_output(ctx, new_value);
  return OUTPUT_STATE_FADE_TO_ON_WAIT;
}

inline unsigned long millis_since_last_state_change(digital_pin_context_t *ctx) {
  return millis()-ctx->output_state->last_update;
}

state_t handle_output_fade_to_on_wait(digital_pin_context_t *ctx,
                                      uint8_t command) {
  unsigned long diff = millis_since_last_state_change(ctx);
  if (diff > FADE_SPEED) {
    return OUTPUT_STATE_FADE_TO_ON;
  } else {
    return OUTPUT_STATE_FADE_TO_ON_WAIT;
  }
}

state_t handle_output_fade_to_off(digital_pin_context_t *ctx, uint8_t command) {
  if (*ctx->current_value == 0x00) {
    return OUTPUT_STATE_IDLE;
  }
  uint8_t new_value = (*ctx->current_value) - 1;
  set_pin_output(ctx, new_value);
  return OUTPUT_STATE_FADE_TO_OFF_WAIT;
}

state_t handle_output_fade_to_off_wait(digital_pin_context_t *ctx,
                                      uint8_t command) {
  unsigned long diff = millis_since_last_state_change(ctx);
  if (diff > FADE_SPEED) {
    return OUTPUT_STATE_FADE_TO_OFF;
  } else {
    return OUTPUT_STATE_FADE_TO_OFF_WAIT;
  }
}

state_t handle_output_pwm_up(digital_pin_context_t *ctx, uint8_t command) {
  if (command == COMMAND_TRIGGER_PWM_UP_DOWN_STOP) {
    return OUTPUT_STATE_PWM_UP_DOWN_IDLE;
  }
  if (*ctx->current_value < 0xFF) {
    uint8_t new_value = (*ctx->current_value) + 1;
    set_pin_output(ctx, new_value);
  }
  return OUTPUT_STATE_PWM_UP_WAIT;
}

state_t handle_output_pwm_up_wait(digital_pin_context_t *ctx, uint8_t command) {
  if (command == COMMAND_TRIGGER_PWM_UP_DOWN_STOP) {
    return OUTPUT_STATE_PWM_UP_DOWN_IDLE;
  }
  unsigned long diff = millis_since_last_state_change(ctx);
  if (diff > FADE_SPEED) {
    return OUTPUT_STATE_PWM_UP;
  } else {
    return OUTPUT_STATE_PWM_UP_WAIT;
  }
}

state_t handle_output_pwm_down(digital_pin_context_t *ctx, uint8_t command) {
  if (command == COMMAND_TRIGGER_PWM_UP_DOWN_STOP) {
    save_pwm_on_value(ctx);
    return OUTPUT_STATE_IDLE;
  }
  if (*ctx->current_value > 0x01) {
    uint8_t new_value = (*ctx->current_value) - 1;
    set_pin_output(ctx, new_value);
  }
  return OUTPUT_STATE_PWM_DOWN_WAIT;
}

state_t handle_output_pwm_down_wait(digital_pin_context_t *ctx,
                                    uint8_t command) {
  if (command == COMMAND_TRIGGER_PWM_UP_DOWN_STOP) {
    save_pwm_on_value(ctx);
    return OUTPUT_STATE_IDLE;
  }
  unsigned long diff = millis_since_last_state_change(ctx);
  if (diff > FADE_SPEED) {
    return OUTPUT_STATE_PWM_DOWN;
  } else {
    return OUTPUT_STATE_PWM_DOWN_WAIT;
  }
}

#define UP_DOWN_TIMEOUT 2000
state_t handle_output_pwm_up_down_idle(digital_pin_context_t *ctx,
                                       uint8_t command) {
  if (command == COMMAND_TRIGGER_PWM_UP_DOWN_START) {
    return OUTPUT_STATE_PWM_DOWN;
  }
  unsigned long diff = millis_since_last_state_change(ctx);
  if (diff > UP_DOWN_TIMEOUT) {
    save_pwm_on_value(ctx);
    return OUTPUT_STATE_IDLE;
  } else {
    return OUTPUT_STATE_PWM_UP_DOWN_IDLE;
  }
}

void update_output(digital_pin_context_t *ctx, uint8_t command) {
  state_t current_state = ctx->output_state->state;
  state_t next_state;
  switch (current_state) {
  case OUTPUT_STATE_INIT:
    next_state = handle_output_init_state(ctx, command);
    break;
  case OUTPUT_STATE_IDLE:
    next_state = handle_output_idle_state(ctx, command);
    break;
  case OUTPUT_STATE_FADE_TO_ON:
    next_state = handle_output_fade_to_on(ctx, command);
    break;
  case OUTPUT_STATE_FADE_TO_ON_WAIT:
    next_state = handle_output_fade_to_on_wait(ctx, command);
    break;
  case OUTPUT_STATE_FADE_TO_OFF:
    next_state = handle_output_fade_to_off(ctx, command);
    break;
  case OUTPUT_STATE_FADE_TO_OFF_WAIT:
    next_state = handle_output_fade_to_off_wait(ctx, command);
    break;
  case OUTPUT_STATE_PWM_UP:
    next_state = handle_output_pwm_up(ctx, command);
    break;
  case OUTPUT_STATE_PWM_UP_WAIT:
    next_state = handle_output_pwm_up_wait(ctx, command);
    break;
  case OUTPUT_STATE_PWM_UP_DOWN_IDLE:
    next_state = handle_output_pwm_up_down_idle(ctx, command);
    break;
  case OUTPUT_STATE_PWM_DOWN:
    next_state = handle_output_pwm_down(ctx, command);
    break;
  case OUTPUT_STATE_PWM_DOWN_WAIT:
    next_state = handle_output_pwm_down_wait(ctx, command);
    break;
  default:
    break;
  }
  if (current_state != next_state) {
    ctx->output_state->state = next_state;
    ctx->output_state->last_update = millis();
  }
}

void init_digital_pin_context(uint8_t index, digital_pin_context_t *ctx) {
  read_digital_pin_settings_from_eeprom(index, ctx->settings);
  ctx->index = index;
  ctx->pin_number = digital_pins_numbers[index];
  ctx->counters = counters + index;
  ctx->current_value = current_values + index;
  ctx->output_state = output_states + index;
}

void trigger_command(digital_pin_context_t *origin_pin, uint8_t command) {
  uint8_t group = origin_pin->settings.group;
  if (group == 0) {
    return;
  }
  for (uint8_t i = 0; i < NR_OF_DIGITAL_PINS; i++) {
    digital_pin_context_t context;
    init_digital_pin_context(i, &context);
    if (context.settings.group != group || !is_output(&(context.settings))) {
      continue;
    }
    update_output(&context, command);    
  }
}

void handle_click(digital_pin_context_t *pin_ctx) {
  pin_ctx->counters->click_cnt++;
  trigger_command(pin_ctx, pin_ctx->settings.click_command);
}

void handle_single_click(digital_pin_context_t *pin_ctx) {
  pin_ctx->counters->single_click_cnt++;
  trigger_command(pin_ctx, pin_ctx->settings.single_click_command);
}

void handle_long_click(digital_pin_context_t *pin_ctx) {
  pin_ctx->counters->long_press_cnt++;
  trigger_command(pin_ctx, pin_ctx->settings.long_click_command);
}

void handle_double_click(digital_pin_context_t *pin_ctx) {
  pin_ctx->counters->double_click_cnt++;
  trigger_command(pin_ctx, pin_ctx->settings.double_click_command);
}

void handle_release(digital_pin_context_t *pin_ctx) {
  pin_ctx->counters->release_cnt++;
  trigger_command(pin_ctx, pin_ctx->settings.release_command);
}

void handle_rise(digital_pin_context_t *pin_ctx) {
  trigger_command(pin_ctx, pin_ctx->settings.signal_rise_command);
}

void handle_fall(digital_pin_context_t *pin_ctx) {
  trigger_command(pin_ctx, pin_ctx->settings.signal_fall_command);
}

uint8_t cb_read_coil(uint8_t fc, uint16_t address, uint16_t length) {
  if (address > NR_OF_DIGITAL_PINS || (address + length) > NR_OF_DIGITAL_PINS) {
    return STATUS_ILLEGAL_DATA_ADDRESS;
  }
  for (uint8_t i = 0; i < length; i++) {
    slave.writeCoilToBuffer(i, current_values[address+i]);
  }
  return STATUS_OK;
}

uint8_t cb_write_coil(uint8_t fc, uint16_t address, uint16_t length) {
  if (address > NR_OF_DIGITAL_PINS || (address + length) > NR_OF_DIGITAL_PINS) {
    return STATUS_ILLEGAL_DATA_ADDRESS;
  }
  for (uint8_t i = 0; i < length; i++) {
    digital_pin_context_t ctx;
    init_digital_pin_context(address+i, &ctx);
    update_output(&ctx, slave.readCoilFromBuffer(i)==1 ? COMMAND_ON: COMMAND_OFF);
  }
  return STATUS_OK;
}

uint8_t read_setting(uint8_t index) {
  uint8_t value = 0;
  EEPROM.get(index, value);
  return value;
};

uint8_t validate_pin_mode(uint8_t value) {
  uint8_t mode = value & DIGITAL_PIN_MODE_MASK;
  if (!(mode == DIGITAL_PIN_MODE_INPUT || mode == DIGITAL_PIN_MODE_OUTPUT ||
        mode == DIGITAL_PIN_MODE_INPUT_PULLUP)) {
    return STATUS_ILLEGAL_DATA_VALUE;
  }
  return STATUS_OK;
}

uint8_t validate_command(uint8_t value) {
  if (value <= COMMAND_TRIGGER_PWM_UP_DOWN_STOP){
    return STATUS_OK;
  }
  return STATUS_ILLEGAL_DATA_VALUE;
}

uint8_t write_digital_pin_setting(uint8_t pin, uint8_t offset, uint8_t value) {
  if (offset == 0) {
    uint8_t status = validate_pin_mode(value);
    if (status != STATUS_OK) {
      return status;
    }
    pinMode(digital_pins_numbers[pin], value & DIGITAL_PIN_MODE_MASK);
  }
  if(offset > 2 && offset < 9) {
    uint8_t status = validate_command(value);
    if (status != STATUS_OK) {
      return status;
    }
  }
  EEPROM.put(pin_setting_start_address(pin) + offset, value);
  return STATUS_OK;
}

#define HOLDING_REGISTER_MAGIC_ADDRESS 0
#define HOLDING_REGISTER_SLAVE_ID_ADDRESS 1
uint8_t write_setting(uint8_t index, uint8_t value) {
  if (index == HOLDING_REGISTER_MAGIC_ADDRESS) {
    return STATUS_ILLEGAL_DATA_ADDRESS;
  }
  if (index == HOLDING_REGISTER_SLAVE_ID_ADDRESS) { // slave id
    if (value > 0xFF) {
      return STATUS_ILLEGAL_DATA_VALUE;
    } else {
      EEPROM.put(index, value);
      return STATUS_OK;
    }
  }
  if (index >= 8) {
    uint8_t pin = (index-8)/sizeof(digital_pin_setting_t);
    uint8_t offset = (index-8) % sizeof(digital_pin_setting_t);
    return write_digital_pin_setting(pin, offset, value);
  }
  return STATUS_ILLEGAL_DATA_ADDRESS;
}

uint16_t read_counter(uint8_t index) {
  uint16_t* counter = (uint16_t *) &counters;
  return *(counter + index);
}

/**
 * input register design
 *
 * address: value
 * 0 : number of digital pins
 * 1 - 7: reserved
 * 
 * per pin registers starting at address 8 
 *
 * 8: pin 1 pin number
 * 9: pin 1 click counter
 * 10: pin 1 single click counter
 * 11: pin 1 double click counter
 * 12: pin 1 long press counter
 * 13: pin 1 release counter
 * 14-15 reserved
 * 
 */

#define INPUT_REGISTER_VALUE_RESERVED 0
#define INPUT_REGISTER_NUMBER_OF_PINS_ADDRESS 0
#define INPUT_REGISTER_PER_PIN_ADDRESS_OFFSET 8
#define INPUT_REGISTER_PER_PIN_PINNUMBER_ADDRESS 0
#define INPUT_REGISTER_UPTIME_ADDRESS 1
#define INPUT_REGISTER_PER_PIN_RESERVED 6
#define INPUT_REGISTER_PER_PIN_SIZE 8
#define INPUT_REGISTER_PER_PIN_COUNTER_SIZE 5

uint16_t read_single_input_register(uint16_t address) {
  if (address == INPUT_REGISTER_NUMBER_OF_PINS_ADDRESS) {
    return NR_OF_DIGITAL_PINS;
  }
  if (address == INPUT_REGISTER_UPTIME_ADDRESS) {
    return (millis() & 0xFFFF);
  }
  if (address == INPUT_REGISTER_UPTIME_ADDRESS + 1) {
    return (millis() >> 16)&0xFFFF;
  }
  if (address < INPUT_REGISTER_PER_PIN_ADDRESS_OFFSET) {
    return INPUT_REGISTER_VALUE_RESERVED;
  }
  uint8_t pin_index = (address-INPUT_REGISTER_PER_PIN_ADDRESS_OFFSET)/INPUT_REGISTER_PER_PIN_SIZE;
  uint8_t pin_address = (address-INPUT_REGISTER_PER_PIN_ADDRESS_OFFSET) % INPUT_REGISTER_PER_PIN_SIZE;

  if (pin_address == INPUT_REGISTER_PER_PIN_PINNUMBER_ADDRESS) {
    return digital_pins_numbers[pin_index];
  }
  if (pin_address >= INPUT_REGISTER_PER_PIN_RESERVED) {
    return INPUT_REGISTER_VALUE_RESERVED;
  }
  return read_counter(pin_index * INPUT_REGISTER_PER_PIN_COUNTER_SIZE
                      + pin_address
                      - 1);
}

uint8_t cb_read_input_register(uint8_t fc, uint16_t address, uint16_t length) {
  uint16_t max_size = NR_OF_DIGITAL_PINS*INPUT_REGISTER_PER_PIN_SIZE
    + INPUT_REGISTER_PER_PIN_ADDRESS_OFFSET;
  if (address > max_size || address + length > max_size) {
    return STATUS_ILLEGAL_DATA_ADDRESS;
  }
  for (uint8_t i = 0; i < length; i++) {
    uint16_t value = read_single_input_register(address + i);
    slave.writeRegisterToBuffer(i, value);
  }
  return STATUS_OK;  
}


uint8_t cb_read_holding_register(uint8_t fc, uint16_t address, uint16_t length) {
  uint16_t max_size = sizeof(settings_t) + sizeof(digital_pin_setting_t) * NR_OF_DIGITAL_PINS;
  if (address > max_size || address + length > max_size) {
    return STATUS_ILLEGAL_DATA_ADDRESS;
  }
  for (uint8_t i = 0; i < length; i++) {
    uint8_t value = read_setting(address + i);
    slave.writeRegisterToBuffer(i, value);
  }
  return STATUS_OK;
}

uint8_t cb_write_holding_register(uint8_t fc, uint16_t address, uint16_t length) {
  uint16_t max_size = sizeof(settings_t) + sizeof(digital_pin_setting_t) * NR_OF_DIGITAL_PINS;
  if (address < 1 || address > max_size || address + length > max_size) {
    return STATUS_ILLEGAL_DATA_ADDRESS;
  }
  for (uint8_t i = 0; i < length; i++) {
    uint16_t value = slave.readRegisterFromBuffer(i);
    if (value > 0xFF) {
      return STATUS_ILLEGAL_DATA_VALUE;
    }
    uint8_t status = write_setting(address+i, value);
    if (status != STATUS_OK) {
      return status;
    }
  }
  return STATUS_OK;
}


void poll_input(digital_pin_context_t *ctx) {
  uint8_t i = ctx->index;
  uint8_t rawValue = digitalRead(digital_pins_numbers[i]);
    if (has_button(&ctx->settings)) {
      buttons[i].update(rawValue == 0);
      if (buttons[i].isClick()) {
        handle_click(ctx);
      }
      if (buttons[i].isSingleClick()) {
        handle_single_click(ctx);
      }
      if (buttons[i].isLongClick()) {
        handle_long_click(ctx);
      }
      if (buttons[i].isDoubleClick()) {
        handle_double_click(ctx);
      }
      if (buttons[i].isReleased()) {
        handle_release(ctx);
      }
    } else {
      if (*ctx->current_value < rawValue) {
        handle_rise(ctx);
      }
      if (*ctx->current_value > rawValue) {
        handle_fall(ctx);
      }
      *ctx->current_value = rawValue;
    }
}

void loop_digital_pins() {
  for (uint8_t i = 0; i < NR_OF_DIGITAL_PINS; i++) {
    digital_pin_context_t pin_ctx;
    init_digital_pin_context(i, &pin_ctx);
    if (is_output(&pin_ctx.settings)) {
      update_output(&pin_ctx, COMMAND_NONE);
    } else {
      poll_input(&pin_ctx);
    }
  }
}

#ifndef UNIT_TEST
// cppcheck-suppress unusedFunction
void setup() {
  memset(counters, 0, NR_OF_DIGITAL_PINS * sizeof(digital_pin_counters_t));
  memset(current_values, 0, NR_OF_DIGITAL_PINS * sizeof(uint8_t));
  memset(output_states, 0, NR_OF_DIGITAL_PINS * sizeof(output_state_t));
  settings_t settings;
  read_settings_from_eeprom(settings);
  Serial.begin(BAUD_RATE);
  if (settings.magic != MAGIC) {
    settings.magic = MAGIC;
    settings.slave_id = DEFAULT_SLAVE_ID;
    write_settings_to_eeprom(settings);
  }

  for (uint8_t i = 0; i < NR_OF_DIGITAL_PINS; i++) {
    digital_pin_setting_t pin_setting;
    read_digital_pin_settings_from_eeprom(i, pin_setting);
    pinMode(digital_pins_numbers[i], pin_mode(&pin_setting));
  }
  
  slave.cbVector[CB_READ_COILS] = cb_read_coil;
  slave.cbVector[CB_WRITE_COILS] = cb_write_coil;
  slave.cbVector[CB_READ_HOLDING_REGISTERS] = cb_read_holding_register;
  slave.cbVector[CB_WRITE_HOLDING_REGISTERS] = cb_write_holding_register;
  slave.cbVector[CB_READ_INPUT_REGISTERS] = cb_read_input_register;
  slave.begin(BAUD_RATE);
}

// cppcheck-suppress unusedFunction
void loop() {
  slave.poll();
  loop_digital_pins();
};

#endif
