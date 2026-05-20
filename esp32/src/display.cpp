// firmware/src/display.cpp
// TFT + XPT2046 touch driver for the CYD (ESP32-2432S028R).

#include "display.h"
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

#define TOUCH_CS   33
#define TOUCH_IRQ  36
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CLK  25

#define HEADER_H      38
#define STATUS_H      30
#define NAV_H         42
#define CONTENT_Y     (HEADER_H + STATUS_H)
#define ITEM_ROW_H    58
#define ITEM_START_Y  CONTENT_Y
#define MAX_CART_ROWS  4
#define DEL_BTN_W     58

#define BG_COLOR     0xF7DE  // #f7fbf4
#define PANEL_COLOR  0xFFFF  // #ffffff
#define SOFT_GREEN   0xEFDD  // #eef8e8
#define BORDER_COLOR 0xDF5A  // #dcebd5
#define ACCENT       0x262B  // #22c55e
#define ACCENT_DARK  0x1407  // #15803d
#define HARVEST      TFT_YELLOW
#define TEXT_MAIN    0x1183  // #17321f
#define TEXT_DIM     0x6BEE  // #6f7f72
#define PRICE_COLOR  0x1509  // #16a34a
#define WARN_COLOR   0xEA28  // #ef4444
#define DEL_COLOR    0xEA28
#define DEL_BG       0xFF9E

#define TOUCH_X_MIN   200
#define TOUCH_X_MAX  3800
#define TOUCH_Y_MIN   300
#define TOUCH_Y_MAX  3700

#define BACK_BTN_W     82
#define BACK_BTN_H     27
#define BACK_BTN_RIGHT 26
#define BACK_BTN_Y      6

TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

struct ScreenPoint { int x; int y; bool valid; };

int screenW() { return tft.width(); }
int screenH() { return tft.height(); }

String fitText(String text, int maxChars) {
  if ((int)text.length() <= maxChars) return text;
  return text.substring(0, maxChars - 3) + "...";
}

void wipeDisplayMemory() {
  for (int rotation = 0; rotation < 4; rotation++) {
    tft.setRotation(rotation);
    tft.fillScreen(TFT_BLACK);
    delay(40);
  }
}

void drawHeader() {
  tft.fillRect(0, 0, screenW(), HEADER_H, PANEL_COLOR);
  tft.drawLine(0, HEADER_H - 1, screenW(), HEADER_H - 1, BORDER_COLOR);
  tft.setTextSize(2);
  tft.setTextColor(ACCENT_DARK, PANEL_COLOR);
  tft.setCursor(8, 11);
  tft.print("WhizCart");
}

void drawStatus(String msg) {
  tft.fillRect(0, HEADER_H, screenW(), STATUS_H, SOFT_GREEN);
  tft.setTextSize(2);
  tft.setTextColor(TEXT_DIM, SOFT_GREEN);
  tft.setCursor(8, HEADER_H + 8);
  tft.print(fitText(msg, 25));
}

void clearContent() {
  tft.fillRect(0, CONTENT_Y, screenW(), screenH() - CONTENT_Y, BG_COLOR);
}

void drawNavBar() {
  int y = screenH() - NAV_H;
  tft.fillRect(0, y, screenW(), NAV_H, SOFT_GREEN);
  tft.drawLine(0, y, screenW(), y, BORDER_COLOR);
  tft.drawLine(screenW() / 2, y, screenW() / 2, screenH(), BORDER_COLOR);

  tft.setTextSize(2);
  tft.setTextColor(ACCENT_DARK, SOFT_GREEN);
  tft.setCursor((screenW() / 4) - 24, y + 13);
  tft.print("Cart");
  tft.setCursor((screenW() * 3 / 4) - 24, y + 13);
  tft.print("Recs");
}

void drawBackHint() {
  tft.fillRoundRect(screenW() - BACK_BTN_W - BACK_BTN_RIGHT, BACK_BTN_Y, BACK_BTN_W, BACK_BTN_H, 5, HARVEST);
  tft.setTextSize(2);
  tft.setTextColor(TEXT_MAIN, HARVEST);
  tft.setCursor(screenW() - BACK_BTN_W - BACK_BTN_RIGHT + 14, BACK_BTN_Y + 6);
  tft.print("BACK");
}

