#include "whizcart.h"

void setup() {
  Serial.begin(115200);
  Serial.println(String("LVGL Library Version: ") + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch());
  Serial.println("Scanner ready. Scan a barcode...");

  initDisplayAndTouch();
  initScanner();
}

void loop() {
  pollScanner();
}
