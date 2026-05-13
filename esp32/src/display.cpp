// firmware/src/display.cpp
// TFT + XPT2046 touch driver for the CYD (ESP32-2432S028R).

#include "display.h"
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

// ── CYD XPT2046 touch pins ───────────────────────────────────────────────────
#define TOUCH_CS   33
#define TOUCH_IRQ  36
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CLK  25

// ── Layout constants: landscape 320×240 ─────────────────────────────────────
#define HEADER_H      34
#define STATUS_H      24
#define NAV_H         36
#define CONTENT_Y     (HEADER_H + STATUS_H)
#define ITEM_ROW_H    44
#define ITEM_START_Y  CONTENT_Y
#define MAX_CART_ROWS  3
#define DEL_BTN_W     64
#define SCREEN_W     240
#define SCREEN_H     320

// ── Accessible light color palette ───────────────────────────────────────────
#define BG_COLOR      TFT_WHITE
#define PANEL_COLOR   0xE71C      // light gray
#define CARD_COLOR    0xF7BE      // very light gray
#define BORDER_COLOR  0xC618      // medium gray
#define ACCENT        0x001F      // blue
#define TEXT_MAIN     TFT_BLACK
#define TEXT_DIM      0x4208      // dark gray
#define TEXT_ON_DARK  TFT_WHITE
#define PRICE_COLOR   TFT_BLACK   // avoids yellow/green-only meaning
#define WARN_COLOR    0xFD20      // orange
#define DEL_COLOR     TFT_WHITE
#define DEL_BG        0xB000      // red
#define SUCCESS_BG    0xD7F0      // light green/cyan
#define WARNING_BG    0xFEA0      // light orange

// ── Touch calibration ────────────────────────────────────────────────────────
#define TOUCH_X_MIN   200
#define TOUCH_X_MAX  3800
#define TOUCH_Y_MIN   300
#define TOUCH_Y_MAX  3700

TFT_eSPI tft = TFT_eSPI();
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

// ── Helpers ──────────────────────────────────────────────────────────────────
struct ScreenPoint {
  int x;
  int y;
  bool valid;
};

String fitText(String text, int maxChars) {
  if ((int)text.length() <= maxChars) return text;
  return text.substring(0, maxChars - 3) + "...";
}

void waitForTouchRelease() {
  while (ts.touched()) delay(10);
  delay(100);
}

void drawHeader(bool showBack = false, String title = "WhizCart") {
  tft.fillRect(0, 0, SCREEN_W, HEADER_H, ACCENT);

  tft.setTextSize(2);
  tft.setTextColor(TEXT_ON_DARK, ACCENT);
  tft.setCursor(8, 9);
  tft.print(title);

  if (showBack) {
    tft.fillRoundRect(228, 5, 84, 24, 6, TFT_WHITE);
    tft.setTextSize(1);
    tft.setTextColor(ACCENT, TFT_WHITE);
    tft.setCursor(250, 13);
    tft.print("BACK");
  }
}

void clearContent() {
  tft.fillRect(0, HEADER_H, SCREEN_W, SCREEN_H - HEADER_H, BG_COLOR);
}

void drawNavBar() {
  int y = SCREEN_H - NAV_H;

  tft.fillRect(0, y, SCREEN_W, NAV_H, PANEL_COLOR);
  tft.drawLine(SCREEN_W / 2, y + 4, SCREEN_W / 2, SCREEN_H - 4, BORDER_COLOR);

  tft.setTextSize(2);
  tft.setTextColor(TEXT_MAIN, PANEL_COLOR);

  tft.setCursor(44, y + 10);
  tft.print("Cart");

  tft.setCursor(190, y + 10);
  tft.print("Ideas");
}

ScreenPoint getTouchPoint() {
  if (!ts.tirqTouched() || !ts.touched()) return {0, 0, false};

  TS_Point p = ts.getPoint();

  int sx = map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, SCREEN_W);
  int sy = map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, SCREEN_H);

  sx = constrain(sx, 0, SCREEN_W - 1);
  sy = constrain(sy, 0, SCREEN_H - 1);

  return {sx, sy, true};
}

// ── Init ─────────────────────────────────────────────────────────────────────
void display_init() {
  tft.init();

  // Landscape mode.
  // If upside down, change both this and ts.setRotation(1) to 3.
  tft.setRotation(0);
  Serial.print("TFT width: ");
Serial.println(tft.width());

Serial.print("TFT height: ");
Serial.println(tft.height());

  // Use false first. If colors look inverted, try true.
  tft.invertDisplay(false);

  tft.fillScreen(BG_COLOR);
  delay(150);

  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);

  touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  ts.begin(touchSPI);
  ts.setRotation(1);

  drawHeader(false, "WhizCart");
  display_showStatus("Ready. Scan an item.");
}

