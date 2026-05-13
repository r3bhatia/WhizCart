// firmware/src/main.cpp
// WhizCart ESP32 Firmware — CYD (ESP32-2432S028R)
//
// Libraries:
//   - TFT_eSPI
//   - XPT2046_Touchscreen
//   - ArduinoJson
//   - HX711, optional if scale is enabled

#include <Arduino.h>
#include <WiFi.h>

#include "scanner.h"
#include "display.h"
#include "api_client.h"
// #include "scale.h"

// ── Config ────────────────────────────────────────────────────────────────────
// Keep your existing WiFi/backend values here.
const char* WIFI_SSID     = "iPhone";
const char* WIFI_PASSWORD = "freshboi";

const char* BACKEND_IP    = "172.20.10.5";
const int   BACKEND_PORT  = 3001;
const char* CART_ID       = "demo";

// ── Weight check frequency ────────────────────────────────────────────────────
#define WEIGHT_CHECK_EVERY 1

// ── Screen mode ───────────────────────────────────────────────────────────────
Mode currentMode = MODE_TOTAL;

// ── State ─────────────────────────────────────────────────────────────────────
float runningTotal = 0.0;
int scannedCount = 0;

CartList cartItems;

// Polling state
unsigned long lastPollMs = 0;
float lastKnownTotal = -1.0;
int lastKnownItemCount = -1;

// ── Helpers ───────────────────────────────────────────────────────────────────
String cleanBarcode(String barcode) {
  barcode.trim();

  // In case scanner output contains formatting characters
  barcode.replace("[", "");
  barcode.replace("]", "");
  barcode.replace("\r", "");
  barcode.replace("\n", "");
  barcode.replace("\t", "");
  barcode.replace(" ", "");

  return barcode;
}

int getCartItemCount(const CartList& items) {
  int count = 0;

  for (int i = 0; i < (int)items.size(); i++) {
    count += items[i].qty;
  }

  return count;
}

void refreshCart() {
  CartResponse cr = apiClient_getCart();

  runningTotal = cr.total;
  cartItems = cr.items;

  lastKnownTotal = runningTotal;
  lastKnownItemCount = getCartItemCount(cartItems);

  Serial.printf(
    "Cart refreshed: total=%.2f, items=%d\n",
    runningTotal,
    lastKnownItemCount
  );
}

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

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  display_showStatus("Connecting WiFi...");
  Serial.print("Connecting to WiFi");

  unsigned long startMs = millis();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");

    if (millis() - startMs > 20000) {
      Serial.println();
      Serial.println("WiFi connection timed out. Retrying...");
      display_showStatus("WiFi retrying...");

      WiFi.disconnect();
      delay(1000);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      startMs = millis();
    }
  }

  Serial.println();
  Serial.print("WiFi connected. ESP32 IP: ");
  Serial.println(WiFi.localIP());

  display_showStatus("WiFi: " + WiFi.localIP().toString());
  delay(800);
}

void ensureWiFiConnected() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.println("WiFi disconnected. Reconnecting...");
  display_showStatus("WiFi reconnecting...");
  connectWiFi();
}

// ── Poll backend so LCD stays synced with webapp/backend ──────────────────────
void pollCartIfNeeded() {
  if (millis() - lastPollMs < 4000) return;

  lastPollMs = millis();

  Serial.println("Polling backend...");

  CartResponse cr = apiClient_getCart();

  int itemCount = getCartItemCount(cr.items);

  Serial.printf(
    "Got total: %.2f, items: %d\n",
    cr.total,
    itemCount
  );

  bool changed =
    cr.total != lastKnownTotal ||
    itemCount != lastKnownItemCount;

  if (!changed) return;

  runningTotal = cr.total;
  cartItems = cr.items;
  lastKnownTotal = cr.total;
  lastKnownItemCount = itemCount;

  if (currentMode == MODE_TOTAL) {
    display_showTotal(runningTotal);
  } else if (currentMode == MODE_CART) {
    display_showCartList(cartItems, runningTotal);
  } else if (currentMode == MODE_RECS) {
    RecommendationList recs = apiClient_getRecommendations();
    display_showRecommendations(recs);
  }

  Serial.println("Screen synced with backend.");
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("Booting WhizCart...");

  display_init();
  Serial.println("Display OK");

  scanner_init();
  Serial.println("Scanner OK");

  // scale_init();
  // Serial.println("Scale OK");

  connectWiFi();

  Serial.println("Initializing API client...");
  apiClient_init(BACKEND_IP, BACKEND_PORT, CART_ID);
  Serial.println("API OK");

  refreshCart();

  display_showTotal(runningTotal);

  Serial.println("Setup complete.");
}

