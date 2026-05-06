#include "whizcart.h"

namespace {
HardwareSerial scanner(2);
constexpr int SCANNER_RX_PIN = 27;
constexpr int SCANNER_TX_PIN = 22;
}

void initScanner() {
  scanner.begin(9600, SERIAL_8N1, SCANNER_RX_PIN, SCANNER_TX_PIN);
}

static void processScannedCode(const String& code) {
  bool found = false;

  for (size_t i = 0; i < itemCount; i++) {
    if (code == items[i].barcode) {
      Serial.println(items[i].name);
      Serial.println(items[i].price);

      price += items[i].price;

      addToCart(items[i]);
      updateCartUI(items[i].name, items[i].price, price);

      if (lv_screen_active() == cartScreen) {
        refreshCartList();
      }

      found = true;
      break;
    }
  }

  if (!found) {
    showItemNotFound();
  }
}

void pollScanner() {
  if (scanner.available()) {
    String code = scanner.readStringUntil('\n');
    code.trim();

    if (code.length() > 0) {
      Serial.print("Scanned: ");
      Serial.println(code);
      processScannedCode(code);
    }
  }

  lv_timer_handler();
  lv_tick_inc(5);
  delay(5);
}
