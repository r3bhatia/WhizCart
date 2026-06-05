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

#define HEADER_H      34
#define STATUS_H      24
#define NAV_H         36
#define CONTENT_Y     (HEADER_H + STATUS_H)
#define ITEM_ROW_H    50
#define ITEM_START_Y  CONTENT_Y
#define MAX_CART_ROWS  4
#define DEL_BTN_W     46

#define BG_COLOR     TFT_BLACK
#define PANEL_COLOR  0x1082
#define BORDER_COLOR 0x4208
#define ACCENT       0x04B3
#define TEXT_MAIN    TFT_WHITE
#define TEXT_DIM     0xC618
#define PRICE_COLOR  0xA7E3
#define WARN_COLOR   0xFD20
#define DEL_COLOR    TFT_WHITE
#define DEL_BG       0xB000

#define TOUCH_X_MIN   200
#define TOUCH_X_MAX  3800
#define TOUCH_Y_MIN   300
#define TOUCH_Y_MAX  3700

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
  tft.fillRect(0, 0, screenW(), HEADER_H, ACCENT);
  tft.setTextSize(2);
  tft.setTextColor(TFT_BLACK, ACCENT);
  tft.setCursor(8, 9);
  tft.print("WhizCart");
}

void drawStatus(String msg) {
  tft.fillRect(0, HEADER_H, screenW(), STATUS_H, BG_COLOR);
  tft.setTextSize(1);
  tft.setTextColor(TEXT_DIM, BG_COLOR);
  tft.setCursor(8, HEADER_H + 8);
  tft.print(fitText(msg, 36));
}

void clearContent() {
  tft.fillRect(0, CONTENT_Y, screenW(), screenH() - CONTENT_Y, BG_COLOR);
}

void drawNavBar() {
  int y = screenH() - NAV_H;
  tft.fillRect(0, y, screenW(), NAV_H, PANEL_COLOR);
  tft.drawLine(screenW() / 2, y, screenW() / 2, screenH(), BORDER_COLOR);

  tft.setTextSize(2);
  tft.setTextColor(TEXT_MAIN, PANEL_COLOR);
  tft.setCursor(30, y + 10);
  tft.print("Cart");
  tft.setCursor(150, y + 10);
  tft.print("Recs");
}

void drawBackHint() {
  tft.fillRoundRect(160, 6, 72, 22, 5, PANEL_COLOR);
  tft.setTextSize(1);
  tft.setTextColor(TEXT_MAIN, PANEL_COLOR);
  tft.setCursor(176, 13);
  tft.print("BACK");
}

ScreenPoint getTouchPoint() {
  if (!ts.tirqTouched() || !ts.touched()) return { 0, 0, false };
  TS_Point p = ts.getPoint();
  int sx = map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, screenW());
  int sy = map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, screenH());
  sx = constrain(sx, 0, screenW() - 1);
  sy = constrain(sy, 0, screenH() - 1);
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

  tft.setTextSize(2);
  tft.setTextColor(TEXT_DIM, BG_COLOR);
  tft.setCursor(14, CONTENT_Y + 10);
  tft.print("Running Total");

  tft.fillRoundRect(10, CONTENT_Y + 42, screenW() - 20, 92, 8, PANEL_COLOR);
  tft.setTextSize(4);
  tft.setTextColor(PRICE_COLOR, PANEL_COLOR);
  tft.setCursor(20, CONTENT_Y + 72);
  tft.print("$");
  tft.print(total, 2);

  tft.fillRoundRect(10, CONTENT_Y + 150, screenW() - 20, 56, 8, 0x0841);
  tft.setTextSize(1);
  tft.setTextColor(TEXT_DIM, 0x0841);
  tft.setCursor(18, CONTENT_Y + 166);
  if (measuredWeightG >= 0) {
    tft.print("Cart weight: ");
    tft.print(measuredWeightG, 1);
    tft.print("g");
  } else {
    tft.print("Scan an item to begin");
  }
  tft.setCursor(18, CONTENT_Y + 186);
  tft.print("Use bottom buttons for views");

  drawNavBar();
}

