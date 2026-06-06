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

// ── Config ────────────────────────────────────────────────────────────────────
const char* WIFI_SSID     = "riya";
const char* WIFI_PASSWORD = "12345678";
const char* BACKEND_IP    = "192.168.137.106";
const int   BACKEND_PORT  = 3001;
const char* CART_ID       = "basket-001";
const char* WEBAPP_URL    = "http://192.168.137.172:5173";

// ── Weight check frequency ────────────────────────────────────────────────────
#define WEIGHT_CHECK_EVERY 1   // check after every N scans

// ── Screen modes ──────────────────────────────────────────────────────────────
//enum Mode { MODE_TOTAL, MODE_CART, MODE_RECS };
Mode currentMode = MODE_TOTAL;
String lastPaymentMessage = "";

// ── State ─────────────────────────────────────────────────────────────────────
float    runningTotal = 0.0;
int      scannedCount = 0;
float    measuredCartWeightG = -1.0;
CartList cartItems;   // local mirror of cart for touch hit-testing

void showStripeQr() {
  if (runningTotal <= 0) {
    display_showStatus("Cart is empty");
    return;
  }

  display_showPayment(runningTotal, "Creating Stripe checkout...");
  CheckoutSessionResult checkout = apiClient_createCheckoutSession();
  if (checkout.success) {
    lastPaymentMessage = "Scan QR to pay";
    currentMode = MODE_PAYMENT;
    display_showCheckoutQr(checkout.url, runningTotal, lastPaymentMessage);
  } else {
    lastPaymentMessage = checkout.errorMsg;
    currentMode = MODE_PAYMENT;
    display_showPayment(runningTotal, lastPaymentMessage);
  }
}

String basketWebUrl() {
  return String(WEBAPP_URL) + "/?cartId=" + String(CART_ID);
}

bool waitForBackendReady() {
  const int maxAttempts = 6;
  int lastCode = 0;

  for (int attempt = 1; attempt <= maxAttempts; attempt++) {
    display_showStatus("Backend check " + String(attempt) + "/" + String(maxAttempts));
    lastCode = apiClient_pingBackend();
    if (lastCode == 200) {
      display_showStatus("Backend online");
      delay(600);
      return true;
    }

    Serial.printf("[API] Backend check failed with code %d; retrying\n", lastCode);
    display_showStatus("Backend failed " + String(lastCode));
    delay(900);
  }

  display_showStatus("Backend failed " + String(lastCode));
  return false;
}

void showBasketQr() {
  currentMode = MODE_BASKET_QR;
  display_showBasketQr(basketWebUrl(), CART_ID);
}

void waitForBasketConnectionOrSkip() {
  apiClient_setBasketConnected(false);
  display_showBasketQr(basketWebUrl(), CART_ID);
  unsigned long lastConnectionPollMs = 0;

  while (true) {
    char qrTap = display_getBasketQrTap();
    if (qrTap == 'N') {
      Serial.println("[Basket] Phone connection skipped on LCD.");
      display_showStatus("Phone connection skipped");
      delay(600);
      return;
    }

    if (millis() - lastConnectionPollMs > 1000) {
      lastConnectionPollMs = millis();
      if (apiClient_isBasketConnected()) {
        Serial.println("[Basket] Phone confirmed basket connection.");
        display_showStatus("Phone connected");
        delay(900);
        return;
      }
    }

    delay(40);
  }
}

// ── Helpers ───────────────────────────────────────────────────────────────────
void setMode(Mode m) {
  currentMode = m;
  if (m == MODE_TOTAL) {
    display_showTotal(runningTotal, measuredCartWeightG);
  } else if (m == MODE_CART) {
    display_showCartList(cartItems, runningTotal, measuredCartWeightG);
  } else if (m == MODE_RECS) {
    RecommendationList recs = apiClient_getRecommendations();
    display_showRecommendations(recs);
  } else if (m == MODE_PAYMENT) {
    display_showPayment(runningTotal, lastPaymentMessage);
  } else if (m == MODE_BASKET_QR) {
    display_showBasketQr(basketWebUrl(), CART_ID);
  }
}

int totalCartQty() {
  int qty = 0;
  for (CartItem& item : cartItems) {
    qty += item.qty;
  }
  return qty;
}

