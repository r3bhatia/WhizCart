// firmware/src/scale.cpp
// HX711_ADC load-cell interface for WhizCart.

#include "scale.h"
#include <HX711_ADC.h>
#include <math.h>

#if defined(ESP8266) || defined(ESP32) || defined(AVR)
#include <EEPROM.h>
#endif

#define DOUT_PIN 4
#define SCK_PIN 5
#define CAL_VALUE_EEPROM_ADDRESS 0
#define FALLBACK_CALIBRATION_VALUE 696.0
#define TOLERANCE_G 100.0

const float noiseClamp = 5.0;
const float itemThreshold = 50.0;
const float emaAlpha = 0.25;
const float stableBand = 15.0;
const unsigned long stableWindowMs = 800;
const unsigned long quickEventTimeoutMs = 1800;

HX711_ADC LoadCell(DOUT_PIN, SCK_PIN);

float filteredWeight = 0;
bool filterReady = false;

float confirmedBasketWeight = 0;

float stableMin = 0;
float stableMax = 0;
unsigned long stableWindowStart = 0;

bool pendingEvent = false;
float pendingPeakChange = 0;
unsigned long pendingEventStart = 0;

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

void scale_init() {
  LoadCell.begin();
  unsigned long stabilizingtime = 2000;
  boolean tare = true;

  LoadCell.start(stabilizingtime, tare);

  if (LoadCell.getTareTimeoutFlag() || LoadCell.getSignalTimeoutFlag()) {
    Serial.println("[Scale] Timeout. Check ESP32-to-HX711 wiring.");
    return;
  }

  float calibrationValue = readCalibrationValue();
  LoadCell.setCalFactor(calibrationValue);

  while (!LoadCell.update()) {
    delay(1);
  }

  resetBasketState();
  Serial.print("[Scale] Initialized. Calibration value: ");
  Serial.println(calibrationValue);
}

float scale_readGrams() {
  LoadCell.update();
  return clampNoise(LoadCell.getData());
}

bool scale_update(ScaleEvent& event) {
  event = { false, false, confirmedBasketWeight, 0 };

  if (!LoadCell.update()) {
    return false;
  }

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

  if (now - stableWindowStart < stableWindowMs) {
    return false;
  }

  float windowRange = stableMax - stableMin;
  bool basketIsStable = windowRange <= stableBand;

  if (basketIsStable) {
    float stableWeight = clampNoise((stableMin + stableMax) / 2.0);
    float stableChange = stableWeight - confirmedBasketWeight;

    if (abs(stableChange) > itemThreshold) {
      confirmedBasketWeight = stableWeight;
      pendingEvent = false;
      pendingPeakChange = 0;

      event.changed = true;
      event.added = stableChange > 0;
      event.newWeightG = confirmedBasketWeight;
      event.changeG = stableChange;
    }
  }

  if (pendingEvent && now - pendingEventStart > quickEventTimeoutMs) {
    pendingEvent = false;
    pendingPeakChange = 0;
  }

  stableMin = currentWeight;
  stableMax = currentWeight;
  stableWindowStart = now;

  return event.changed;
}

WeightCheckResult scale_verify(float expectedG, int expectedCount) {
  WeightCheckResult result;
  result.expectedG = expectedG;
  result.measuredG = scale_readGrams();
  result.diffG = result.measuredG - expectedG;
  result.ok = abs(result.diffG) <= TOLERANCE_G;
  result.itemCount = expectedCount;

  Serial.printf("[Scale] Expected: %.1fg | Measured: %.1fg | Diff: %.1fg | %s\n",
    result.expectedG, result.measuredG, result.diffG,
    result.ok ? "OK" : "MISMATCH");

  return result;
}

void scale_tare() {
  LoadCell.tareNoDelay();
  resetBasketState();
  Serial.println("[Scale] Tare requested.");
}

void scale_calibrate() {
  Serial.println("[Scale] Use Calibration.ino to calculate and save the calibration value.");
}
