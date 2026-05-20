// firmware/src/loadcell_node.cpp
// Standalone ESP32 firmware for WhizCart load-cell/HX711 weight reporting.

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <HX711_ADC.h>
#include <math.h>

#if defined(ESP8266) || defined(ESP32) || defined(AVR)
#include <EEPROM.h>
#endif

const char* WIFI_SSID = "Insert new here";
const char* WIFI_PASSWORD = "Insert new here";
const char* BACKEND_IP = "Insert new here";
const int BACKEND_PORT = 3001;
const char* CART_ID = "demo";

const int HX711_DOUT = 4;
const int HX711_SCK = 5;
const int CAL_VALUE_EEPROM_ADDRESS = 0;
const float FALLBACK_CALIBRATION_VALUE = 696.0;

const float noiseClamp = 5.0;
const float itemThreshold = 50.0;
const float emaAlpha = 0.25;
const float stableBand = 15.0;
const unsigned long stableWindowMs = 800;
const unsigned long quickEventTimeoutMs = 1800;
const unsigned long periodicReportMs = 5000;

HX711_ADC LoadCell(HX711_DOUT, HX711_SCK);

float filteredWeight = 0;
bool filterReady = false;
float confirmedBasketWeight = 0;
float stableMin = 0;
float stableMax = 0;
unsigned long stableWindowStart = 0;
bool pendingEvent = false;
float pendingPeakChange = 0;
unsigned long pendingEventStart = 0;
unsigned long lastReportMs = 0;

float clampNoise(float value) {
  if (value < 0) return 0;
  if (abs(value) < noiseClamp) return 0;
  return value;
}

void resetBasketState() {
  filteredWeight = 0;
  filterReady = false;
  confirmedBasketWeight = 0;
  stableMin = 0;
  stableMax = 0;
  stableWindowStart = millis();
  pendingEvent = false;
  pendingPeakChange = 0;
  pendingEventStart = 0;
}

float readCalibrationValue() {
  float calibrationValue = FALLBACK_CALIBRATION_VALUE;

#if defined(ESP8266) || defined(ESP32)
  EEPROM.begin(512);
#endif

#if defined(ESP8266) || defined(ESP32) || defined(AVR)
  EEPROM.get(CAL_VALUE_EEPROM_ADDRESS, calibrationValue);
#endif

  if (!isfinite(calibrationValue) || calibrationValue == 0) {
    calibrationValue = FALLBACK_CALIBRATION_VALUE;
  }

  return calibrationValue;
}

String baseUrl() {
  return "http://" + String(BACKEND_IP) + ":" + String(BACKEND_PORT);
}

void connectWiFi() {
  Serial.print("[WiFi] Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("[WiFi] Connected. IP: ");
  Serial.println(WiFi.localIP());
}

void reportWeight(float measuredG, bool autoRemove) {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  HTTPClient http;
  String url = baseUrl() + "/api/cart/verify-weight";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  String body = "{\"measuredG\":" + String(measuredG, 1) +
                ",\"autoRemove\":" + String(autoRemove ? "true" : "false") +
                ",\"cartId\":\"" + String(CART_ID) + "\"}";

  Serial.println("[Scale API] POST " + url);
  Serial.println("[Scale API] Body: " + body);
  int code = http.POST(body);
  String payload = http.getString();
  Serial.printf("[Scale API] HTTP code: %d\n", code);
  Serial.println("[Scale API] Response: " + payload);
  http.end();
  lastReportMs = millis();
}

void initScale() {
  LoadCell.begin();
  unsigned long stabilizingtime = 2000;
  boolean tare = true;

  LoadCell.start(stabilizingtime, tare);

  if (LoadCell.getTareTimeoutFlag() || LoadCell.getSignalTimeoutFlag()) {
    Serial.println("[Scale] Timeout. Check ESP32-to-HX711 wiring.");
    while (true) delay(1000);
  }

  float calibrationValue = readCalibrationValue();
  LoadCell.setCalFactor(calibrationValue);

  while (!LoadCell.update()) {
    delay(1);
  }

  resetBasketState();
  Serial.print("[Scale] Initialized. Calibration value: ");
  Serial.println(calibrationValue);
  Serial.println("[Scale] Serial commands: t=tare, p=post current weight");
}

bool updateStableWeight(float& newWeightG, float& changeG) {
  if (!LoadCell.update()) return false;

  unsigned long now = millis();
  float rawWeight = LoadCell.getData();

  if (!filterReady) {
    filteredWeight = rawWeight;
    filterReady = true;
    stableMin = filteredWeight;
    stableMax = filteredWeight;
    stableWindowStart = now;
  } else {
    filteredWeight = filteredWeight + emaAlpha * (rawWeight - filteredWeight);
  }

  float currentWeight = clampNoise(filteredWeight);
  float changeFromConfirmed = currentWeight - confirmedBasketWeight;
  float absChange = abs(changeFromConfirmed);

  if (absChange > itemThreshold) {
    if (!pendingEvent) {
      pendingEvent = true;
      pendingEventStart = now;
      pendingPeakChange = absChange;
    } else if (absChange > pendingPeakChange) {
      pendingPeakChange = absChange;
    }
  }

  if (currentWeight < stableMin) stableMin = currentWeight;
  if (currentWeight > stableMax) stableMax = currentWeight;

  if (now - stableWindowStart < stableWindowMs) return false;

  bool changed = false;
  float windowRange = stableMax - stableMin;

  if (windowRange <= stableBand) {
    float stableWeight = clampNoise((stableMin + stableMax) / 2.0);
    float stableChange = stableWeight - confirmedBasketWeight;

    if (abs(stableChange) > itemThreshold) {
      confirmedBasketWeight = stableWeight;
      pendingEvent = false;
      pendingPeakChange = 0;
      newWeightG = confirmedBasketWeight;
      changeG = stableChange;
      changed = true;
    }
  }

  if (pendingEvent && now - pendingEventStart > quickEventTimeoutMs) {
    pendingEvent = false;
    pendingPeakChange = 0;
  }

  stableMin = currentWeight;
  stableMax = currentWeight;
  stableWindowStart = now;
  return changed;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println("WhizCart scale node starting...");
  connectWiFi();
  initScale();
  reportWeight(0, false);
}

void loop() {
  float newWeightG = 0;
  float changeG = 0;

  if (updateStableWeight(newWeightG, changeG)) {
    Serial.printf("[Scale] Stable basket weight: %.1fg (%+.1fg)\n", newWeightG, changeG);
    reportWeight(newWeightG, true);
  }

  if (millis() - lastReportMs > periodicReportMs) {
    float currentWeight = clampNoise(LoadCell.getData());
    Serial.printf("[Scale] Periodic weight: %.1fg\n", currentWeight);
    reportWeight(currentWeight, false);
  }

  if (Serial.available() > 0) {
    char inByte = Serial.read();
    if (inByte == 't') {
      LoadCell.tareNoDelay();
      resetBasketState();
      Serial.println("[Scale] Tare requested.");
    } else if (inByte == 'p') {
      reportWeight(clampNoise(LoadCell.getData()), true);
    }
  }

  if (LoadCell.getTareStatus()) {
    resetBasketState();
    Serial.println("[Scale] Tare complete.");
    reportWeight(0, false);
  }
}
