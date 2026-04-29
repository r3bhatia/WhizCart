// firmware/src/scanner.cpp
// Reads barcode strings from a UART barcode scanner module.
// Most USB/TTL barcode scanners output the barcode followed by CR+LF.
// Wiring: Scanner TX → ESP32 GPIO16 (RX2), Scanner RX → ESP32 GPIO17 (TX2)
// Adjust RX_PIN / TX_PIN and BAUD_RATE to match your scanner module.

#include "scanner.h"

#define SCANNER_RX   16
#define SCANNER_TX   17
#define BAUD_RATE    9600

HardwareSerial scannerSerial(2); // UART2

void scanner_init() {
  scannerSerial.begin(BAUD_RATE, SERIAL_8N1, SCANNER_RX, SCANNER_TX);
}

String scanner_read() {
  if (!scannerSerial.available()) return "";

  String raw = scannerSerial.readStringUntil('\n');
  raw.trim(); // strip CR, LF, whitespace
  return raw;
}
