// firmware/src/scale.h
// HX711 load-cell support for WhizCart.

#pragma once
#include <Arduino.h>

struct WeightCheckResult {
  float measuredG;
  float expectedG;
  float diffG;
  bool ok;
  int itemCount;
};

struct ScaleEvent {
  bool changed;
  bool added;
  float newWeightG;
  float changeG;
};

void scale_init();
float scale_readGrams();
bool scale_update(ScaleEvent& event);
WeightCheckResult scale_verify(float expectedG, int expectedCount);
void scale_tare();
void scale_calibrate();
