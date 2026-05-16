// firmware/src/api_client.h
#pragma once
#include <Arduino.h>
#include "display.h"  // for CartList, RecommendationList

struct ScanResult {
  bool   success;
  String productName;
  float  productPrice;
  float  productWeightG;
  float  total;
  String errorMsg;
};

struct CartResponse {
  CartList items;
  float    total;
};

struct DeleteResult {
  bool  success;
  float total;
};

struct WeightVerifyResult {
  bool   ok;
  float  expectedG;
  float  measuredG;
  float  diffG;
  bool   cartChanged;
  String removedName;
  float  total;
  String message;
};

void               apiClient_init(const char* host, int port, const char* cartId);
ScanResult         apiClient_scan(String barcode);
CartResponse       apiClient_getCart();
DeleteResult       apiClient_deleteItem(String barcode);
RecommendationList apiClient_getRecommendations();
void               apiClient_clearCart();
WeightVerifyResult apiClient_verifyWeight(float measuredG, int itemCount);
WeightVerifyResult apiClient_reportWeight(float measuredG, int itemCount, bool autoRemove);
