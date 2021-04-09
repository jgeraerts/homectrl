#include <unity.h>
#include <homectrl.h>

void test_handle_click(void) {
  handle_click(0);
  TEST_ASSERT_EQUAL(1, counters[0].click_cnt);
}

void test_read_setting_magic(void) {
  uint8_t value = read_setting(0);
  TEST_ASSERT_EQUAL(STATUS_OK, value);
}

void test_read_setting_slave_id(void) {
  settings.slave_id = 3;
  uint8_t value = read_setting(1);
  TEST_ASSERT_EQUAL(3, value);
}

void test_read_setting_mode_pin_2(void) {
  settings.digital_pin_settings[2].mode=DIGITAL_PIN_MODE_OUTPUT;
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

void test_write_setting_slave_id(void) {
  write_setting(1, 10);
  TEST_ASSERT_EQUAL(10, settings.slave_id);
}

void test_w_setting_group_p2(void) {
  write_setting(8 + (2 * 16) + 1, 23);
  TEST_ASSERT_EQUAL(23, settings.digital_pin_settings[2].group);
}



void setup() {
  // NOTE!!! Wait for >2 secs
  // if board doesn't support software reset via Serial.DTR/RTS
  delay(2000);
  UNITY_BEGIN(); // IMPORTANT LINE!
  RUN_TEST(test_handle_click);
  RUN_TEST(test_read_setting_magic);
  RUN_TEST(test_read_setting_slave_id);
  RUN_TEST(test_read_setting_mode_pin_2);
  RUN_TEST(test_write_setting_slave_id);
  RUN_TEST(test_w_setting_group_p2);
  RUN_TEST(test_read_click_cnt_pin_2);
  RUN_TEST(test_read_release_cnt_pin_2);
  UNITY_END();
}

void loop() {}