// Fetch full cart from backend and rebuild local cartItems mirror
void refreshCart() {
  CartResponse cr = apiClient_getCart();
  if (!cr.success) {
    Serial.println("[Cart] Refresh failed; keeping current cart on display.");
    return;
  }
  runningTotal = cr.total;
  cartItems    = cr.items;
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(300);

  Serial.println("[WiFi] Scanning nearby networks...");
  int networkCount = WiFi.scanNetworks();
  Serial.printf("[WiFi] Found %d networks\n", networkCount);
  for (int i = 0; i < networkCount; i++) {
    Serial.printf("[WiFi] %s RSSI=%d channel=%d encryption=%d\n",
      WiFi.SSID(i).c_str(),
      WiFi.RSSI(i),
      WiFi.channel(i),
      WiFi.encryptionType(i));
  }
  WiFi.scanDelete();

  Serial.printf("[WiFi] Connecting to SSID: %s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  display_showStatus("Connecting WiFi...");

  unsigned long startMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startMs < 20000) {
    delay(500);
    Serial.printf("[WiFi] status=%d elapsed=%lus\n", WiFi.status(), (millis() - startMs) / 1000);
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("[WiFi] FAILED status=%d. Check SSID/password and 2.4GHz WiFi.\n", WiFi.status());
    display_showStatus("WiFi failed. Check Serial.");
    return;
  }

  WiFi.setSleep(false);
  Serial.print("[WiFi] Connected IP: ");
  Serial.println(WiFi.localIP());
  display_showStatus("WiFi: " + WiFi.localIP().toString());
  delay(800);
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);
  delay(2000);
  Serial.println("Booting WhizCart...");
  display_init();
  Serial.println("Display OK");
  scanner_init();
  Serial.println("Scanner OK");
  Serial.println("About to connect WiFi...");
  connectWiFi();
  Serial.println("WiFi done");
  Serial.println("About to init API...");
  apiClient_init(BACKEND_IP, BACKEND_PORT, CART_ID);
  Serial.println("API OK");
  waitForBackendReady();
  waitForBasketConnectionOrSkip();
  display_showTotal(0.0, measuredCartWeightG);
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
unsigned long lastWeightStatusMs = 0;
String lastWeightEventKey = "";
bool temporaryScreenActive = false;
Mode temporaryReturnMode = MODE_TOTAL;
unsigned long temporaryScreenUntilMs = 0;

void showTemporaryScreen(Mode returnMode, unsigned long durationMs) {
  temporaryScreenActive = true;
  temporaryReturnMode = returnMode;
  temporaryScreenUntilMs = millis() + durationMs;
}

bool handleTemporaryScreen() {
  if (!temporaryScreenActive) return false;

  if (millis() >= temporaryScreenUntilMs) {
    temporaryScreenActive = false;
    setMode(temporaryReturnMode);
    return false;
  }

  return true;
}

void pollWeightStatusIfNeeded() {
  if (millis() - lastWeightStatusMs < 8000) return;
  lastWeightStatusMs = millis();

  WeightVerifyResult status = apiClient_getWeightStatus();
  if (!status.success) return;

  measuredCartWeightG = status.measuredG;
  String eventKey = status.event + "|" + status.message + "|" + String(status.total, 2);
  bool newWeightEvent = eventKey != lastWeightEventKey;
  if (newWeightEvent) {
    lastWeightEventKey = eventKey;
  }

  if (status.event == "pending") {
    if (newWeightEvent) {
      Serial.println("[Weight] Pending: " + status.message);
      display_showStatus(status.message);
    }
    return;
  }

  if (status.cartChanged) {
    Serial.println("[Weight] " + status.message);
    refreshCart();
    lastKnownTotal = runningTotal;
    if (newWeightEvent && status.confirmedName.length() > 0) {
      display_showItem(status.confirmedName, status.confirmedPrice, status.confirmedWeightG, runningTotal);
      currentMode = MODE_CART;
      showTemporaryScreen(MODE_CART, 900);
    } else if (currentMode == MODE_CART) {
      display_showCartList(cartItems, runningTotal, measuredCartWeightG);
    }
    return;
  }

  if (!status.ok && newWeightEvent) {
    Serial.println("[Weight] MISMATCH: " + status.message);
    display_showWeightCheck(status.measuredG, status.expectedG, false);
    showTemporaryScreen(currentMode, 1100);
  }
}