void display_showItem(String name, float price, float weightG, float total) {
  tft.fillScreen(BG_COLOR);
  drawHeader();
  drawStatus("Added item");

  tft.fillRoundRect(10, CONTENT_Y + 12, screenW() - 20, 108, 8, PANEL_COLOR);
  tft.setTextSize(2);
  tft.setTextColor(TEXT_MAIN, PANEL_COLOR);
  tft.setCursor(18, CONTENT_Y + 28);
  tft.print(fitText(name, 18));

  tft.setTextColor(PRICE_COLOR, PANEL_COLOR);
  tft.setCursor(18, CONTENT_Y + 62);
  tft.print("$");
  tft.print(price, 2);

  if (weightG > 0) {
    tft.setTextSize(1);
    tft.setTextColor(TEXT_DIM, PANEL_COLOR);
    tft.setCursor(18, CONTENT_Y + 92);
    tft.print("Expected weight: ");
    tft.print(weightG, 0);
    tft.print("g");
  }

  tft.fillRoundRect(10, CONTENT_Y + 140, screenW() - 20, 58, 8, 0x0841);
  tft.setTextSize(1);
  tft.setTextColor(TEXT_DIM, 0x0841);
  tft.setCursor(18, CONTENT_Y + 154);
  tft.print("TOTAL");
  tft.setTextSize(3);
  tft.setTextColor(TEXT_MAIN, 0x0841);
  tft.setCursor(18, CONTENT_Y + 172);
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
    tft.fillRoundRect(10, CONTENT_Y + 30, screenW() - 20, 126, 8, PANEL_COLOR);
    tft.setTextSize(2);
    tft.setTextColor(TEXT_DIM, PANEL_COLOR);
    tft.setCursor(36, CONTENT_Y + 76);
    tft.print("Cart is empty");
    tft.setTextSize(1);
    tft.setCursor(54, CONTENT_Y + 108);
    tft.print("Scan something");
    drawNavBar();
    return;
  }

  int footerY = screenH() - NAV_H - 44;
  int rows = min((int)items.size(), min(MAX_CART_ROWS, (footerY - ITEM_START_Y) / ITEM_ROW_H));

  for (int i = 0; i < rows; i++) {
    int y = ITEM_START_Y + i * ITEM_ROW_H;
    int rowBottom = y + ITEM_ROW_H - 2;

    tft.setTextSize(1);
    tft.setTextColor(TEXT_MAIN, BG_COLOR);
    tft.setCursor(8, y + 7);
    tft.print(fitText(items[i].name, 24));

    tft.setTextColor(PRICE_COLOR, BG_COLOR);
    tft.setCursor(8, y + 27);
    tft.print("$");
    tft.print(items[i].price, 2);
    if (items[i].weightG > 0) {
      tft.setTextColor(TEXT_DIM, BG_COLOR);
      tft.print(" ");
      tft.print(items[i].weightG, 0);
      tft.print("g");
    }
    if (items[i].qty > 1) {
      tft.setTextColor(TEXT_DIM, BG_COLOR);
      tft.print(" qty ");
      tft.print(items[i].qty);
    }

    tft.fillRoundRect(screenW() - DEL_BTN_W - 6, y + 8, DEL_BTN_W, ITEM_ROW_H - 16, 6, DEL_BG);
    tft.setTextColor(DEL_COLOR, DEL_BG);
    tft.setCursor(screenW() - DEL_BTN_W + 3, y + 19);
    tft.print("DEL");

    tft.drawLine(0, rowBottom, screenW(), rowBottom, BORDER_COLOR);
  }

  tft.fillRoundRect(8, footerY, screenW() - 16, 36, 8, PANEL_COLOR);
  tft.setTextSize(1);
  tft.setTextColor(TEXT_MAIN, PANEL_COLOR);
  tft.setCursor(16, footerY + 8);
  tft.print("Total: $");
  tft.print(total, 2);
  tft.setTextColor(TEXT_DIM, PANEL_COLOR);
  tft.setCursor(16, footerY + 23);
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

  if (p.x < screenW() - DEL_BTN_W - 6) return "";

  int row = (p.y - ITEM_START_Y) / ITEM_ROW_H;
  if (row < 0 || row >= (int)items.size() || row >= MAX_CART_ROWS) return "";

  waitForTouchRelease();
  return items[row].barcode;
}

