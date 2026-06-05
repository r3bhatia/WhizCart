// firmware/src/api_client.h
#pragma once
#include <Arduino.h>
#include "display.h"  // for CartList, RecommendationList

struct ScanResult {
  bool   success;
  bool   pending;
  String productName;
  float  productPrice;
  float  productWeightG;
  float  total;
  String errorMsg;
  String message;
};

struct CartResponse {
  CartList items;
  float    total;
  bool     success;
};

struct DeleteResult {
  bool  success;
  float total;
};

struct WeightVerifyResult {
  bool   success;
  bool   ok;
  float  expectedG;
  float  measuredG;
  float  diffG;
  bool   cartChanged;
  String removedName;
  String confirmedName;
  float  confirmedPrice;
  float  confirmedWeightG;
  String rejectedName;
  String event;
  float  total;
  String message;
};

struct PaymentResult {
  bool success;
  String method;
  float total;
  String message;
};

struct CheckoutSessionResult {
  bool success;
  String url;
  String sessionId;
  String mode;
  String errorMsg;
};

void               apiClient_init(const char* host, int port, const char* cartId);
ScanResult         apiClient_scan(String barcode);
CartResponse       apiClient_getCart();
DeleteResult       apiClient_deleteItem(String barcode);
RecommendationList apiClient_getRecommendations();
void               apiClient_clearCart();
WeightVerifyResult apiClient_verifyWeight(float measuredG, int itemCount);
WeightVerifyResult apiClient_reportWeight(float measuredG, int itemCount, bool autoRemove);
WeightVerifyResult apiClient_getWeightStatus();
PaymentResult      apiClient_checkout(String method);
CheckoutSessionResult apiClient_createCheckoutSession();
bool                  apiClient_isBasketConnected();
void                  apiClient_setBasketConnected(bool connected);
