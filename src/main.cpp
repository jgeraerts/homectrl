#include "Arduino.h"
#include <avr/pgmspace.h>
#include <EEPROM.h>
#include <stdlib.h>
#include <stdint.h>
#include <homectrl.h>

Modbus slave(Serial, DEFAULT_SLAVE_ID, RS485_CTRL_PIN);

const uint8_t digital_pins_numbers[NR_OF_DIGITAL_PINS] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
digital_pin_counters_t counters[NR_OF_DIGITAL_PINS];
settings_t settings;
MultiButton buttons[NR_OF_DIGITAL_PINS];
uint8_t current_values[NR_OF_DIGITAL_PINS];


void clear_eeprom() {
  for (uint8_t i = 0; i < EEPROM.length(); i++) {
    EEPROM.put(i, 0);
  }
}

void save_settings() {
  EEPROM.put(SETTINGS_OFFSET, settings);
}

uint8_t pin_mode(digital_pin_setting_t *setting) {
  return (setting->mode & DIGITAL_PIN_MODE_MASK);
}

uint8_t is_output(digital_pin_setting_t *setting) {
  return pin_mode(setting)  == DIGITAL_PIN_MODE_OUTPUT;
}

uint8_t has_button(digital_pin_setting_t *setting) {
  return (setting->mode & DIGITAL_PIN_BUTTON_MASK) > 0;
}

void update_output(uint8_t index, uint8_t new_value) {
  if (is_output(&(settings.digital_pin_settings[index]))) {
    current_values[index] = new_value;
    settings.digital_pin_settings[index].output_value = new_value;
    save_settings();
    digitalWrite(digital_pins_numbers[index], new_value);
  }
}

void trigger_command(uint8_t index, uint8_t command) {

  digital_pin_setting_t setting = settings.digital_pin_settings[index];
  uint8_t group = setting.group;
  if (group == 0) {
    return;
  }
  for (uint8_t i = 0; i < NR_OF_DIGITAL_PINS; i++) {
    
    if (is_output(&(settings.digital_pin_settings[i])) && settings.digital_pin_settings[i].group == group) {
      switch (command) {
      case COMMAND_NONE:
        break;
      case COMMAND_ON:
        update_output(i, 0x1);
        break;
      case COMMAND_OFF:
        update_output(i, 0x0);
        break;
      case COMMAND_TOGGLE:
        update_output(i, current_values[i] > 0 ? 0 : 1);
        break;
      default:
        break;
      }
    }
  }
}

void handle_click(uint8_t index) {
  counters[index].click_cnt++;
  trigger_command(index, settings.digital_pin_settings[index].click_command);
}

void handle_single_click(uint8_t index) {
  counters[index].single_click_cnt++;
  trigger_command(index, settings.digital_pin_settings[index].single_click_command);
}

void handle_long_click(uint8_t index) {
  counters[index].long_press_cnt++;
  trigger_command(index, settings.digital_pin_settings[index].long_click_command);
}

void handle_double_click(uint8_t index) {
  counters[index].double_click_cnt++;
  trigger_command(index, settings.digital_pin_settings[index].double_click_command);
}

void handle_release(uint8_t index) {
  counters[index].release_cnt++;
  trigger_command(index, settings.digital_pin_settings[index].release_command);
}

void handle_rise(uint8_t index) {
  trigger_command(index, settings.digital_pin_settings[index].signal_rise_command);
}

void handle_fall(uint8_t index) {
  trigger_command(index, settings.digital_pin_settings[index].signal_fall_command);
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
    update_output(address+i, slave.readCoilFromBuffer(i));
  }
  
  return STATUS_OK;
}

uint8_t read_setting(uint8_t index) {
  uint8_t* settings_ptr = (uint8_t *)&settings;
  uint8_t value = *(settings_ptr +index);
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
  if (value == COMMAND_NONE || value == COMMAND_ON || value == COMMAND_OFF ||
      value == COMMAND_TOGGLE) {
    return STATUS_OK;
  }
  return STATUS_ILLEGAL_DATA_VALUE;
}

uint8_t write_digital_pin_setting(uint8_t pin, uint8_t offset, uint8_t value) {
  uint8_t*  pin_setting  = (uint8_t *)(&(settings.digital_pin_settings[pin]));
  if (offset == 0) {
    uint8_t status = validate_pin_mode(value);
    if (status != STATUS_OK) {
      return status;
    }
    pinMode(digital_pins_numbers[pin], value & DIGITAL_PIN_MODE_MASK);
  }
  if(offset > 1 && offset < 8) {
    uint8_t status = validate_command(value);
    if (status != STATUS_OK) {
      return status;
    }
  }
  *(pin_setting + offset) = value;
  save_settings();
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
      settings.slave_id=value;
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
#define INPUT_REGISTER_PER_PIN_RESERVED 6
#define INPUT_REGISTER_PER_PIN_SIZE 8
#define INPUT_REGISTER_PER_PIN_COUNTER_SIZE 5

uint16_t read_single_input_register(uint16_t address) {
  if (address == INPUT_REGISTER_NUMBER_OF_PINS_ADDRESS) {
    return NR_OF_DIGITAL_PINS;
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
  uint16_t max_size = sizeof(settings);
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
  uint16_t max_size = sizeof(settings);
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

#ifndef UNIT_TEST
void setup() {
  Serial.begin(BAUD_RATE);
  memset(&settings, 0, sizeof(settings_t));
  EEPROM.get(SETTINGS_OFFSET, settings);
  if (settings.magic != MAGIC) {
    settings.magic = MAGIC;
    save_settings();
  }
  for (uint8_t i = 0; i < NR_OF_DIGITAL_PINS; i++) {
    digital_pin_setting_t pin_setting = settings.digital_pin_settings[i];
    pinMode(digital_pins_numbers[i], pin_mode(&pin_setting));
    if (is_output(&pin_setting) ) {
      update_output(i,pin_setting.output_value);
    }  
  }
  slave.cbVector[CB_READ_COILS] = cb_read_coil;
  slave.cbVector[CB_WRITE_COILS] = cb_write_coil;
  slave.cbVector[CB_READ_HOLDING_REGISTERS] = cb_read_holding_register;
  slave.cbVector[CB_WRITE_HOLDING_REGISTERS] = cb_write_holding_register;
  slave.cbVector[CB_READ_INPUT_REGISTERS] = cb_read_input_register;
  slave.begin(BAUD_RATE);
}


void loop() {
  slave.poll();
  for (uint8_t i = 0; i < NR_OF_DIGITAL_PINS; i++) {
    digital_pin_setting_t* setting = &(settings.digital_pin_settings[i]);
    if(is_output(setting)){
      continue;
    }
    uint8_t rawValue = digitalRead(digital_pins_numbers[i]);
    if (has_button(setting)) {
      
      buttons[i].update(rawValue == 0);
      if (buttons[i].isClick()) {
        handle_click(i);
      }
      if (buttons[i].isSingleClick()) {
        handle_single_click(i);
      }
      if (buttons[i].isLongClick()) {
        handle_long_click(i);
      }
      if (buttons[i].isDoubleClick()) {
        handle_double_click(i);
      }
      if (buttons[i].isReleased()) {
        handle_release(i);
      }
    } else {
      if (current_values[i] < rawValue) {
        handle_rise(i);
      }
      if (current_values[i] > rawValue) {
        handle_fall(i);
      }
      current_values[i] = rawValue;
    }
  }
};

#endif