// ── Main loop ─────────────────────────────────────────────────────────────────
void loop() {
  ensureWiFiConnected();

  // Keep LVGL responsive by checking nav/touch every loop.
  char nav = display_getNavTap(currentMode);

  if (nav == 'C') {
    refreshCart();
    setMode(MODE_CART);
  } else if (nav == 'R') {
    setMode(MODE_RECS);
  } else if (nav == 'B') {
    setMode(MODE_TOTAL);
  }

  // ── Delete item from cart mode ──────────────────────────────────────────────
  if (currentMode == MODE_CART && cartItems.size() > 0) {
    String tappedBarcode = display_getCartTap(cartItems);

    if (tappedBarcode.length() > 0) {
      Serial.print("Delete tapped for barcode: ");
      Serial.println(tappedBarcode);

      display_showStatus("Removing item...");

      DeleteResult dr = apiClient_deleteItem(tappedBarcode);

      if (dr.success) {
        runningTotal = dr.total;
        refreshCart();
        display_showCartList(cartItems, runningTotal);
        Serial.println("Item removed.");
      } else {
        display_showStatus("Remove failed");
        Serial.println("Delete failed.");
      }
    }
  }

  // ── Barcode scan ────────────────────────────────────────────────────────────
  String rawBarcode = scanner_read();

  if (rawBarcode.length() > 0) {
    Serial.print("Scanner raw read: [");
    Serial.print(rawBarcode);
    Serial.println("]");

    String barcode = cleanBarcode(rawBarcode);

    Serial.print("Clean barcode: [");
    Serial.print(barcode);
    Serial.println("]");

    if (barcode.length() == 0) {
      display_showStatus("Invalid barcode");
      delay(800);
      setMode(currentMode);
      return;
    }

    display_showStatus("Adding item...");

    Serial.println("Sending barcode to backend through apiClient_scan...");
    ScanResult result = apiClient_scan(barcode);

    if (result.success) {
      Serial.println("Scan successful.");
      Serial.print("Product: ");
      Serial.println(result.productName);
      Serial.print("Price: ");
      Serial.println(result.productPrice, 2);
      Serial.print("New total: ");
      Serial.println(result.total, 2);

      runningTotal = result.total;
      scannedCount++;

      // Pull backend cart immediately so local LCD state matches webapp state.
      refreshCart();

      // Show confirmation screen briefly.
      display_showItem(result.productName, result.productPrice, runningTotal);
      delay(1200);

      /*
      // Optional weight check
      if (scannedCount % WEIGHT_CHECK_EVERY == 0) {
        display_showStatus("Checking weight...");
        float measured = scale_readGrams();
        WeightVerifyResult wr = apiClient_verifyWeight(measured, scannedCount);
        display_showWeightCheck(wr.measuredG, wr.expectedG, wr.ok);
        delay(2200);
      }
      */

      // After scan, show the cart list so the user sees the item was added.
      currentMode = MODE_CART;
      display_showCartList(cartItems, runningTotal);
    } else {
      Serial.println("Scan failed.");
      Serial.print("Barcode not found or backend error: ");
      Serial.println(barcode);

      display_showStatus("Not found: " + barcode);
      delay(1500);

      setMode(currentMode);
    }
  }

  // Keep backend/webapp/LCD synced.
  pollCartIfNeeded();

  delay(20);
}