// firmware/src/api_client.cpp
// Uses HTTPClient and ArduinoJson.

#include "api_client.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

static String _host;
static int    _port;
static String _cartId;

String baseUrl() {
  return "http://" + _host + ":" + String(_port);
}
void apiClient_init(const char* host, int port, const char* cartId) {
  _host   = String(host);
  _port   = port;
  _cartId = String(cartId);

  Serial.println("API client initialized");
  Serial.print("Backend base URL: ");
  Serial.println(baseUrl());
  Serial.print("Cart ID: ");
  Serial.println(_cartId);
}


String cleanApiBarcode(String barcode) {
  barcode.trim();
  barcode.replace("[", "");
  barcode.replace("]", "");
  barcode.replace("\r", "");
  barcode.replace("\n", "");
  barcode.replace("\t", "");
  barcode.replace(" ", "");
  return barcode;
}

// ── POST /api/cart/scan ──────────────────────────────────────────────────────
ScanResult apiClient_scan(String barcode) {
  ScanResult result = { false, "", 0.0, 0.0, "" };

  barcode = cleanApiBarcode(barcode);

  if (barcode.length() == 0) {
    result.errorMsg = "Empty barcode";
    Serial.println("Scan failed: barcode is empty after cleaning");
    return result;
  }

  HTTPClient http;

  String url = baseUrl() + "/api/cart/scan";
  String body = "{\"barcode\":\"" + barcode + "\",\"cartId\":\"" + _cartId + "\"}";

  Serial.println("----- API SCAN REQUEST -----");
  Serial.print("POST URL: ");
  Serial.println(url);
  Serial.print("POST body: ");
  Serial.println(body);

  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  int code = http.POST(body);
  String response = http.getString();

  Serial.print("Scan HTTP code: ");
  Serial.println(code);
  Serial.print("Scan response body: ");
  Serial.println(response);
  Serial.println("----------------------------");

  if (code == 200) {
    DynamicJsonDocument doc(2048);
    DeserializationError err = deserializeJson(doc, response);

    if (err) {
      result.errorMsg = "JSON parse error";
      Serial.print("JSON parse error: ");
      Serial.println(err.c_str());
      http.end();
      return result;
    }

    result.success      = true;
    result.productName  = String((const char*)doc["product"]["name"]);
    result.productPrice = doc["product"]["price"].as<float>();
    result.total        = doc["cart"]["total"].as<float>();

    Serial.println("Scan parsed successfully");
    Serial.print("Product name: ");
    Serial.println(result.productName);
    Serial.print("Product price: ");
    Serial.println(result.productPrice, 2);
    Serial.print("Cart total: ");
    Serial.println(result.total, 2);

  } else if (code == 404) {
    result.errorMsg = "Product not found";
    Serial.println("Backend says product was not found.");
    Serial.print("Check mock_db/products.js for barcode: ");
    Serial.println(barcode);

  } else if (code <= 0) {
    result.errorMsg = "Connection failed";
    Serial.println("HTTP connection failed.");
    Serial.print("HTTPClient code: ");
    Serial.println(code);

  } else {
    result.errorMsg = "HTTP " + String(code);
    Serial.print("Scan failed with HTTP code: ");
    Serial.println(code);
  }

  http.end();
  return result;
}

// ── GET /api/recommendations ─────────────────────────────────────────────────
RecommendationList apiClient_getRecommendations() {
  RecommendationList recs;

  HTTPClient http;
  String url = baseUrl() + "/api/recommendations?cartId=" + _cartId + "&n=4";

  Serial.print("GET recommendations: ");
  Serial.println(url);

  http.begin(url);
  int code = http.GET();
  String response = http.getString();

  Serial.print("Recommendations HTTP code: ");
  Serial.println(code);

  if (code == 200) {
    DynamicJsonDocument doc(2048);
    DeserializationError err = deserializeJson(doc, response);

    if (!err) {
      JsonArray arr = doc.as<JsonArray>();

      for (JsonObject obj : arr) {
        RecItem item;
        item.name  = String((const char*)obj["name"]);
        item.price = obj["price"].as<float>();
        item.aisle = String((const char*)obj["aisle"]);
        recs.push_back(item);
      }
    } else {
      Serial.print("Recommendations JSON parse error: ");
      Serial.println(err.c_str());
    }
  } else {
    Serial.print("Recommendations failed. Response: ");
    Serial.println(response);
  }

  http.end();
  return recs;
}