// ── Status line ──────────────────────────────────────────────────────────────
void display_showStatus(String msg) {
  tft.fillRect(0, HEADER_H, SCREEN_W, STATUS_H, BG_COLOR);

  tft.setTextSize(1);
  tft.setTextColor(TEXT_DIM, BG_COLOR);
  tft.setCursor(8, HEADER_H + 8);
  tft.print(fitText(msg, 48));
}

// ── Running total screen ─────────────────────────────────────────────────────
void display_showTotal(float total) {
  drawHeader(false, "WhizCart");

  tft.fillRect(0, HEADER_H, SCREEN_W, SCREEN_H - HEADER_H - NAV_H, BG_COLOR);

  display_showStatus("Ready. Scan an item.");

  tft.setTextSize(2);
  tft.setTextColor(TEXT_DIM, BG_COLOR);
  tft.setCursor(12, CONTENT_Y + 8);
  tft.print("Current Total");

  tft.setTextSize(4);
  tft.setTextColor(TEXT_MAIN, BG_COLOR);
  tft.setCursor(12, CONTENT_Y + 38);
  tft.print("$");
  tft.print(total, 2);

  tft.fillRoundRect(12, CONTENT_Y + 104, SCREEN_W - 24, 42, 8, PANEL_COLOR);
  tft.setTextSize(2);
  tft.setTextColor(TEXT_MAIN, PANEL_COLOR);
  tft.setCursor(28, CONTENT_Y + 116);
  tft.print("Scan item to begin");

  drawNavBar();
}

// ── Just-scanned item screen ─────────────────────────────────────────────────
void display_showItem(String name, float price, float total) {
  drawHeader(false, "WhizCart");
  clearContent();

  tft.fillRoundRect(10, HEADER_H + 10, SCREEN_W - 20, 46, 8, SUCCESS_BG);

  tft.setTextSize(2);
  tft.setTextColor(TEXT_MAIN, SUCCESS_BG);
  tft.setCursor(22, HEADER_H + 22);
  tft.print("Item Added");

  tft.setTextSize(2);
  tft.setTextColor(TEXT_MAIN, BG_COLOR);
  tft.setCursor(12, HEADER_H + 76);
  tft.print(fitText(name, 22));

  tft.setTextSize(2);
  tft.setTextColor(TEXT_MAIN, BG_COLOR);
  tft.setCursor(12, HEADER_H + 106);
  tft.print("Price: $");
  tft.print(price, 2);

  tft.drawLine(12, HEADER_H + 138, SCREEN_W - 12, HEADER_H + 138, BORDER_COLOR);

  tft.setTextSize(2);
  tft.setTextColor(TEXT_DIM, BG_COLOR);
  tft.setCursor(12, HEADER_H + 154);
  tft.print("New Total");

  tft.setTextSize(3);
  tft.setTextColor(TEXT_MAIN, BG_COLOR);
  tft.setCursor(12, HEADER_H + 180);
  tft.print("$");
  tft.print(total, 2);
}

// ── Cart list with tappable delete buttons ───────────────────────────────────
void display_showCartList(CartList& items, float total) {
  drawHeader(true, "Cart");
  clearContent();

  if (items.empty()) {
    tft.setTextSize(2);
    tft.setTextColor(TEXT_MAIN, BG_COLOR);
    tft.setCursor(52, 104);
    tft.print("Cart is empty");

    tft.setTextSize(1);
    tft.setTextColor(TEXT_DIM, BG_COLOR);
    tft.setCursor(58, 132);
    tft.print("Scan an item to begin.");
    return;
  }

  int rows = min((int)items.size(), MAX_CART_ROWS);

  for (int i = 0; i < rows; i++) {
    int y = ITEM_START_Y + i * ITEM_ROW_H;

    tft.fillRoundRect(8, y + 2, SCREEN_W - 16, ITEM_ROW_H - 5, 8, CARD_COLOR);
    tft.drawRoundRect(8, y + 2, SCREEN_W - 16, ITEM_ROW_H - 5, 8, BORDER_COLOR);

    tft.setTextSize(1);
    tft.setTextColor(TEXT_MAIN, CARD_COLOR);
    tft.setCursor(16, y + 9);
    tft.print(fitText(items[i].name, 25));

    tft.setTextColor(TEXT_DIM, CARD_COLOR);
    tft.setCursor(16, y + 25);
    tft.print("$");
    tft.print(items[i].price, 2);

    if (items[i].qty > 1) {
      tft.print("  Qty ");
      tft.print(items[i].qty);
    }

    tft.fillRoundRect(SCREEN_W - DEL_BTN_W - 14, y + 8, DEL_BTN_W, 28, 6, DEL_BG);

    tft.setTextSize(1);
    tft.setTextColor(DEL_COLOR, DEL_BG);
    tft.setCursor(SCREEN_W - DEL_BTN_W - 1, y + 18);
    tft.print("DEL");
  }

  tft.fillRect(0, SCREEN_H - 28, SCREEN_W, 28, PANEL_COLOR);

  tft.setTextSize(1);
  tft.setTextColor(TEXT_MAIN, PANEL_COLOR);
  tft.setCursor(10, SCREEN_H - 19);

  if ((int)items.size() > MAX_CART_ROWS) {
    tft.print("+");
    tft.print(items.size() - MAX_CART_ROWS);
    tft.print(" more   ");
  }

  tft.print("Total: $");
  tft.print(total, 2);
}