ScreenPoint getTouchPoint() {
  if (!ts.tirqTouched() || !ts.touched()) return { 0, 0, false };
  TS_Point p = ts.getPoint();
  int sx = map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, screenW());
  int sy = map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, screenH());
  sx = constrain(sx, 0, screenW() - 1);
  sy = constrain(sy, 0, screenH() - 1);

  static unsigned long lastTouchLogMs = 0;
  unsigned long now = millis();
  if (now - lastTouchLogMs > 250) {
    Serial.printf("[Touch] raw=(%d,%d,%d) screen=(%d,%d)\n", p.x, p.y, p.z, sx, sy);
    lastTouchLogMs = now;
  }

  return { sx, sy, true };
}

void waitForTouchRelease() {
  while (ts.touched()) delay(10);
  delay(80);
}

void display_init() {
  tft.init();
  wipeDisplayMemory();
  tft.setRotation(0);
  tft.fillScreen(BG_COLOR);
  Serial.printf("[Display] rotation=0 width=%d height=%d\n", tft.width(), tft.height());

  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);

  SPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  ts.begin();
  ts.setRotation(0);

  drawHeader();
  display_showStatus("Ready");
}

void display_showStatus(String msg) {
  drawStatus(msg);
}

void display_showTotal(float total, float measuredWeightG) {
  tft.fillScreen(BG_COLOR);
  drawHeader();
  drawStatus("Ready to scan");

  int navY = screenH() - NAV_H;
  int contentH = navY - CONTENT_Y;

  tft.fillRect(0, CONTENT_Y, screenW(), contentH, SOFT_GREEN);

  tft.setTextSize(2);
  tft.setTextColor(TEXT_DIM, SOFT_GREEN);
  tft.setCursor(10, CONTENT_Y + 12);
  tft.print("Running Total");

  tft.fillRect(0, CONTENT_Y + 44, screenW(), 104, PANEL_COLOR);
  tft.setTextSize(4);
  tft.setTextColor(ACCENT_DARK, PANEL_COLOR);
  tft.setCursor(12, CONTENT_Y + 76);
  tft.print("$");
  tft.print(total, 2);

  tft.fillRect(0, CONTENT_Y + 148, screenW(), contentH - 148, 0xDFFC);
  tft.setTextSize(2);
  tft.setTextColor(TEXT_DIM, 0xDFFC);
  tft.setCursor(10, CONTENT_Y + 166);
  if (measuredWeightG >= 0) {
    tft.print("Cart weight: ");
    tft.print(measuredWeightG, 1);
    tft.print("g");
  } else {
    tft.print("Scan an item to begin");
  }
  tft.setCursor(10, CONTENT_Y + 194);
  tft.print("Tap Cart or Recs");

  drawNavBar();
}

void display_showItem(String name, float price, float weightG, float total) {
  tft.fillScreen(BG_COLOR);
  drawHeader();
  drawStatus("Added item");

  int navY = screenH() - NAV_H;
  tft.fillRect(0, CONTENT_Y, screenW(), navY - CONTENT_Y, SOFT_GREEN);

  tft.setTextSize(2);
  tft.setTextColor(TEXT_MAIN, SOFT_GREEN);
  tft.setCursor(10, CONTENT_Y + 20);
  tft.print(fitText(name, 18));

  tft.setTextColor(PRICE_COLOR, SOFT_GREEN);
  tft.setCursor(10, CONTENT_Y + 56);
  tft.print("$");
  tft.print(price, 2);

  if (weightG > 0) {
    tft.setTextSize(2);
    tft.setTextColor(TEXT_DIM, SOFT_GREEN);
    tft.setCursor(10, CONTENT_Y + 88);
    tft.print("Weight: ");
    tft.print(weightG, 0);
    tft.print("g");
  }

  tft.fillRect(0, CONTENT_Y + 128, screenW(), navY - (CONTENT_Y + 128), PANEL_COLOR);
  tft.setTextSize(2);
  tft.setTextColor(TEXT_DIM, PANEL_COLOR);
  tft.setCursor(10, CONTENT_Y + 144);
  tft.print("TOTAL");
  tft.setTextSize(3);
  tft.setTextColor(ACCENT_DARK, PANEL_COLOR);
  tft.setCursor(10, CONTENT_Y + 166);
  tft.print("$");
  tft.print(total, 2);

  drawNavBar();
}

