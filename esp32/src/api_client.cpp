// firmware/src/api_client.cpp
// Uses HTTPClient (built-in ESP32 core) and ArduinoJson.

#include "api_client.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

static String _host;
static int    _port;
static String _cartId;

void apiClient_init(const char* host, int port, const char* cartId) {
  _host   = String(host);
  _port   = port;
  _cartId = String(cartId);
}

String baseUrl() {
  return "http://" + _host + ":" + String(_port);
}

void applyHttpTimeouts(HTTPClient& http) {
  http.setTimeout(2000);
  http.setReuse(false);
}

void applyStartupHttpTimeouts(HTTPClient& http) {
  http.setTimeout(3000);
  http.setReuse(false);
}

String httpFailureMessage(int code) {
  if (code >= 0) return "HTTP " + String(code);
  return "Backend offline (" + String(code) + ")";
}

int apiClient_pingBackend() {
  HTTPClient http;
  String url = baseUrl() + "/";
  http.begin(url);
  applyStartupHttpTimeouts(http);
  Serial.println("[API] GET " + url);
  int code = http.GET();
  Serial.printf("[API] Backend ping HTTP code: %d\n", code);
  if (code > 0) {
    Serial.println("[API] Backend ping response: " + http.getString());
  }
  http.end();
  return code;
}

// ── POST /api/cart/scan ──────────────────────────────────────────────────────
ScanResult apiClient_scan(String barcode) {
  ScanResult result = { false, false, "", 0.0, 0.0, 0.0, "", "" };

  HTTPClient http;
  String url = baseUrl() + "/api/cart/scan";
  http.begin(url);
  applyHttpTimeouts(http);
  http.addHeader("Content-Type", "application/json");

  String body = "{\"barcode\":\"" + barcode + "\",\"cartId\":\"" + _cartId + "\"}";
  Serial.println("[API] POST " + url);
  Serial.println("[API] Body: " + body);
  int code = http.POST(body);
  String payload = http.getString();
  Serial.printf("[API] Scan HTTP code: %d\n", code);
  Serial.println("[API] Scan response: " + payload);

  if (code == 200) {
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    result.success      = true;
    result.pending      = doc["pending"].as<bool>();
    result.productName  = String((const char*)doc["product"]["name"]);
    result.productPrice = doc["product"]["price"].as<float>();
    result.productWeightG = doc["product"]["weightG"].as<float>();
    result.total        = doc["cart"]["total"].as<float>();
    if (!doc["message"].isNull()) {
      result.message = String((const char*)doc["message"]);
    }
  } else if (code == 409) {
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    if (!error && !doc["error"].isNull()) {
      result.errorMsg = String((const char*)doc["error"]);
    } else {
      result.errorMsg = "Finish pending item first";
    }
  } else if (code == 404) {
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, payload);
    if (!error && !doc["barcode"].isNull()) {
      result.errorMsg = "Not in mock_db: " + String((const char*)doc["barcode"]);
    } else {
      result.errorMsg = "Not in mock_db: " + barcode;
    }
  } else {
    result.errorMsg = httpFailureMessage(code);
  }

  http.end();
  return result;
}

// ── GET /api/recommendations ─────────────────────────────────────────────────
RecommendationList apiClient_getRecommendations() {
  RecommendationList recs;

  HTTPClient http;
  http.begin(baseUrl() + "/api/recommendations?cartId=" + _cartId + "&n=4");
  applyHttpTimeouts(http);
  int code = http.GET();

  if (code == 200) {
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, http.getString());
    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject obj : arr) {
      RecItem item;
      item.name  = String((const char*)obj["name"]);
      item.price = obj["price"].as<float>();
      item.aisle = String((const char*)obj["aisle"]);
      recs.push_back(item);
    }
  }

  http.end();
  return recs;
}

// ── DELETE /api/cart ─────────────────────────────────────────────────────────
void apiClient_clearCart() {
  HTTPClient http;
  http.begin(baseUrl() + "/api/cart?cartId=" + _cartId);
  applyHttpTimeouts(http);
  http.sendRequest("DELETE");
  http.end();
}

