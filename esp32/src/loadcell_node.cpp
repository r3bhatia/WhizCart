// firmware/src/loadcell_node.cpp
// Standalone ESP32 firmware for WhizCart load-cell/HX711 weight reporting.

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <HX711_ADC.h>
#include <ArduinoJson.h>
#include <math.h>

#if defined(ESP8266) || defined(ESP32) || defined(AVR)
#include <EEPROM.h>
#endif

const char* WIFI_SSID = "iPhone";
const char* WIFI_PASSWORD = "freshboi";
const char* BACKEND_IP = "172.20.10.7";
const int BACKEND_PORT = 3001;
const char* CART_ID = "basket-001";

const int HX711_DOUT = 4;
const int HX711_SCK = 5;
const int CAL_VALUE_EEPROM_ADDRESS = 0;
const float FALLBACK_CALIBRATION_VALUE = 696.0;

const float noiseClamp = 5.0;
const float itemThreshold = 50.0;
const float emaAlpha = 0.25;
const float stableBand = 20.0;
const float stableRepeatBand = 18.0;
const int requiredStableWindows = 2;
const unsigned long stableWindowMs = 1200;
const unsigned long quickEventTimeoutMs = 3000;
const unsigned long periodicReportMs = 7000;
const unsigned long cartStatePollMs = 1000;
const float autoZeroBand = 30.0;
const unsigned long autoZeroCooldownMs = 5000;

HX711_ADC LoadCell(HX711_DOUT, HX711_SCK);

float filteredWeight = 0;
bool filterReady = false;
float confirmedBasketWeight = 0;
float lastRawWeight = 0;
float stableMin = 0;
float stableMax = 0;
unsigned long stableWindowStart = 0;
bool pendingEvent = false;
float pendingPeakChange = 0;
unsigned long pendingEventStart = 0;
unsigned long lastReportMs = 0;
float candidateStableWeight = 0;
int consecutiveStableWindows = 0;
bool backendHasPendingItem = false;
bool backendHasCartItems = false;
unsigned long lastCartStatePollMs = 0;
unsigned long lastAutoZeroMs = 0;
bool autoZeroTarePending = false;

void connectWiFi();

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
  candidateStableWeight = 0;
  consecutiveStableWindows = 0;
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

bool refreshCartState() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  HTTPClient http;
  String url = baseUrl() + "/api/cart?cartId=" + String(CART_ID);
  http.begin(url);
  http.setTimeout(1500);
  http.setReuse(false);

  int code = http.GET();
  bool ok = false;
  if (code == 200) {
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, http.getString());
    if (!error) {
      backendHasPendingItem = !doc["pendingItem"].isNull();
      JsonArray items = doc["items"].as<JsonArray>();
      backendHasCartItems = items.size() > 0;
      ok = true;
    }
  }

  Serial.printf("[Scale API] Cart state HTTP code: %d pending=%s items=%s\n",
    code,
    backendHasPendingItem ? "yes" : "no",
    backendHasCartItems ? "yes" : "no");

  http.end();
  return ok;
}

void refreshCartStateIfNeeded() {
  if (millis() - lastCartStatePollMs < cartStatePollMs) return;
  lastCartStatePollMs = millis();
  refreshCartState();
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

  Serial.print("[WiFi] Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startMs < 20000) {
    delay(500);
    Serial.printf("[WiFi] status=%d elapsed=%lus\n", WiFi.status(), (millis() - startMs) / 1000);
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("[WiFi] FAILED status=%d. Check SSID/password and 2.4GHz WiFi.\n", WiFi.status());
    return;
  }

  WiFi.setSleep(false);
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
  http.setTimeout(2000);
  http.setReuse(false);
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
  lastRawWeight = rawWeight;

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

    if (consecutiveStableWindows == 0 || abs(stableWeight - candidateStableWeight) > stableRepeatBand) {
      candidateStableWeight = stableWeight;
      consecutiveStableWindows = 1;
    } else {
      candidateStableWeight = (candidateStableWeight + stableWeight) / 2.0;
      consecutiveStableWindows++;
    }

    if (consecutiveStableWindows >= requiredStableWindows && abs(stableChange) > itemThreshold) {
      float previousConfirmedWeight = confirmedBasketWeight;
      confirmedBasketWeight = candidateStableWeight;
      pendingEvent = false;
      pendingPeakChange = 0;
      newWeightG = confirmedBasketWeight;
      changeG = confirmedBasketWeight - previousConfirmedWeight;
      changed = true;
    }

    bool cartShouldBeEmpty = !backendHasPendingItem && !backendHasCartItems;
    bool scaleLooksEmpty = abs(stableWeight) <= noiseClamp && abs(filteredWeight) <= autoZeroBand;
    bool hasHiddenDrift = abs(filteredWeight) > noiseClamp;
    bool tareIsDue = !autoZeroTarePending && millis() - lastAutoZeroMs > autoZeroCooldownMs;

    if (!changed && cartShouldBeEmpty && scaleLooksEmpty && hasHiddenDrift && tareIsDue) {
      autoZeroTarePending = true;
      lastAutoZeroMs = millis();
      LoadCell.tareNoDelay();
      Serial.printf("[Scale] Auto-zero requested. Filtered drift: %.1fg\n", filteredWeight);
    }
  } else {
    consecutiveStableWindows = 0;
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
  refreshCartState();
}

void loop() {
  float newWeightG = 0;
  float changeG = 0;

  if (updateStableWeight(newWeightG, changeG)) {
    Serial.printf("[Scale] Stable basket weight: %.1fg (%+.1fg)\n", newWeightG, changeG);
    refreshCartState();
    if (backendHasPendingItem || backendHasCartItems) {
      reportWeight(newWeightG, true);
    } else {
      Serial.println("[Scale] Ignoring stable change because cart has no pending item.");
    }
  }

  refreshCartStateIfNeeded();

  if (millis() - lastReportMs > periodicReportMs) {
    float currentWeight = clampNoise(LoadCell.getData());
    if (filterReady) {
      currentWeight = clampNoise(filteredWeight);
    }
    Serial.printf("[Scale] Periodic raw=%.1fg filtered=%.1fg normalized=%.1fg\n",
      lastRawWeight,
      filterReady ? filteredWeight : lastRawWeight,
      currentWeight);
    if (backendHasPendingItem || backendHasCartItems || currentWeight <= noiseClamp) {
      reportWeight(currentWeight, false);
    } else {
      Serial.println("[Scale] Skipping periodic report until an item is scanned.");
      lastReportMs = millis();
    }
  }

  if (Serial.available() > 0) {
    char inByte = Serial.read();
    if (inByte == 't') {
      LoadCell.tareNoDelay();
      resetBasketState();
      Serial.println("[Scale] Tare requested.");
    } else if (inByte == 'p') {
      LoadCell.update();
      reportWeight(clampNoise(LoadCell.getData()), true);
    }
  }

  if (LoadCell.getTareStatus()) {
    autoZeroTarePending = false;
    resetBasketState();
    Serial.println("[Scale] Tare complete.");
    reportWeight(0, false);
  }
}