void display_showRecommendations(RecommendationList& recs) {
  tft.fillScreen(BG_COLOR);
  drawHeader();
  drawBackHint();
  drawStatus("Recommendations");

  tft.setTextSize(2);
  tft.setTextColor(ACCENT, BG_COLOR);
  tft.setCursor(8, CONTENT_Y + 8);
  tft.print("Recommended");

  if (recs.empty()) {
    tft.fillRoundRect(10, CONTENT_Y + 48, screenW() - 20, 96, 8, PANEL_COLOR);
    tft.setTextSize(1);
    tft.setTextColor(TEXT_DIM, PANEL_COLOR);
    tft.setCursor(22, CONTENT_Y + 88);
    tft.print("Add items to see suggestions.");
    drawNavBar();
    return;
  }

  int y = CONTENT_Y + 42;
  for (int i = 0; i < (int)recs.size() && i < 5; i++) {
    tft.fillRoundRect(8, y - 4, screenW() - 16, 38, 6, PANEL_COLOR);
    tft.setTextSize(1);
    tft.setTextColor(TEXT_MAIN, PANEL_COLOR);
    tft.setCursor(14, y + 2);
    tft.print(fitText(recs[i].name, 23));

    tft.setTextColor(PRICE_COLOR, PANEL_COLOR);
    tft.setCursor(178, y + 2);
    tft.print("$");
    tft.print(recs[i].price, 2);

    tft.setTextColor(TEXT_DIM, PANEL_COLOR);
    tft.setCursor(14, y + 18);
    tft.print("Aisle: ");
    tft.print(recs[i].aisle);

    y += 42;
  }

  drawNavBar();
}

void display_showWeightCheck(float measured, float expected, bool ok) {
  tft.fillScreen(BG_COLOR);
  drawHeader();
  drawStatus(ok ? "Weight verified" : "Weight mismatch");

  uint16_t statusColor = ok ? PRICE_COLOR : WARN_COLOR;
  uint16_t bgTint = ok ? 0x0440 : 0x4200;

  tft.fillRoundRect(8, CONTENT_Y + 10, screenW() - 16, 62, 8, bgTint);
  tft.setTextSize(2);
  tft.setTextColor(statusColor, bgTint);
  tft.setCursor(18, CONTENT_Y + 32);
  tft.print(ok ? "Weight OK" : "Check Weight");

  tft.fillRoundRect(8, CONTENT_Y + 88, screenW() - 16, 96, 8, PANEL_COLOR);
  tft.setTextSize(1);
  tft.setTextColor(TEXT_DIM, PANEL_COLOR);
  tft.setCursor(18, CONTENT_Y + 104);  tft.print("Measured:");
  tft.setTextColor(TEXT_MAIN, PANEL_COLOR);
  tft.setCursor(100, CONTENT_Y + 104); tft.print(measured, 1); tft.print("g");

  tft.setTextColor(TEXT_DIM, PANEL_COLOR);
  tft.setCursor(18, CONTENT_Y + 130);  tft.print("Expected:");
  tft.setTextColor(TEXT_MAIN, PANEL_COLOR);
  tft.setCursor(100, CONTENT_Y + 130); tft.print(expected, 1); tft.print("g");

  float diff = measured - expected;
  tft.setTextColor(TEXT_DIM, PANEL_COLOR);
  tft.setCursor(18, CONTENT_Y + 156);  tft.print("Diff:");
  tft.setTextColor(ok ? PRICE_COLOR : WARN_COLOR, PANEL_COLOR);
  tft.setCursor(100, CONTENT_Y + 156);
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

  if (p.x > 155 && p.y < HEADER_H + 4 && currentMode != MODE_TOTAL) {
    waitForTouchRelease();
    return 'B';
  }

  if (p.y > screenH() - NAV_H) {
    waitForTouchRelease();
    if (p.x < screenW() / 2) return 'C';
    return 'R';
  }

  return '\0';
}
