// firmware/src/scanner.cpp
#include "scanner.h"

// Read scanner data on an ESP32 GPIO, not the board's USB-serial RXD/TXD path.
// Wire scanner TXD to display-board GPIO17/P4 IO17, plus shared GND.
#define SCANNER_USES_MAIN_SERIAL 0
#define SCANNER_RX 17
#define SCANNER_TX -1
#define SCANNER_BAUD 9600

String scanner2Buffer = "";
String serialBuffer = "";
String lastBarcode = "";
unsigned long lastBarcodeMs = 0;

void logRawScannerByte(uint8_t b) {
  (void)b;
}

String barcodeFromDigitRun(String digits) {
  if (digits.length() >= 8 && digits.length() <= 14) {
    return digits;
  }

  // Some scanners repeat the same barcode while the trigger is held, with no
  // separator: 012345678901012345678901. Detect the repeated unit.
  for (int len = 8; len <= 14; len++) {
    if ((int)digits.length() < len * 2) continue;

    String candidate = digits.substring(0, len);
    bool repeated = true;
    for (int i = len; i + len <= (int)digits.length(); i += len) {
      if (digits.substring(i, i + len) != candidate) {
        repeated = false;
        break;
      }
    }

    if (repeated) return candidate;
  }

  return "";
}

String extractBarcode(String& buffer) {
  String digits = "";

  for (int i = 0; i < (int)buffer.length(); i++) {
    char c = buffer.charAt(i);
    if (c >= '0' && c <= '9') {
      digits += c;
    } else {
      String barcode = barcodeFromDigitRun(digits);
      if (barcode.length() > 0) {
        buffer = buffer.substring(i + 1);
        return barcode;
      }
      digits = "";
    }
  }

  String barcode = barcodeFromDigitRun(digits);
  if (barcode.length() > 0) {
    buffer = "";
    return barcode;
  }

  if (buffer.length() > 128) {
    buffer = buffer.substring(buffer.length() - 32);
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
  Serial.println("Scanner TXD must be wired to ESP32 GPIO17/P4 IO17.");
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
    unsigned long now = millis();
    if (barcode == lastBarcode && now - lastBarcodeMs < 1800) {
      Serial.println("Scanner duplicate ignored: [" + barcode + "]");
      return "";
    }

    lastBarcode = barcode;
    lastBarcodeMs = now;
    Serial.println();
    Serial.println("=== SCANNED BARCODE ===");
    Serial.println(barcode);
    Serial.println("=======================");
    return barcode;
  }

  return "";
}
