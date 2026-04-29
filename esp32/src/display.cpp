// firmware/src/display.cpp
// Drives a TFT LCD using the TFT_eSPI library.
// Configure TFT_eSPI for your specific screen in its User_Setup.h before building.
// Common choices: ILI9341 (240x320), ST7789 (240x240 or 170x320).

#include "display.h"
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

// Color palette
#define BG_COLOR    TFT_BLACK
#define ACCENT      0x04B3   // teal-ish green
#define TEXT_MAIN   TFT_WHITE
#define TEXT_DIM    TFT_DARKGREY
#define PRICE_COLOR 0xA7E3   // bright green

void display_init() {
  tft.init();
  tft.setRotation(1); // landscape
  tft.fillScreen(BG_COLOR);
  tft.setTextColor(TEXT_MAIN, BG_COLOR);

  // Header bar
  tft.fillRect(0, 0, tft.width(), 28, ACCENT);
  tft.setTextSize(2);
  tft.setCursor(8, 6);
  tft.setTextColor(TFT_WHITE, ACCENT);
  tft.print("WhizCart");
}

void display_showStatus(String msg) {
  tft.fillRect(0, 30, tft.width(), 20, BG_COLOR);
  tft.setTextSize(1);
  tft.setTextColor(TEXT_DIM, BG_COLOR);
  tft.setCursor(8, 36);
  tft.print(msg);
}

void display_showTotal(float total) {
  tft.fillRect(0, 30, tft.width(), tft.height() - 30, BG_COLOR);
  tft.setTextSize(1);
  tft.setTextColor(TEXT_DIM, BG_COLOR);
  tft.setCursor(8, 36);
  tft.print("Running Total:");

  tft.setTextSize(4);
  tft.setTextColor(PRICE_COLOR, BG_COLOR);
  tft.setCursor(8, 60);
  tft.print("$");
  tft.print(total, 2);
}

void display_showItem(String name, float price, float total) {
  tft.fillRect(0, 30, tft.width(), tft.height() - 30, BG_COLOR);

  // Item name
  tft.setTextSize(1);
  tft.setTextColor(TEXT_MAIN, BG_COLOR);
  tft.setCursor(8, 36);
  tft.print(name.substring(0, 35)); // truncate long names

  // Item price
  tft.setTextSize(2);
  tft.setTextColor(PRICE_COLOR, BG_COLOR);
  tft.setCursor(8, 52);
  tft.print("$");
  tft.print(price, 2);

  // Divider
  tft.drawLine(0, 80, tft.width(), 80, TEXT_DIM);

  // Running total
  tft.setTextSize(1);
  tft.setTextColor(TEXT_DIM, BG_COLOR);
  tft.setCursor(8, 88);
  tft.print("TOTAL:");
  tft.setTextSize(3);
  tft.setTextColor(TFT_WHITE, BG_COLOR);
  tft.setCursor(8, 100);
  tft.print("$");
  tft.print(total, 2);
}

void display_showRecommendations(RecommendationList& recs) {
  tft.fillRect(0, 30, tft.width(), tft.height() - 30, BG_COLOR);
  tft.setTextSize(1);
  tft.setTextColor(ACCENT, BG_COLOR);
  tft.setCursor(8, 36);
  tft.print("-- You might also want --");

  int y = 52;
  for (int i = 0; i < (int)recs.size() && i < 4; i++) {
    tft.setTextColor(TEXT_MAIN, BG_COLOR);
    tft.setCursor(8, y);
    tft.print(recs[i].name.substring(0, 26));

    tft.setTextColor(PRICE_COLOR, BG_COLOR);
    tft.setCursor(200, y);
    tft.print("$");
    tft.print(recs[i].price, 2);

    tft.setTextColor(TEXT_DIM, BG_COLOR);
    tft.setCursor(8, y + 12);
    tft.print("Aisle: ");
    tft.print(recs[i].aisle);

    y += 34;
    tft.drawLine(0, y - 4, tft.width(), y - 4, 0x2104); // subtle divider
  }
}

// ── Weight verification screen ───────────────────────────────────────────────
// Shows a clear OK or WARNING after each scan, with measured vs expected weight.
void display_showWeightCheck(float measured, float expected, bool ok) {
  tft.fillRect(0, 30, tft.width(), tft.height() - 30, BG_COLOR);

  // Status icon area
  uint16_t statusColor = ok ? PRICE_COLOR : TFT_RED;
  tft.fillRoundRect(6, 34, tft.width() - 12, 44, 8, ok ? 0x0440 : 0x3000); // tinted bg

  tft.setTextSize(2);
  tft.setTextColor(statusColor, ok ? 0x0440 : 0x3000);
  tft.setCursor(16, 44);
  tft.print(ok ? "Weight OK" : "Weight Mismatch!");

  // Measured weight
  tft.setTextSize(1);
  tft.setTextColor(TEXT_DIM, BG_COLOR);
  tft.setCursor(8, 88);
  tft.print("Measured:");
  tft.setTextColor(TEXT_MAIN, BG_COLOR);
  tft.setCursor(80, 88);
  tft.print(measured, 1);
  tft.print("g");

  // Expected weight
  tft.setTextColor(TEXT_DIM, BG_COLOR);
  tft.setCursor(8, 104);
  tft.print("Expected:");
  tft.setTextColor(TEXT_MAIN, BG_COLOR);
  tft.setCursor(80, 104);
  tft.print(expected, 1);
  tft.print("g");

  // Difference
  float diff = measured - expected;
  tft.setTextColor(TEXT_DIM, BG_COLOR);
  tft.setCursor(8, 120);
  tft.print("Diff:    ");
  tft.setTextColor(ok ? PRICE_COLOR : TFT_RED, BG_COLOR);
  tft.setCursor(80, 120);
  if (diff >= 0) tft.print("+");
  tft.print(diff, 1);
  tft.print("g");

  // Warning message if mismatch
  if (!ok) {
    tft.setTextSize(1);
    tft.setTextColor(TFT_RED, BG_COLOR);
    tft.setCursor(8, 144);
    tft.print("Please scan all items!");
  }
}
