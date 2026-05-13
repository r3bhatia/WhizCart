// firmware/src/scanner.cpp
#include "scanner.h"

#define SCANNER_RX 27
#define SCANNER_TX 22

void scanner_init() {
  Serial2.begin(9600, SERIAL_8N1, SCANNER_RX, SCANNER_TX);
  Serial.println("Scanner initialized on GPIO 22/27 at 9600");
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
/*

String scanner_read() {
  if (!Serial2.available()) return "";
  
  delay(50);
  String raw = Serial2.readStringUntil('\n');
  
  // Remove ALL non-printable characters including \r, \t, and other control chars
  String cleaned = "";
  for (int i = 0; i < raw.length(); i++) {
    char c = raw[i];
    if (c >= 32 && c < 127) {  // only keep standard printable ASCII
      cleaned += c;
    }
  }

  if (cleaned.length() > 0) {
    Serial.println("Scanner read: [" + cleaned + "]");
  }

  return cleaned;
}
  */