void display_showCartList(CartList& items, float total, float measuredWeightG) {
  tft.fillScreen(BG_COLOR);
  drawHeader();
  drawBackHint();
  drawStatus("Cart items");

  if (items.empty()) {
    int navY = screenH() - NAV_H;
    tft.fillRect(0, CONTENT_Y, screenW(), navY - CONTENT_Y, SOFT_GREEN);
    tft.setTextSize(2);
    tft.setTextColor(TEXT_DIM, SOFT_GREEN);
    tft.setCursor((screenW() - 156) / 2, CONTENT_Y + 74);
    tft.print("Cart is empty");
    tft.setTextSize(2);
    tft.setCursor((screenW() - 168) / 2, CONTENT_Y + 108);
    tft.print("Scan something");
    drawNavBar();
    return;
  }

  int footerY = screenH() - NAV_H - 44;
  int rows = min((int)items.size(), min(MAX_CART_ROWS, (footerY - ITEM_START_Y) / ITEM_ROW_H));

  for (int i = 0; i < rows; i++) {
    int y = ITEM_START_Y + i * ITEM_ROW_H;
    int rowBottom = y + ITEM_ROW_H - 2;

    tft.setTextSize(2);
    uint16_t rowColor = (i % 2 == 0) ? PANEL_COLOR : SOFT_GREEN;
    tft.fillRect(0, y, screenW(), ITEM_ROW_H - 2, rowColor);

    tft.setTextColor(TEXT_MAIN, rowColor);
    tft.setCursor(8, y + 6);
    tft.print(fitText(items[i].name, 18));

    tft.setTextColor(PRICE_COLOR, rowColor);
    tft.setCursor(8, y + 32);
    tft.print("$");
    tft.print(items[i].price, 2);
    if (items[i].weightG > 0) {
      tft.setTextColor(TEXT_DIM, rowColor);
      tft.print(" ");
      tft.print(items[i].weightG, 0);
      tft.print("g");
    }
    if (items[i].qty > 1) {
      tft.setTextColor(TEXT_DIM, rowColor);
      tft.print(" qty ");
      tft.print(items[i].qty);
    }

    tft.fillRect(screenW() - DEL_BTN_W, y, DEL_BTN_W, ITEM_ROW_H - 2, DEL_BG);
    tft.setTextColor(DEL_COLOR, DEL_BG);
    tft.setCursor(screenW() - DEL_BTN_W + 8, y + 20);
    tft.print("DEL");

    tft.drawLine(0, rowBottom, screenW(), rowBottom, BORDER_COLOR);
  }

  tft.fillRect(0, footerY, screenW(), 44, PANEL_COLOR);
  tft.drawLine(0, footerY, screenW(), footerY, BORDER_COLOR);
  tft.setTextSize(2);
  tft.setTextColor(TEXT_MAIN, PANEL_COLOR);
  tft.setCursor(8, footerY + 8);
  tft.print("Total: $");
  tft.print(total, 2);
  tft.setTextColor(TEXT_DIM, PANEL_COLOR);
  tft.setCursor(8, footerY + 27);
  if (measuredWeightG >= 0) {
    tft.print("Weight: ");
    tft.print(measuredWeightG, 0);
    tft.print("g");
  } else {
    tft.print("Tap BACK for total");
  }

  drawNavBar();
}

String display_getCartTap(CartList& items) {
  ScreenPoint p = getTouchPoint();
  if (!p.valid) return "";

  int deleteX = screenW() - DEL_BTN_W;
  if (p.x < deleteX) return "";

  int row = (p.y - ITEM_START_Y) / ITEM_ROW_H;
  if (row < 0 || row >= (int)items.size() || row >= MAX_CART_ROWS) return "";

  waitForTouchRelease();
  Serial.printf("[Touch] Delete row %d barcode=%s\n", row, items[row].barcode.c_str());
  return items[row].barcode;
}

