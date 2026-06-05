#include <HX711_ADC.h>

#if defined(ESP8266) || defined(ESP32) || defined(AVR)
#include <EEPROM.h>
#endif

// ================= PINS =================
const int HX711_dout = 4;
const int HX711_sck = 5;

// ================= HX711 =================
HX711_ADC LoadCell(HX711_dout, HX711_sck);

const int calVal_eepromAdress = 0;

// ================= SETTINGS =================
const float noiseClamp = 5.0;          // grams near zero treated as zero
const float itemThreshold = 50.0;      // minimum meaningful basket change

const float emaAlpha = 0.25;           // higher = faster, lower = smoother
const float stableBand = 15.0;         // max allowed variation while "stable"
const unsigned long stableWindowMs = 800;

const unsigned long quickEventDwellMs = 120;
const unsigned long quickEventTimeoutMs = 1800;

// ================= STATE VARIABLES =================
float filteredWeight = 0;
bool filterReady = false;

float confirmedBasketWeight = 0;

float stableMin = 0;
float stableMax = 0;
unsigned long stableWindowStart = 0;

bool pendingEvent = false;
float pendingPeakChange = 0;
unsigned long pendingEventStart = 0;

// ================= HELPERS =================
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

float clampNoise(float value) {
  if (value < 0) {
    return 0;
  }

  if (abs(value) < noiseClamp) {
    return 0;
  }

  return value;
}

void reportCommittedChange(float newWeight) {
  float change = newWeight - confirmedBasketWeight;

  if (abs(change) <= itemThreshold) {
    return;
  }

  if (change > 0) {
    Serial.print("Item added: +");
    Serial.print(change, 1);
    Serial.println(" g");
  } else {
    Serial.print("Item removed: -");
    Serial.print(abs(change), 1);
    Serial.println(" g");
  }

  confirmedBasketWeight = newWeight;

  Serial.print("Current basket weight: ");
  Serial.print(confirmedBasketWeight, 1);
  Serial.println(" g");

  pendingEvent = false;
  pendingPeakChange = 0;
}

void reportQuickNoChange() {
  Serial.print("Quick add/remove detected, basket unchanged. Peak change was about ");
  Serial.print(pendingPeakChange, 1);
  Serial.println(" g");

  Serial.print("Current basket weight: ");
  Serial.print(confirmedBasketWeight, 1);
  Serial.println(" g");

  pendingEvent = false;
  pendingPeakChange = 0;
}

// ================= SETUP =================
void setup() {
  Serial.begin(57600);
  delay(10);

  Serial.println();
  Serial.println("Starting...");

  LoadCell.begin();

  // Uncomment this if adding weight makes readings more negative
  // LoadCell.setReverseOutput();

  unsigned long stabilizingtime = 2000;
  boolean _tare = true;

  LoadCell.start(stabilizingtime, _tare);

  if (LoadCell.getTareTimeoutFlag() || LoadCell.getSignalTimeoutFlag()) {
    Serial.println("Timeout, check MCU > HX711 wiring");
    while (1);
  } else {
    LoadCell.setCalFactor(1.0);
    Serial.println("Startup is complete");
  }

  while (!LoadCell.update());

  resetBasketState();

  calibrate();
}

// ================= LOOP =================
void loop() {
  if (LoadCell.update()) {
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

    // Quickly notice a possible item add/remove.
    if (absChange > itemThreshold) {
      if (!pendingEvent) {
        pendingEvent = true;
        pendingEventStart = now;
        pendingPeakChange = absChange;
      } else if (absChange > pendingPeakChange) {
        pendingPeakChange = absChange;
      }
    }

    // Track how stable the basket is over a short time window.
    if (currentWeight < stableMin) {
      stableMin = currentWeight;
    }

    if (currentWeight > stableMax) {
      stableMax = currentWeight;
    }

    if (now - stableWindowStart >= stableWindowMs) {
      float windowRange = stableMax - stableMin;
      bool basketIsStable = windowRange <= stableBand;

      if (basketIsStable) {
        float stableWeight = clampNoise((stableMin + stableMax) / 2.0);
        float stableChange = stableWeight - confirmedBasketWeight;

        if (abs(stableChange) > itemThreshold) {
          reportCommittedChange(stableWeight);
        } else if (
          pendingEvent &&
          pendingPeakChange > itemThreshold &&
          now - pendingEventStart >= quickEventDwellMs
        ) {
          reportQuickNoChange();
        }
      }

      // If movement keeps happening and never settles, forget the pending event.
      if (pendingEvent && now - pendingEventStart > quickEventTimeoutMs) {
        pendingEvent = false;
        pendingPeakChange = 0;
      }

      stableMin = currentWeight;
      stableMax = currentWeight;
      stableWindowStart = now;
    }
  }

  if (Serial.available() > 0) {
    char inByte = Serial.read();

    if (inByte == 't') {
      LoadCell.tareNoDelay();
      resetBasketState();
    } else if (inByte == 'r') {
      calibrate();
    } else if (inByte == 'c') {
      changeSavedCalFactor();
    }
  }

  if (LoadCell.getTareStatus() == true) {
    Serial.println("Tare complete");
    resetBasketState();
  }
}

