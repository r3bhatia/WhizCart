// firmware/src/scanner.cpp
#include "scanner.h"

#define SCANNER_RX 27
#define SCANNER_TX 22
#define SCANNER_BAUD 9600

void scanner_init() {
  Serial2.begin(SCANNER_BAUD, SERIAL_8N1, SCANNER_RX, SCANNER_TX);
  Serial.printf("Scanner initialized RX=%d TX=%d at %d baud\n",
    SCANNER_RX, SCANNER_TX, SCANNER_BAUD);
}

String scanner_read() {
  if (!Serial2.available()) return "";
  
  delay(50);
  String raw = Serial2.readString();
  String barcode = "";

  for (int i = 0; i < (int)raw.length(); i++) {
    char c = raw.charAt(i);
    if (c >= '0' && c <= '9') {
      barcode += c;
    }
  }

  if (barcode.length() > 0) {
    Serial.println("Scanner barcode: [" + barcode + "]");
  } else if (raw.length() > 0) {
    Serial.print("Scanner ignored non-barcode bytes: ");
    Serial.println(raw.length());
  }

  return barcode;
}
