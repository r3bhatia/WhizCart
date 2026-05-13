// firmware/src/main.cpp
// WhizCart ESP32 Firmware — CYD (ESP32-2432S028R)
//
// Libraries (install via Arduino Library Manager):
//   - TFT_eSPI              by Bodmer
//   - XPT2046_Touchscreen   by Paul Stoffregen
//   - ArduinoJson           by Benoit Blanchon
//   - HX711                 by bogde

#include <Arduino.h>
#include <WiFi.h>
#include "scanner.h"
#include "display.h"
#include "api_client.h"
//#include "scale.h"

// ── Config ────────────────────────────────────────────────────────────────────
const char* WIFI_SSID     = "Suhyun";
const char* WIFI_PASSWORD = "12345678";
const char* BACKEND_IP    = "172.20.10.8";
const int   BACKEND_PORT  = 3001;
const char* CART_ID       = "demo";

// ── Weight check frequency ────────────────────────────────────────────────────
#define WEIGHT_CHECK_EVERY 1   // check after every N scans

// ── Screen modes ──────────────────────────────────────────────────────────────
//enum Mode { MODE_TOTAL, MODE_CART, MODE_RECS };
Mode currentMode = MODE_TOTAL;

// ── State ─────────────────────────────────────────────────────────────────────
float    runningTotal = 0.0;
int      scannedCount = 0;
CartList cartItems;   // local mirror of cart for touch hit-testing

// ── Helpers ───────────────────────────────────────────────────────────────────
void setMode(Mode m) {
  currentMode = m;
  if (m == MODE_TOTAL) {
    display_showTotal(runningTotal);
  } else if (m == MODE_CART) {
    display_showCartList(cartItems, runningTotal);
  } else if (m == MODE_RECS) {
    RecommendationList recs = apiClient_getRecommendations();
    display_showRecommendations(recs);
  }
}

// Fetch full cart from backend and rebuild local cartItems mirror
void refreshCart() {
  CartResponse cr = apiClient_getCart();
  runningTotal = cr.total;
  cartItems    = cr.items;
}

void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  display_showStatus("Connecting WiFi...");
  while (WiFi.status() != WL_CONNECTED) delay(500);
  display_showStatus("WiFi: " + WiFi.localIP().toString());
  delay(800);
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("Booting WhizCart...");
  display_init();
  Serial.println("Display OK");
  scanner_init();
  Serial.println("Scanner OK");
  // scale_init();
  Serial.println("About to connect WiFi...");
  connectWiFi();
  Serial.println("WiFi done");
  Serial.println("About to init API...");
  apiClient_init(BACKEND_IP, BACKEND_PORT, CART_ID);
  Serial.println("API OK");
  display_showTotal(0.0);
  Serial.println("Setup complete");
/*
  Serial.begin(115200);
  display_init();
  scanner_init();
  scale_init();
  connectWiFi();
  apiClient_init(BACKEND_IP, BACKEND_PORT, CART_ID);
  display_showTotal(0.0);
  */
}

unsigned long lastPollMs = 0;
float lastKnownTotal = -1.0;

void pollCartIfNeeded() {
  if (millis() - lastPollMs < 4000) return;
  lastPollMs = millis();

  Serial.println("Polling backend...");
  CartResponse cr = apiClient_getCart();
  Serial.printf("Got total: %.2f, items: %d\n", cr.total, (int)cr.items.size());

  if (cr.total != lastKnownTotal) {
    lastKnownTotal = cr.total;
    runningTotal   = cr.total;
    cartItems      = cr.items;

    // Show the scanned item name briefly
    if (!cr.items.empty()) {
      CartItem& last = cr.items.back();
      display_showItem(last.name, last.price, cr.total);
      delay(2000);  // show item for 2 seconds
    }

    // Then switch to cart list so all items are visible
    currentMode = MODE_CART;
    display_showCartList(cartItems, runningTotal);
    Serial.println("Screen updated with cart list!");
  }
}

// ── Main loop ─────────────────────────────────────────────────────────────────
void loop() {
  static int loopCount = 0;
  loopCount++;
  if (loopCount % 100 == 0) {
    Serial.printf("Loop running, millis=%lu\n", millis());
  }
  // ── 1. Barcode scan ──────────────────────────────────────────────────────
  String barcode = scanner_read();
  if (barcode.length() > 0) {
    display_showStatus("Scanning...");
    ScanResult result = apiClient_scan(barcode);

    if (result.success) {
      runningTotal = result.total;
      scannedCount++;

      // Rebuild local cart mirror so touch delete stays in sync
      refreshCart();

      // Show scanned item briefly
      display_showItem(result.productName, result.productPrice, runningTotal);
      delay(1200);
/*
      // Weight check
      if (scannedCount % WEIGHT_CHECK_EVERY == 0) {
        display_showStatus("Checking weight...");
        float measured = scale_readGrams();
        WeightVerifyResult wr = apiClient_verifyWeight(measured, scannedCount);
        display_showWeightCheck(wr.measuredG, wr.expectedG, wr.ok);
        delay(2200);
      }*/

      // Return to whatever mode was active
      setMode(currentMode);

    } else {
      display_showStatus("Not found: " + barcode);
      delay(1500);
      setMode(currentMode);
    }
  }

  // ── 2. Touch: delete item (only active in cart list mode) ────────────────
  if (currentMode == MODE_CART && cartItems.size() > 0) {
    String tappedBarcode = display_getCartTap(cartItems);
    if (tappedBarcode.length() > 0) {
      display_showStatus("Removing item...");
      DeleteResult dr = apiClient_deleteItem(tappedBarcode);
      if (dr.success) {
        runningTotal = dr.total;
        refreshCart();
        //scale_tare();      // re-zero so scale reflects removed item
      }
      display_showCartList(cartItems, runningTotal);
    }
  }

  // ── 3. Touch: mode toggle buttons (bottom of total screen) ───────────────
  // The total screen shows a hint "Tap [Cart] | [Recs]" at the bottom.
  // We use simple Y-zone taps on the bottom strip (y > 220) to switch modes.
  // Left half → Cart list    Right half → Recommendations
  // Any tap while in cart/recs mode on top-left corner → back to total
  if (currentMode != MODE_CART) {
    // Check for mode-switch taps — handled inside display_getNavTap()
    // (returns 'C' for cart, 'R' for recs, 'B' for back, '\0' for none)
    char nav = display_getNavTap(currentMode);
    if      (nav == 'C') setMode(MODE_CART);
    else if (nav == 'R') setMode(MODE_RECS);
    else if (nav == 'B') setMode(MODE_TOTAL);
  } else {
    // In cart mode: back tap goes to total
    char nav = display_getNavTap(currentMode);
    if (nav == 'B') setMode(MODE_TOTAL);
  }
  pollCartIfNeeded();
  delay(40);
}