// ================= CALIBRATION =================
void calibrate() {
  Serial.println("***");
  Serial.println("Start calibration:");
  Serial.println("Remove all weight except the empty basket.");
  Serial.println("Send 't' to tare.");

  boolean _resume = false;

  while (_resume == false) {
    LoadCell.update();

    if (Serial.available() > 0) {
      char inByte = Serial.read();

      if (inByte == 't') {
        LoadCell.tareNoDelay();
      }
    }

    if (LoadCell.getTareStatus() == true) {
      Serial.println("Tare complete");
      resetBasketState();
      _resume = true;
    }
  }

  Serial.println("Place known mass on load cell.");
  Serial.println("Enter known mass in grams.");

  float known_mass = 0;
  _resume = false;

  while (_resume == false) {
    LoadCell.update();

    if (Serial.available() > 0) {
      known_mass = Serial.parseFloat();

      if (known_mass != 0) {
        Serial.print("Known mass is: ");
        Serial.println(known_mass);
        _resume = true;
      }
    }
  }

  LoadCell.refreshDataSet();

  float newCalibrationValue = LoadCell.getNewCalibration(known_mass);

  Serial.print("New calibration value: ");
  Serial.println(newCalibrationValue);

  Serial.println("Save to EEPROM? y/n");

  _resume = false;

  while (_resume == false) {
    if (Serial.available() > 0) {
      char inByte = Serial.read();

      if (inByte == 'y') {
#if defined(ESP8266) || defined(ESP32)
        EEPROM.begin(512);
#endif

        EEPROM.put(calVal_eepromAdress, newCalibrationValue);

#if defined(ESP8266) || defined(ESP32)
        EEPROM.commit();
#endif

        Serial.println("Value saved to EEPROM");
        _resume = true;
      } else if (inByte == 'n') {
        Serial.println("Value not saved");
        _resume = true;
      }
    }
  }

  LoadCell.setCalFactor(newCalibrationValue);
  resetBasketState();

  Serial.println("***");
  Serial.println("Calibration complete");
  Serial.println("***");
}

// ================= CHANGE CAL FACTOR =================
void changeSavedCalFactor() {
  float oldCalibrationValue = LoadCell.getCalFactor();

  Serial.println("***");
  Serial.print("Current value: ");
  Serial.println(oldCalibrationValue);

  Serial.println("Enter new calibration value:");

  boolean _resume = false;
  float newCalibrationValue = 0;

  while (_resume == false) {
    if (Serial.available() > 0) {
      newCalibrationValue = Serial.parseFloat();

      if (newCalibrationValue != 0) {
        Serial.print("New calibration value: ");
        Serial.println(newCalibrationValue);

        LoadCell.setCalFactor(newCalibrationValue);
        _resume = true;
      }
    }
  }

  Serial.println("Save to EEPROM? y/n");

  _resume = false;

  while (_resume == false) {
    if (Serial.available() > 0) {
      char inByte = Serial.read();

      if (inByte == 'y') {
#if defined(ESP8266) || defined(ESP32)
        EEPROM.begin(512);
#endif

        EEPROM.put(calVal_eepromAdress, newCalibrationValue);

#if defined(ESP8266) || defined(ESP32)
        EEPROM.commit();
#endif

        Serial.println("Value saved");
        _resume = true;
      } else if (inByte == 'n') {
        Serial.println("Value not saved");
        _resume = true;
      }
    }
  }

  resetBasketState();

  Serial.println("***");
}