void pollCartIfNeeded() {
  if (millis() - lastPollMs < 8000) return;
  if (millis() - lastWeightStatusMs < 700) return;
  lastPollMs = millis();

  Serial.println("Polling backend...");
  CartResponse cr = apiClient_getCart();
  if (!cr.success) {
    Serial.println("[Cart] Poll failed; keeping current cart on display.");
    return;
  }
  Serial.printf("Got total: %.2f, items: %d\n", cr.total, (int)cr.items.size());

  if (cr.total != lastKnownTotal) {
    lastKnownTotal = cr.total;
    runningTotal   = cr.total;
    cartItems      = cr.items;

    // Show the scanned item name briefly
    if (!cr.items.empty()) {
      CartItem& last = cr.items.back();
      display_showItem(last.name, last.price, last.weightG, cr.total);
      currentMode = MODE_CART;
      showTemporaryScreen(MODE_CART, 900);
      Serial.println("Screen updated with scanned item!");
      return;
    }

    // Then switch to cart list so all items are visible
    currentMode = MODE_CART;
    display_showCartList(cartItems, runningTotal, measuredCartWeightG);
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

  if (handleTemporaryScreen()) {
    delay(10);
    return;
  }

  if (currentMode == MODE_BASKET_QR) {
    static unsigned long lastBasketConnectionPollMs = 0;
    char qrTap = display_getBasketQrTap();
    if (qrTap == 'N' || qrTap == 'B') {
      display_showStatus(qrTap == 'N' ? "Phone connection skipped" : "Back to cart");
      delay(300);
      setMode(MODE_TOTAL);
    } else if (millis() - lastBasketConnectionPollMs > 1000) {
      lastBasketConnectionPollMs = millis();
      if (apiClient_isBasketConnected()) {
        display_showStatus("Phone connected");
        delay(600);
        setMode(MODE_TOTAL);
      }
    }

    delay(10);
    return;
  }

  // ── 1. Barcode scan ──────────────────────────────────────────────────────
  String barcode = scanner_read();
  if (barcode.length() > 0) {
    Serial.println();
    Serial.println("========== BARCODE SCANNED ==========");
    Serial.println("Barcode: " + barcode);
    Serial.println("=====================================");
    display_showStatus("Barcode: " + barcode);
    display_showStatus("Sending to cart...");
    ScanResult result = apiClient_scan(barcode);

    if (result.success) {
      runningTotal = result.total;
      Serial.print("Product found: ");
      Serial.print(result.productName);
      Serial.print("  $");
      Serial.print(result.productPrice, 2);
      Serial.println("  pending weight confirmation");

      display_showPendingItem(result.productName, result.productWeightG);

    } else {
      String msg = result.errorMsg.length() > 0 ? result.errorMsg : "Scan failed";
      Serial.println("Scan failed for " + barcode + ": " + msg);
      display_showStatus("Scan failed: " + msg);
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
      }
      display_showCartList(cartItems, runningTotal, measuredCartWeightG);
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
    else if (nav == 'P') showStripeQr();
    else if (nav == 'Q') showBasketQr();
    else if (nav == 'B') setMode(MODE_TOTAL);
  } else {
    char nav = display_getNavTap(currentMode);
    if (nav == 'B') {
      setMode(MODE_TOTAL);
    } else if (nav == 'P') {
      showStripeQr();
    } else if (nav == 'R') {
      setMode(MODE_RECS);
    } else if (nav == 'C') {
      setMode(MODE_CART);
    } else if (nav == 'Q') {
      showBasketQr();
    }
  }

  if (currentMode == MODE_PAYMENT) {
    char method = display_getPaymentTap();
    if (method != '\0') {
      display_showPayment(runningTotal, "Creating Stripe checkout...");
      CheckoutSessionResult checkout = apiClient_createCheckoutSession();
      if (checkout.success) {
        lastPaymentMessage = "Scan QR to pay";
        display_showCheckoutQr(checkout.url, runningTotal, lastPaymentMessage);
      } else {
        lastPaymentMessage = checkout.errorMsg;
        display_showPayment(runningTotal, lastPaymentMessage);
      }
    }
  }
  pollWeightStatusIfNeeded();
  pollCartIfNeeded();
  delay(10);
}
