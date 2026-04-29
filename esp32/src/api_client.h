// firmware/src/api_client.h
#pragma once
#include <Arduino.h>
#include "display.h"  // for RecommendationList

struct ScanResult {
  bool   success;
  String productName;
  float  productPrice;
  float  total;
  String errorMsg;
  
};

void          apiClient_init(const char* host, int port, const char* cartId);
ScanResult    apiClient_scan(String barcode);
RecommendationList apiClient_getRecommendations();
void          apiClient_clearCart();
