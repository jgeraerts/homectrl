#ifndef HOMECTRL_h
#define HOMECTRL_h


#include <stdlib.h>
#include <stdint.h>

#include <MultiButton.h>
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
#define COMMAND_FADE_TO_ON 0x06
#define COMMAND_FADE_TO_OFF 0x07
#define COMMAND_FADE_TOGGLE 0x08
#define COMMAND_TRIGGER_PWM_UP_DOWN_START 0x09
#define COMMAND_TRIGGER_PWM_UP_DOWN_STOP 0x0a

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
  uint8_t pwm_on_value;
  uint8_t reserved[5];
} digital_pin_setting_t;

typedef struct __attribute__((__packed__)) {
  uint8_t magic;
  uint8_t slave_id;
  uint8_t reserved[6];
} settings_t;

typedef struct {
  uint16_t click_cnt;
  uint16_t single_click_cnt;
  uint16_t double_click_cnt;
  uint16_t long_press_cnt;
  uint16_t release_cnt;
} digital_pin_counters_t;

enum output_state {
  OUTPUT_STATE_INIT=0,
  OUTPUT_STATE_IDLE,
  OUTPUT_STATE_FADE_TO_ON,
  OUTPUT_STATE_FADE_TO_ON_WAIT,
  OUTPUT_STATE_FADE_TO_OFF,
  OUTPUT_STATE_FADE_TO_OFF_WAIT,
  OUTPUT_STATE_PWM_UP,
  OUTPUT_STATE_PWM_UP_WAIT,
  OUTPUT_STATE_PWM_UP_DOWN_IDLE,
  OUTPUT_STATE_PWM_DOWN,
  OUTPUT_STATE_PWM_DOWN_WAIT,
};

struct output_pin_state {
  enum output_state state;
  unsigned long last_update;
};

typedef struct {
  uint8_t index;
  uint8_t pin_number;
  digital_pin_setting_t settings;
  digital_pin_counters_t *counters;
  uint8_t *current_value;
  struct output_pin_state *output_state;
} digital_pin_context_t;

extern const uint8_t digital_pins_numbers[];
extern digital_pin_counters_t counters[];
extern MultiButton buttons[];
extern uint8_t current_values[];
extern struct output_pin_state output_states[];


void handle_click(digital_pin_context_t *pin_ctx);

uint8_t read_setting(uint8_t index);
uint8_t write_setting(uint8_t index, uint8_t value);
uint16_t read_single_input_register(uint16_t address);
void write_digital_pin_settings_to_eeprom(uint8_t index, const digital_pin_setting_t &setting);
void read_digital_pin_settings_from_eeprom(uint8_t index, digital_pin_setting_t &setting);
void read_settings_from_eeprom(settings_t &settings);
void write_settings_to_eeprom(const settings_t &settings); 
#endif
