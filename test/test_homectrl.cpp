#include <unity.h>
#include <eeprom.h>
#include <homectrl.h>

#define SLAVE_ID 3

settings_t settings {
  MAGIC,
  SLAVE_ID
};

uint8_t pinidx=2;

digital_pin_setting_t pin_settings;

digital_pin_context_t pin_ctx;

void setUp() {
  pin_ctx.index=pinidx;
  pin_ctx.settings = &pin_settings;
  pin_ctx.counters = counters + pinidx;

}
void test_handle_click(void) {
  handle_click(&pin_ctx);
  TEST_ASSERT_EQUAL(1, counters[pinidx].click_cnt);
}

void test_read_setting_magic(void) {
  EEPROM.put(SETTINGS_OFFSET, settings);
  uint8_t value = read_setting(0);
  TEST_ASSERT_EQUAL(MAGIC, value);
}

void test_write_setting_slave_id(void) {
  write_settings_to_eeprom(settings);
  uint8_t value = read_setting(1);
  TEST_ASSERT_EQUAL(SLAVE_ID, value);
}

void test_read_setting_slave_id(void) {
  write_setting(1, 10);
  read_settings_from_eeprom(settings);
  TEST_ASSERT_EQUAL(10, settings.slave_id);
}

void test_r_setting_group_p2(void) {
  write_setting(8 + (2 * 16) + 1, 23);
  read_digital_pin_settings_from_eeprom(pinidx, pin_settings);
  TEST_ASSERT_EQUAL(23, pin_settings.group);
}

void test_write_setting_mode_pin_2(void) {
  pin_settings.mode=DIGITAL_PIN_MODE_OUTPUT;
  write_digital_pin_settings_to_eeprom(pinidx, pin_settings);
  uint8_t value = read_setting(8+(2*16)+0);
  TEST_ASSERT_EQUAL(DIGITAL_PIN_MODE_OUTPUT, value);
}

void test_read_click_cnt_pin_2(void) {
  counters[2].click_cnt=5;
  uint16_t value = read_single_input_register(8 + 2*8 + 1);
  TEST_ASSERT_EQUAL(5, value);
}

void test_read_release_cnt_pin_2(void) {
  counters[2].release_cnt=5;
  uint16_t value = read_single_input_register(8 + 2*8 + 5);
  TEST_ASSERT_EQUAL(5, value);
}


void setup() {
  // NOTE!!! Wait for >2 secs
  // if board doesn't support software reset via Serial.DTR/RTS
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_handle_click);
  RUN_TEST(test_read_setting_magic);
  RUN_TEST(test_write_setting_slave_id);
  RUN_TEST(test_write_setting_mode_pin_2);
  RUN_TEST(test_read_setting_slave_id);
  RUN_TEST(test_r_setting_group_p2);
  RUN_TEST(test_read_click_cnt_pin_2);
  RUN_TEST(test_read_release_cnt_pin_2);
  UNITY_END();
}

void loop() {}