// ── Touch handler for cart list ──────────────────────────────────────────────
String display_getCartTap(CartList& items) {
  ScreenPoint p = getTouchPoint();

  if (!p.valid) return "";

  if (p.x < SCREEN_W - DEL_BTN_W - 14) return "";

  int row = (p.y - ITEM_START_Y) / ITEM_ROW_H;

  if (row < 0 || row >= (int)items.size() || row >= MAX_CART_ROWS) return "";

  waitForTouchRelease();

  return items[row].barcode;
}

// ── Recommendations ──────────────────────────────────────────────────────────
void display_showRecommendations(RecommendationList& recs) {
  drawHeader(true, "Ideas");
  clearContent();

  tft.setTextSize(2);
  tft.setTextColor(TEXT_MAIN, BG_COLOR);
  tft.setCursor(10, CONTENT_Y - 14);
  tft.print("Suggested Items");

  if (recs.empty()) {
    tft.setTextSize(1);
    tft.setTextColor(TEXT_DIM, BG_COLOR);
    tft.setCursor(10, CONTENT_Y + 18);
    tft.print("Add items to see suggestions.");
    return;
  }

  int y = CONTENT_Y + 14;

  for (int i = 0; i < (int)recs.size() && i < 4; i++) {
    tft.fillRoundRect(8, y, SCREEN_W - 16, 34, 7, CARD_COLOR);
    tft.drawRoundRect(8, y, SCREEN_W - 16, 34, 7, BORDER_COLOR);

    tft.setTextSize(1);
    tft.setTextColor(TEXT_MAIN, CARD_COLOR);
    tft.setCursor(16, y + 6);
    tft.print(fitText(recs[i].name, 24));

    tft.setTextColor(TEXT_MAIN, CARD_COLOR);
    tft.setCursor(236, y + 6);
    tft.print("$");
    tft.print(recs[i].price, 2);

    tft.setTextColor(TEXT_DIM, CARD_COLOR);
    tft.setCursor(16, y + 20);
    tft.print("Aisle ");
    tft.print(recs[i].aisle);

    y += 40;
  }
}

// ── Weight check ─────────────────────────────────────────────────────────────
void display_showWeightCheck(float measured, float expected, bool ok) {
  drawHeader(false, "Weight");
  clearContent();

  uint16_t boxColor = ok ? SUCCESS_BG : WARNING_BG;

  tft.fillRoundRect(10, HEADER_H + 12, SCREEN_W - 20, 58, 8, boxColor);
  tft.drawRoundRect(10, HEADER_H + 12, SCREEN_W - 20, 58, 8, ok ? ACCENT : WARN_COLOR);

  tft.setTextSize(2);
  tft.setTextColor(TEXT_MAIN, boxColor);
  tft.setCursor(22, HEADER_H + 25);

  if (ok) {
    tft.print("OK: Match");
  } else {
    tft.print("Check Cart");
  }

  tft.setTextSize(1);
  tft.setTextColor(TEXT_DIM, boxColor);
  tft.setCursor(22, HEADER_H + 50);

  if (ok) {
    tft.print("Item weight looks correct.");
  } else {
    tft.print("Weight does not match scan.");
  }

  tft.setTextSize(2);
  tft.setTextColor(TEXT_MAIN, BG_COLOR);

  tft.setCursor(12, HEADER_H + 92);
  tft.print("Measured: ");
  tft.print(measured, 1);
  tft.print("g");

  tft.setCursor(12, HEADER_H + 122);
  tft.print("Expected: ");
  tft.print(expected, 1);
  tft.print("g");

  float diff = measured - expected;

  tft.setCursor(12, HEADER_H + 152);
  tft.print("Diff: ");

  if (diff >= 0) tft.print("+");

  tft.print(diff, 1);
  tft.print("g");

  if (!ok) {
    tft.setTextSize(1);
    tft.setTextColor(TEXT_MAIN, BG_COLOR);
    tft.setCursor(12, HEADER_H + 188);
    tft.print("Scan missing item or remove extra item.");
  }
}

// ── Navigation tap handler ──────────────────────────────────────────────────
char display_getNavTap(Mode currentMode) {
  ScreenPoint p = getTouchPoint();

  if (!p.valid) return '\0';

  if (p.x > 220 && p.y < HEADER_H + 8 && currentMode != MODE_TOTAL) {
    waitForTouchRelease();
    return 'B';
  }

  if (p.y >= SCREEN_H - NAV_H) {
    waitForTouchRelease();

    if (p.x < SCREEN_W / 2) return 'C';
    return 'R';
  }

  return '\0';
}