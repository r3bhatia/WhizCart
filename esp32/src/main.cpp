// firmware/src/main.cpp
// WhizCart ESP32 Firmware
// Dependencies (install via Arduino Library Manager):
//   - TFT_eSPI  (Bodmer)          → TFT display driver
//   - ArduinoJson (Benoit Blanchon) → parse backend JSON
//   - HX711 (bogde)               → load cell amplifier
//   - WiFi (built-in ESP32 core)

#include <Arduino.h>
#include <WiFi.h>
#include "scanner.h"
#include "display.h"
#include "api_client.h"
#include "scale.h"

// ── Config ──────────────────────────────────────────────────────────────────
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* BACKEND_IP    = "192.168.1.100";  // machine running node server
const int   BACKEND_PORT  = 3001;
const char* CART_ID       = "demo";

// ── Button pins (adjust to your wiring) ─────────────────────────────────────
#define BTN_CLEAR  15   // Clears the entire cart
#define BTN_TOGGLE 16   // Switches display between cart view and recommendations

// ── State ────────────────────────────────────────────────────────────────────
float    runningTotal   = 0.0;
float    expectedWeight = 0.0;  // cumulative expected grams from scanned items
int      scannedCount   = 0;    // total number of items scanned (inc. duplicates)
bool     showRecs       = false;

// Weight check runs every N scans (not every scan, to reduce latency)
#define WEIGHT_CHECK_EVERY 1    // set to 1 to check after every scan; increase if slow

void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  display_showStatus("Connecting WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  display_showStatus("WiFi OK: " + WiFi.localIP().toString());
  delay(1000);
}

void setup() {
  Serial.begin(115200);
  scanner_init();    // UART2 for barcode scanner (see scanner.cpp for pins)
  display_init();    // TFT setup
  scale_init();      // HX711 load cell (tares automatically)
  pinMode(BTN_CLEAR,  INPUT_PULLUP);
  pinMode(BTN_TOGGLE, INPUT_PULLUP);
  connectWiFi();
  apiClient_init(BACKEND_IP, BACKEND_PORT, CART_ID);
  display_showTotal(0.0);
}

void loop() {
  // ── Scan barcode if available ─────────────────────────────────────────────
  String barcode = scanner_read();
  if (barcode.length() > 0) {
    display_showStatus("Scanning...");
    ScanResult result = apiClient_scan(barcode);

    if (result.success) {
      runningTotal = result.total;
      scannedCount++;
      display_showItem(result.productName, result.productPrice, runningTotal);
      delay(1200);

      // ── Weight verification after scan ──────────────────────────────────
      if (scannedCount % WEIGHT_CHECK_EVERY == 0) {
        display_showStatus("Checking weight...");
        float measured = scale_readGrams();
        WeightVerifyResult wr = apiClient_verifyWeight(measured, scannedCount);
        display_showWeightCheck(wr.measuredG, wr.expectedG, wr.ok);
        delay(2500); // hold the weight screen so user can see it
      }

      // Return to total view
      display_showTotal(runningTotal);

    } else {
      display_showStatus("Not found: " + barcode);
      delay(1500);
    }
  }

  // ── Toggle button: cart view <-> recommendations ──────────────────────────
  if (digitalRead(BTN_TOGGLE) == LOW) {
    showRecs = !showRecs;
    delay(300); // debounce
    if (showRecs) {
      RecommendationList recs = apiClient_getRecommendations();
      display_showRecommendations(recs);
    } else {
      display_showTotal(runningTotal);
    }
  }

  // ── Clear cart button ─────────────────────────────────────────────────────
  if (digitalRead(BTN_CLEAR) == LOW) {
    delay(300);
    apiClient_clearCart();
    runningTotal   = 0.0;
    expectedWeight = 0.0;
    scannedCount   = 0;
    showRecs       = false;
    scale_tare();  // re-zero scale with items removed
    display_showTotal(0.0);
    display_showStatus("Cart cleared!");
    delay(1000);
  }

  delay(50);
}
