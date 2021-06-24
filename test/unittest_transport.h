#ifndef UNITTEST_TRANSPORT_H
#define UNITTEST_TRANSPORT_H

#include <Arduino.h>

void unittest_uart_begin() {
  Serial.begin(115200);
}

void unittest_uart_putchar(char c) {
  digitalWrite(8, 1);
  Serial.write(c);
  digitalWrite(8, 0);
}

void unittest_uart_flush() {
  Serial.flush();
}

void unittest_uart_end() {
  Serial.end();
}

#endif