void display_showRecommendations(RecommendationList& recs) {
  tft.fillScreen(BG_COLOR);
  drawHeader();
  drawBackHint();
  drawStatus("Recommendations");

  int navY = screenH() - NAV_H;
  tft.fillRect(0, CONTENT_Y, screenW(), navY - CONTENT_Y, BG_COLOR);

  tft.setTextSize(2);
  tft.setTextColor(ACCENT, BG_COLOR);
  tft.setCursor(8, CONTENT_Y + 8);
  tft.print("Recommended");

  if (recs.empty()) {
    tft.fillRect(0, CONTENT_Y + 48, screenW(), navY - (CONTENT_Y + 48), SOFT_GREEN);
    tft.setTextSize(2);
    tft.setTextColor(TEXT_DIM, SOFT_GREEN);
    tft.setCursor(10, CONTENT_Y + 88);
    tft.print("Add items to see suggestions.");
    drawNavBar();
    return;
  }

  int y = CONTENT_Y + 42;
  for (int i = 0; i < (int)recs.size() && i < 5; i++) {
    uint16_t recBg = (i % 2 == 0) ? PANEL_COLOR : SOFT_GREEN;
    tft.fillRect(0, y - 4, screenW(), 50, recBg);
    tft.setTextSize(2);
    tft.setTextColor(TEXT_MAIN, recBg);
    tft.setCursor(8, y + 2);
    tft.print(fitText(recs[i].name, 18));

    tft.setTextColor(PRICE_COLOR, recBg);
    tft.setCursor(218, y + 2);
    tft.print("$");
    tft.print(recs[i].price, 2);

    tft.setTextSize(1);
    tft.setTextColor(TEXT_DIM, recBg);
    tft.setCursor(8, y + 30);
    tft.print("Aisle: ");
    tft.print(recs[i].aisle);

    y += 54;
  }

  drawNavBar();
}

void display_showWeightCheck(float measured, float expected, bool ok) {
  tft.fillScreen(BG_COLOR);
  drawHeader();
  drawStatus(ok ? "Weight verified" : "Weight mismatch");

  uint16_t statusColor = ok ? PRICE_COLOR : WARN_COLOR;
  uint16_t bgTint = ok ? 0xDFFC : 0xFF9E;

  tft.fillRect(0, CONTENT_Y, screenW(), 78, bgTint);
  tft.setTextSize(2);
  tft.setTextColor(statusColor, bgTint);
  tft.setCursor(10, CONTENT_Y + 28);
  tft.print(ok ? "Weight OK" : "Check Weight");

  tft.fillRect(0, CONTENT_Y + 78, screenW(), screenH() - NAV_H - (CONTENT_Y + 78), PANEL_COLOR);
  tft.setTextSize(2);
  tft.setTextColor(TEXT_DIM, PANEL_COLOR);
  tft.setCursor(10, CONTENT_Y + 100);  tft.print("Measured:");
  tft.setTextColor(TEXT_MAIN, PANEL_COLOR);
  tft.setCursor(128, CONTENT_Y + 100); tft.print(measured, 1); tft.print("g");

  tft.setTextColor(TEXT_DIM, PANEL_COLOR);
  tft.setCursor(10, CONTENT_Y + 130);  tft.print("Expected:");
  tft.setTextColor(TEXT_MAIN, PANEL_COLOR);
  tft.setCursor(128, CONTENT_Y + 130); tft.print(expected, 1); tft.print("g");

  float diff = measured - expected;
  tft.setTextColor(TEXT_DIM, PANEL_COLOR);
  tft.setCursor(10, CONTENT_Y + 156);  tft.print("Diff:");
  tft.setTextColor(ok ? PRICE_COLOR : WARN_COLOR, PANEL_COLOR);
  tft.setCursor(128, CONTENT_Y + 156);
  if (diff >= 0) tft.print("+");
  tft.print(diff, 1);
  tft.print("g");

  if (!ok) {
    tft.setTextColor(WARN_COLOR, BG_COLOR);
    tft.setCursor(14, CONTENT_Y + 206);
    tft.print("Scan missing item or remove extra.");
  }

  drawNavBar();
}

char display_getNavTap(Mode currentMode) {
  ScreenPoint p = getTouchPoint();
  if (!p.valid) return '\0';

  int backX = screenW() - BACK_BTN_W - BACK_BTN_RIGHT;
  bool tappedBack = p.x >= backX &&
                    p.x <= backX + BACK_BTN_W &&
                    p.y >= BACK_BTN_Y &&
                    p.y <= BACK_BTN_Y + BACK_BTN_H;
  if (tappedBack && currentMode != MODE_TOTAL) {
    waitForTouchRelease();
    Serial.println("[Touch] Back");
    return 'B';
  }

  if (p.y >= screenH() - NAV_H) {
    waitForTouchRelease();
    if (p.x < screenW() / 2) {
      Serial.println("[Touch] Cart");
      return 'C';
    }
    Serial.println("[Touch] Recs");
    return 'R';
  }

  return '\0';
}
