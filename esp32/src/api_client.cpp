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

// ── POST /api/cart/scan ──────────────────────────────────────────────────────
ScanResult apiClient_scan(String barcode) {
  ScanResult result = { false, "", 0.0, 0.0, "" };

  HTTPClient http;
  http.begin(baseUrl() + "/api/cart/scan");
  http.addHeader("Content-Type", "application/json");

  String body = "{\"barcode\":\"" + barcode + "\",\"cartId\":\"" + _cartId + "\"}";
  int code = http.POST(body);

  if (code == 200) {
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, http.getString());
    result.success      = true;
    result.productName  = String((const char*)doc["product"]["name"]);
    result.productPrice = doc["product"]["price"].as<float>();
    result.total        = doc["cart"]["total"].as<float>();
  } else if (code == 404) {
    result.errorMsg = "Not found";
  } else {
    result.errorMsg = "HTTP " + String(code);
  }

  http.end();
  return result;
}

// ── GET /api/recommendations ─────────────────────────────────────────────────
RecommendationList apiClient_getRecommendations() {
  RecommendationList recs;

  HTTPClient http;
  http.begin(baseUrl() + "/api/recommendations?cartId=" + _cartId + "&n=4");
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
  http.sendRequest("DELETE");
  http.end();
}

// ── POST /api/cart/verify-weight ─────────────────────────────────────────────
WeightVerifyResult apiClient_verifyWeight(float measuredG, int itemCount) {
  WeightVerifyResult result = { false, 0.0, measuredG, 0.0, "" };

  HTTPClient http;
  http.begin(baseUrl() + "/api/cart/verify-weight");
  http.addHeader("Content-Type", "application/json");

  String body = "{\"measuredG\":" + String(measuredG, 1) +
                ",\"itemCount\":" + String(itemCount) +
                ",\"cartId\":\"" + _cartId + "\"}";
  int code = http.POST(body);

  if (code == 200) {
    DynamicJsonDocument doc(512);
    deserializeJson(doc, http.getString());
    result.ok        = doc["ok"].as<bool>();
    result.expectedG = doc["expectedG"].as<float>();
    result.measuredG = doc["measuredG"].as<float>();
    result.diffG     = doc["diffG"].as<float>();
    result.message   = String((const char*)doc["message"]);
  } else {
    result.message = "HTTP " + String(code);
  }

  http.end();
  return result;
}

// ── GET /api/cart ─────────────────────────────────────────────────────────────
CartResponse apiClient_getCart() {
  CartResponse cr;
  cr.total = 0.0;

  HTTPClient http;
  http.begin(baseUrl() + "/api/cart?cartId=" + _cartId);
  int code = http.GET();

  if (code == 200) {
    DynamicJsonDocument doc(4096);
    deserializeJson(doc, http.getString());
    cr.total = doc["total"].as<float>();
    JsonArray arr = doc["items"].as<JsonArray>();
    for (JsonObject obj : arr) {
      CartItem item;
      item.barcode = String((const char*)obj["barcode"]);
      item.name    = String((const char*)obj["name"]);
      item.price   = obj["price"].as<float>();
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
