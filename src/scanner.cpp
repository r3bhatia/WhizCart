// firmware/src/scanner.cpp
#include "scanner.h"

// The 7-inch ESP32-S3 board's exposed RXD/TXD scanner connector appears to use
// the same UART as the Serial Monitor on this board, so scanner input is read
// from Serial at 9600 baud. Set Serial Monitor to 9600 for scanner testing.
#define SCANNER_USES_MAIN_SERIAL 1
#define SCANNER_RX 44
#define SCANNER_TX 43
#define SCANNER_BAUD 9600

String scanner2Buffer = "";
String serialBuffer = "";
unsigned long lastRawScannerLogMs = 0;
int rawScannerBytesThisWindow = 0;

void logRawScannerByte(uint8_t b) {
  unsigned long now = millis();
  if (now - lastRawScannerLogMs > 700) {
    if (rawScannerBytesThisWindow > 0) Serial.println();
    Serial.print("[Scanner raw hex] ");
    rawScannerBytesThisWindow = 0;
    lastRawScannerLogMs = now;
  }

  if (rawScannerBytesThisWindow < 32) {
    if (b < 16) Serial.print("0");
    Serial.print(b, HEX);
    Serial.print(" ");
  }
  rawScannerBytesThisWindow++;
}

String extractBarcode(String& buffer) {
  String digits = "";

  for (int i = 0; i < (int)buffer.length(); i++) {
    char c = buffer.charAt(i);
    if (c >= '0' && c <= '9') {
      digits += c;
    } else {
      if (digits.length() >= 8 && digits.length() <= 14) {
        buffer = buffer.substring(i + 1);
        return digits;
      }
      digits = "";
    }
  }

  if (digits.length() >= 8 && digits.length() <= 14) {
    buffer = "";
    return digits;
  }

  if (buffer.length() > 64) {
    buffer = buffer.substring(buffer.length() - 16);
  }

  return "";
}

void scanner_init() {
#if SCANNER_USES_MAIN_SERIAL
  Serial.println("Scanner initialized on main Serial at 9600 baud");
#else
  Serial2.begin(SCANNER_BAUD, SERIAL_8N1, SCANNER_RX, SCANNER_TX);
  Serial.printf("Scanner initialized Serial2 RX=%d TX=%d at %d baud\n",
    SCANNER_RX, SCANNER_TX, SCANNER_BAUD);
#endif
  Serial.println("Scanner accepts barcode digits from the board RXD/TXD connector or Serial Monitor.");
}

String scanner_read() {
  String barcode = "";

#if !SCANNER_USES_MAIN_SERIAL
  while (Serial2.available()) {
    uint8_t b = Serial2.read();
    logRawScannerByte(b);
    scanner2Buffer += (char)b;
  }

  barcode = extractBarcode(scanner2Buffer);
  if (barcode.length() > 0) {
    Serial.println("Scanner barcode Serial2: [" + barcode + "]");
    return barcode;
  }
#endif

  while (Serial.available()) {
    uint8_t b = Serial.read();
    logRawScannerByte(b);
    serialBuffer += (char)b;
  }

  barcode = extractBarcode(serialBuffer);
  if (barcode.length() > 0) {
    Serial.println("Scanner barcode Serial: [" + barcode + "]");
    return barcode;
  }

  return "";
}
