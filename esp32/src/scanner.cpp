// firmware/src/scanner.cpp
#include "scanner.h"

#define SCANNER_RX 27
#define SCANNER_TX 22

void scanner_init() {
  Serial2.begin(115200, SERIAL_8N1, SCANNER_RX, SCANNER_TX);
  Serial.println("Scanner initialized on GPIO 16/17 at 115200");
}

String scanner_read() {
  if (!Serial2.available()) return "";
  
  delay(50);
  String raw = Serial2.readStringUntil('\n');
  raw.trim();
  
  if (raw.length() > 0) {
    Serial.println("Scanner read: [" + raw + "]");
  }
  
  return raw;
}