// firmware/src/scale.h
// Reads weight from load cells via HX711 amplifier.
// Library: "HX711" by bogde — install via Arduino Library Manager.
//
// Wiring (4 load cells in a Wheatstone bridge configuration):
//   HX711 DOUT → ESP32 GPIO 4
//   HX711 SCK  → ESP32 GPIO 5
//   HX711 VCC  → 3.3V
//   HX711 GND  → GND
//
// Calibration: run scale_calibrate() once with a known weight,
// note the printed SCALE_FACTOR and paste it below.

#pragma once
#include <Arduino.h>

// Result of a weight verification check
struct WeightCheckResult {
  float   measuredG;    // grams read from scale
  float   expectedG;    // grams expected from scanned items
  float   diffG;        // measuredG - expectedG
  bool    ok;           // true if within tolerance
  int     itemCount;    // number of distinct physical items detected
};

void             scale_init();
float            scale_readGrams();       // raw weight reading
WeightCheckResult scale_verify(float expectedG, int expectedCount);
void             scale_tare();            // zero the scale (call on cart clear)
void             scale_calibrate();       // prints calibration factor to Serial