#ifndef HOMECTRL_h
#define HOMECTRL_h


#include <stdlib.h>
#include <stdint.h>

#include <PinButton.h>
#include <ModbusSlave.h>

#define MAGIC 0x43
#define SETTINGS_OFFSET 0
#define DEFAULT_SLAVE_ID 1
#define BAUD_RATE 19200
#define RS485_CTRL_PIN 8


#define DIGITAL_PIN_MODE_MASK 0x03
#define DIGITAL_PIN_MODE_INPUT 0x0
#define DIGITAL_PIN_MODE_OUTPUT 0x1
#define DIGITAL_PIN_MODE_INPUT_PULLUP 0x2
#define DIGITAL_PIN_BUTTON_MASK 0x04
#define DIGITAL_PIN_INVERSE_MASK 0x08
#define DIGITAL_PIN_PWM_MASK 0x10

#define COMMAND_NONE 0x00
#define COMMAND_OFF 0x01
#define COMMAND_ON 0x02
#define COMMAND_TOGGLE 0x03
#define COMMAND_PWM_INCREASE 0x04
#define COMMAND_PWM_DECREASE 0x05

#define NR_OF_DIGITAL_PINS 12


typedef struct __attribute__((__packed__)) {
  uint8_t mode;
  uint8_t group;
  uint8_t output_value;
  uint8_t click_command;
  uint8_t single_click_command;
  uint8_t long_click_command;
  uint8_t double_click_command;
  uint8_t signal_rise_command;
  uint8_t signal_fall_command;
  uint8_t release_command;
  uint8_t reserved[6];
} digital_pin_setting_t;

typedef struct __attribute__((__packed__)) {
  uint8_t magic;
  uint8_t slave_id;
  uint8_t reserved[6];
  digital_pin_setting_t digital_pin_settings[NR_OF_DIGITAL_PINS];
} settings_t;

typedef struct {
  uint16_t click_cnt;
  uint16_t single_click_cnt;
  uint16_t double_click_cnt;
  uint16_t long_press_cnt;
  uint16_t release_cnt;
} digital_pin_counters_t;

extern const uint8_t digital_pins_numbers[];
extern digital_pin_counters_t counters[];
extern settings_t settings;
extern MultiButton buttons[];
extern uint8_t current_values[];

void handle_click(uint8_t index);

uint8_t read_setting(uint8_t index);
uint8_t write_setting(uint8_t index, uint8_t value);
uint16_t read_single_input_register(uint16_t address);
#endif