PaymentResult apiClient_checkout(String method) {
  PaymentResult result = { false, method, 0.0, "Checkout failed" };

  HTTPClient http;
  String url = baseUrl() + "/api/cart/checkout";
  http.begin(url);
  applyHttpTimeouts(http);
  http.addHeader("Content-Type", "application/json");

  String body = "{\"method\":\"" + method + "\",\"cartId\":\"" + _cartId + "\"}";
  Serial.println("[API] POST " + url);
  Serial.println("[API] Body: " + body);
  int code = http.POST(body);
  String payload = http.getString();
  Serial.printf("[API] Checkout HTTP code: %d\n", code);
  Serial.println("[API] Checkout response: " + payload);

  if (code == 200 || code == 400) {
    DynamicJsonDocument doc(768);
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      result.success = doc["success"].as<bool>();
      result.method = String((const char*)doc["method"]);
      result.total = doc["total"].as<float>();
      result.message = String((const char*)doc["message"]);
    } else {
      result.message = "Bad checkout response";
    }
  } else {
    result.message = "HTTP " + String(code);
  }

  http.end();
  return result;
}

CheckoutSessionResult apiClient_createCheckoutSession() {
  CheckoutSessionResult result = { false, "", "", "", "Checkout failed" };

  HTTPClient http;
  String url = baseUrl() + "/api/checkout/session";
  http.begin(url);
  applyHttpTimeouts(http);
  http.addHeader("Content-Type", "application/json");

  String body = "{\"cartId\":\"" + _cartId + "\"}";
  Serial.println("[API] POST " + url);
  Serial.println("[API] Body: " + body);
  int code = http.POST(body);
  String payload = http.getString();
  Serial.printf("[API] Stripe session HTTP code: %d\n", code);
  Serial.println("[API] Stripe session response: " + payload);

  if (code == 200) {
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      result.success = true;
      result.url = String((const char*)doc["url"]);
      result.sessionId = String((const char*)doc["sessionId"]);
      result.mode = String((const char*)doc["mode"]);
      result.errorMsg = "";
    } else {
      result.errorMsg = "Bad Stripe response";
    }
  } else {
    if (code < 0) {
      result.errorMsg = httpFailureMessage(code);
      http.end();
      return result;
    }
      DynamicJsonDocument doc(1536);
    DeserializationError error = deserializeJson(doc, payload);
    if (!error && !doc["error"].isNull()) {
      result.errorMsg = String((const char*)doc["error"]);
    } else {
      result.errorMsg = "HTTP " + String(code);
    }
  }

  http.end();
  return result;
}

// ── POST /api/cart/verify-weight ─────────────────────────────────────────────
WeightVerifyResult apiClient_verifyWeight(float measuredG, int itemCount) {
  return apiClient_reportWeight(measuredG, itemCount, false);
}

bool apiClient_isBasketConnected() {
  HTTPClient http;
  String url = baseUrl() + "/api/cart/connection-status?cartId=" + _cartId;
  http.begin(url);
  applyHttpTimeouts(http);
  int code = http.GET();

  bool connected = false;
  if (code == 200) {
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, http.getString());
    if (!error) {
      connected = doc["connected"].as<bool>();
    }
  }

  http.end();
  return connected;
}

void apiClient_setBasketConnected(bool connected) {
  HTTPClient http;
  String url = baseUrl() + "/api/cart/connect";
  http.begin(url);
  applyHttpTimeouts(http);
  http.addHeader("Content-Type", "application/json");

  String body = "{\"cartId\":\"" + _cartId +
                "\",\"connected\":" + String(connected ? "true" : "false") +
                "}";
  int code = http.POST(body);
  Serial.printf("[API] Basket connection reset HTTP code: %d\n", code);
  http.end();
}

WeightVerifyResult apiClient_reportWeight(float measuredG, int itemCount, bool autoRemove) {
  WeightVerifyResult result = { false, false, 0.0, measuredG, 0.0, false, "", "", 0.0, 0.0, "", "", 0.0, "" };

  HTTPClient http;
  http.begin(baseUrl() + "/api/cart/verify-weight");
  applyHttpTimeouts(http);
  http.addHeader("Content-Type", "application/json");

  String body = "{\"measuredG\":" + String(measuredG, 1) +
                ",\"itemCount\":" + String(itemCount) +
                ",\"autoRemove\":" + String(autoRemove ? "true" : "false") +
                ",\"cartId\":\"" + _cartId + "\"}";
  int code = http.POST(body);

  if (code == 200) {
    DynamicJsonDocument doc(1536);
    deserializeJson(doc, http.getString());
    result.success   = true;
    result.ok        = doc["ok"].as<bool>();
    result.expectedG = doc["expectedG"].as<float>();
    result.measuredG = doc["measuredG"].as<float>();
    result.diffG     = doc["diffG"].as<float>();
      result.cartChanged = doc["cartChanged"].as<bool>();
      result.total = doc["total"].as<float>();
      result.event = String((const char*)doc["event"]);
      if (!doc["removedItem"].isNull()) {
        result.removedName = String((const char*)doc["removedItem"]["name"]);
      }
      if (!doc["confirmedItem"].isNull()) {
        result.confirmedName = String((const char*)doc["confirmedItem"]["name"]);
        result.confirmedPrice = doc["confirmedItem"]["price"].as<float>();
        result.confirmedWeightG = doc["confirmedItem"]["weightG"].as<float>();
      }
      if (!doc["rejectedItem"].isNull()) {
        result.rejectedName = String((const char*)doc["rejectedItem"]["name"]);
      }
      result.message   = String((const char*)doc["message"]);
  } else {
    result.message = "HTTP " + String(code);
  }

  http.end();
  return result;
}