// ── DELETE /api/cart ─────────────────────────────────────────────────────────
void apiClient_clearCart() {
  HTTPClient http;

  String url = baseUrl() + "/api/cart?cartId=" + _cartId;

  Serial.print("DELETE cart: ");
  Serial.println(url);

  http.begin(url);
  int code = http.sendRequest("DELETE");
  String response = http.getString();

  Serial.print("Clear cart HTTP code: ");
  Serial.println(code);
  Serial.print("Clear cart response: ");
  Serial.println(response);

  http.end();
}

// ── POST /api/cart/verify-weight ─────────────────────────────────────────────
WeightVerifyResult apiClient_verifyWeight(float measuredG, int itemCount) {
  WeightVerifyResult result = { false, 0.0, measuredG, 0.0, "" };

  HTTPClient http;

  String url = baseUrl() + "/api/cart/verify-weight";

  String body = "{\"measuredG\":" + String(measuredG, 1) +
                ",\"itemCount\":" + String(itemCount) +
                ",\"cartId\":\"" + _cartId + "\"}";

  Serial.println("POST verify weight:");
  Serial.println(url);
  Serial.println(body);

  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  int code = http.POST(body);
  String response = http.getString();

  Serial.print("Verify weight HTTP code: ");
  Serial.println(code);
  Serial.print("Verify weight response: ");
  Serial.println(response);

  if (code == 200) {
    DynamicJsonDocument doc(512);
    DeserializationError err = deserializeJson(doc, response);

    if (!err) {
      result.ok        = doc["ok"].as<bool>();
      result.expectedG = doc["expectedG"].as<float>();
      result.measuredG = doc["measuredG"].as<float>();
      result.diffG     = doc["diffG"].as<float>();
      result.message   = String((const char*)doc["message"]);
    } else {
      result.message = "JSON parse error";
      Serial.print("Verify weight JSON parse error: ");
      Serial.println(err.c_str());
    }

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

  String url = baseUrl() + "/api/cart?cartId=" + _cartId;

  http.begin(url);
  int code = http.GET();
  String response = http.getString();

  if (code == 200) {
    DynamicJsonDocument doc(4096);
    DeserializationError err = deserializeJson(doc, response);

    if (!err) {
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
    } else {
      Serial.print("Cart JSON parse error: ");
      Serial.println(err.c_str());
    }

  } else {
    Serial.print("Get cart failed. HTTP code: ");
    Serial.println(code);
    Serial.print("Get cart response: ");
    Serial.println(response);
  }

  http.end();
  return cr;
}

// ── DELETE /api/cart/item ────────────────────────────────────────────────────
DeleteResult apiClient_deleteItem(String barcode) {
  DeleteResult result = { false, 0.0 };

  barcode = cleanApiBarcode(barcode);

  HTTPClient http;

  String url = baseUrl() + "/api/cart/item";
  String body = "{\"barcode\":\"" + barcode + "\",\"cartId\":\"" + _cartId + "\"}";

  Serial.println("DELETE cart item:");
  Serial.println(url);
  Serial.println(body);

  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  int code = http.sendRequest("DELETE", body);
  String response = http.getString();

  Serial.print("Delete HTTP code: ");
  Serial.println(code);
  Serial.print("Delete response: ");
  Serial.println(response);

  if (code == 200) {
    DynamicJsonDocument doc(512);
    DeserializationError err = deserializeJson(doc, response);

    if (!err) {
      result.success = true;
      result.total   = doc["total"].as<float>();
    } else {
      Serial.print("Delete JSON parse error: ");
      Serial.println(err.c_str());
    }
  }

  http.end();
  return result;
}