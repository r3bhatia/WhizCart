// firmware/src/scale.cpp
// HX711 load cell interface for WhizCart.
//
// CALIBRATION STEPS (do once before demo):
//   1. Upload firmware with scale_calibrate() called in setup()
//   2. Open Serial Monitor at 115200 baud
//   3. Place a known weight (e.g. 500g) on the cart platform
//   4. Note the printed raw value — divide by known grams → SCALE_FACTOR
//   5. Paste the factor below and comment out scale_calibrate()

#include "scale.h"
#include <HX711.h>

#define DOUT_PIN     4
#define SCK_PIN      5
#define SCALE_FACTOR 420.0   // ← replace with your calibrated value
#define TOLERANCE_G  75.0    // ±75g tolerance for weight mismatch
#define TARE_SAMPLES 10      // averages for stable tare

HX711 scale;

void scale_init() {
  scale.begin(DOUT_PIN, SCK_PIN);
  scale.set_scale(SCALE_FACTOR);
  scale.tare(TARE_SAMPLES); // zero out the empty cart weight
  Serial.println("[Scale] Initialized and tared.");
}

float scale_readGrams() {
  if (!scale.is_ready()) return -1.0;
  // Average 5 readings for stability
  return scale.get_units(5);
}

void scale_tare() {
  scale.tare(TARE_SAMPLES);
  Serial.println("[Scale] Tared.");
}

WeightCheckResult scale_verify(float expectedG, int expectedCount) {
  WeightCheckResult result;
  result.expectedG   = expectedG;
  result.measuredG   = scale_readGrams();
  result.diffG       = result.measuredG - expectedG;
  result.ok          = abs(result.diffG) <= TOLERANCE_G;

  // Rough item count estimate: if each item averages ~200g,
  // significant jumps in weight can suggest unscanned items.
  // This is an approximation — weight check is the primary signal.
  result.itemCount   = expectedCount; // firmware tracks scan count; passed in

  Serial.printf("[Scale] Expected: %.1fg | Measured: %.1fg | Diff: %.1fg | %s\n",
    result.expectedG, result.measuredG, result.diffG,
    result.ok ? "OK" : "MISMATCH");

  return result;
}

void scale_calibrate() {
  Serial.println("[Scale] Calibration mode. Remove all weight and press Enter...");
  while (!Serial.available()) delay(100);
  Serial.read();
  scale.tare(20);
  Serial.println("Tared. Now place your known weight and press Enter...");
  while (!Serial.available()) delay(100);
  Serial.read();
  long raw = scale.get_value(20);
  Serial.printf("Raw value: %ld\n", raw);
  Serial.println("Divide raw by your known weight in grams to get SCALE_FACTOR.");
}