WeightVerifyResult apiClient_getWeightStatus() {
  WeightVerifyResult result = { false, false, 0.0, 0.0, 0.0, false, "", "", 0.0, 0.0, "", "", 0.0, "" };

  HTTPClient http;
  String url = baseUrl() + "/api/cart/weight-status?cartId=" + _cartId;
  http.begin(url);
  applyHttpTimeouts(http);
  int code = http.GET();

  if (code == 200) {
      DynamicJsonDocument doc(1536);
    DeserializationError error = deserializeJson(doc, http.getString());
    if (!error) {
      result.success = true;
      result.ok = doc["ok"].as<bool>();
      result.expectedG = doc["expectedG"].as<float>();
      result.measuredG = doc["measuredG"].as<float>();
      result.diffG = doc["diffG"].as<float>();
      result.cartChanged = doc["cartChanged"].as<bool>();
      result.total = doc["total"].as<float>();
      result.event = String((const char*)doc["event"]);
      if (!doc["removedItem"].isNull()) {
        result.removedName = String((const char*)doc["removedItem"]["name"]);
      }
      if (!doc["confirmedItem"].isNull()) {
        result.confirmedName = String((const char*)doc["confirmedItem"]["name"]);
        result.confirmedPrice = doc["confirmedItem"]["price"].as<float>();
        result.confirmedWeightG = doc["confirmedItem"]["weightG"].as<float>();
      }
      if (!doc["rejectedItem"].isNull()) {
        result.rejectedName = String((const char*)doc["rejectedItem"]["name"]);
      }
      result.message = String((const char*)doc["message"]);
    }
  }

  http.end();
  return result;
}

// ── GET /api/cart ─────────────────────────────────────────────────────────────
CartResponse apiClient_getCart() {
  CartResponse cr;
  cr.total = 0.0;
  cr.success = false;

  HTTPClient http;
  String url = baseUrl() + "/api/cart?cartId=" + _cartId;
  http.begin(url);
  applyHttpTimeouts(http);
  int code = http.GET();
  Serial.println("[API] GET " + url);
  Serial.printf("[API] Cart HTTP code: %d\n", code);

  if (code == 200) {
    DynamicJsonDocument doc(4096);
    deserializeJson(doc, http.getString());
    cr.success = true;
    cr.total = doc["total"].as<float>();
    JsonArray arr = doc["items"].as<JsonArray>();
    for (JsonObject obj : arr) {
      CartItem item;
      item.barcode = String((const char*)obj["barcode"]);
      item.name    = String((const char*)obj["name"]);
      item.price   = obj["price"].as<float>();
      item.weightG = obj["weightG"].as<float>();
      item.qty     = obj["qty"].as<int>();
      cr.items.push_back(item);
    }
  }

  http.end();
  return cr;
}

// ── DELETE /api/cart/item ────────────────────────────────────────────────────
DeleteResult apiClient_deleteItem(String barcode) {
  DeleteResult result = { false, 0.0 };

  HTTPClient http;
  http.begin(baseUrl() + "/api/cart/item");
  applyHttpTimeouts(http);
  http.addHeader("Content-Type", "application/json");

  String body = "{\"barcode\":\"" + barcode + "\",\"cartId\":\"" + _cartId + "\"}";
  int code = http.sendRequest("DELETE", body);

  if (code == 200) {
    DynamicJsonDocument doc(512);
    deserializeJson(doc, http.getString());
    result.success = true;
    result.total   = doc["total"].as<float>();
  }

  http.end();
  return result;
}
