// firmware/src/display.cpp
// TFT + XPT2046 touch driver for the CYD (ESP32-2432S028R).
//
// Libraries needed (Arduino Library Manager):
//   - TFT_eSPI        by Bodmer          (display)
//   - XPT2046_Touchscreen by Paul Stoffregen (touch)
//
// Copy User_Setup.h into your TFT_eSPI library folder before building.

#include "display.h"
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

// ── CYD XPT2046 touch pins (separate SPI bus from display) ───────────────────
#define TOUCH_CS   33
#define TOUCH_IRQ  36   // T_IRQ — active LOW when screen is touched
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CLK  25

// ── Layout constants (landscape 320×240) ─────────────────────────────────────
#define HEADER_H      32
#define STATUS_H      22
#define NAV_H         32
#define CONTENT_Y     (HEADER_H + STATUS_H)
#define ITEM_ROW_H    42
#define ITEM_START_Y  CONTENT_Y
#define MAX_CART_ROWS  4
#define DEL_BTN_W     52
#define SCREEN_W     320
#define SCREEN_H     240

// ── Color palette ─────────────────────────────────────────────────────────────
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

// ── Touch calibration ─────────────────────────────────────────────────────────
// Raw touch values at screen corners for the CYD. If taps feel off, adjust these
// by running a calibration sketch and printing raw TS_Point values.
#define TOUCH_X_MIN   200
#define TOUCH_X_MAX  3800
#define TOUCH_Y_MIN   300
#define TOUCH_Y_MAX  3700

TFT_eSPI tft = TFT_eSPI();
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

// ── Helpers ───────────────────────────────────────────────────────────────────
struct ScreenPoint { int x; int y; bool valid; };

String fitText(String text, int maxChars) {
  if ((int)text.length() <= maxChars) return text;
  return text.substring(0, maxChars - 3) + "...";
}

void drawHeader() {
  tft.fillRect(0, 0, SCREEN_W, HEADER_H, ACCENT);
  tft.setTextSize(2);
  tft.setTextColor(TFT_BLACK, ACCENT);
  tft.setCursor(8, 8);
  tft.print("WhizCart");
}

void clearContent() {
  tft.fillRect(0, HEADER_H, SCREEN_W, SCREEN_H - HEADER_H, BG_COLOR);
}

void drawNavBar() {
  int y = SCREEN_H - NAV_H;
  tft.fillRect(0, y, SCREEN_W, NAV_H, PANEL_COLOR);
  tft.drawLine(SCREEN_W / 2, y, SCREEN_W / 2, SCREEN_H, BORDER_COLOR);

  tft.setTextSize(2);
  tft.setTextColor(TEXT_MAIN, PANEL_COLOR);
  tft.setCursor(44, y + 8);
  tft.print("Cart");
  tft.setCursor(196, y + 8);
  tft.print("Recs");
}

void drawBackHint() {
  tft.fillRoundRect(226, 5, 86, 22, 5, PANEL_COLOR);
  tft.setTextSize(1);
  tft.setTextColor(TEXT_MAIN, PANEL_COLOR);
  tft.setCursor(240, 12);
  tft.print("BACK");
}

ScreenPoint getTouchPoint() {
  if (!ts.tirqTouched() || !ts.touched()) return { 0, 0, false };
  TS_Point p = ts.getPoint();
  int sx = map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, SCREEN_W);
  int sy = map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, SCREEN_H);
  sx = constrain(sx, 0, SCREEN_W - 1);
  sy = constrain(sy, 0, SCREEN_H - 1);
  return { sx, sy, true };
}

void waitForTouchRelease() {
  while (ts.touched()) delay(10);
  delay(80);
}

// ── Init ──────────────────────────────────────────────────────────────────────
void display_init() {
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(BG_COLOR);

  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);

  touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  ts.begin(touchSPI);
  ts.setRotation(1);

  drawHeader();
  display_showStatus("Ready");
}

// ── Status line ───────────────────────────────────────────────────────────────
void display_showStatus(String msg) {
  tft.fillRect(0, HEADER_H, SCREEN_W, STATUS_H, BG_COLOR);
  tft.setTextSize(1);
  tft.setTextColor(TEXT_DIM, BG_COLOR);
  tft.setCursor(8, HEADER_H + 7);
  tft.print(fitText(msg, 48));
}

// ── Running total (home screen) ───────────────────────────────────────────────
void display_showTotal(float total) {
  drawHeader();
  clearContent();

  tft.setTextSize(2);
  tft.setTextColor(TEXT_DIM, BG_COLOR);
  tft.setCursor(8, CONTENT_Y + 4);
  tft.print("Total");

  tft.setTextSize(4);
  tft.setTextColor(PRICE_COLOR, BG_COLOR);
  tft.setCursor(8, CONTENT_Y + 30);
  tft.print("$");
  tft.print(total, 2);

  tft.setTextSize(1);
  tft.setTextColor(TEXT_DIM, BG_COLOR);
  tft.setCursor(8, CONTENT_Y + 92);
  tft.print("Tap a bottom button to switch views");

  drawNavBar();
}

// ── Just-scanned item flash ───────────────────────────────────────────────────
void display_showItem(String name, float price, float total) {
  drawHeader();
  clearContent();

  tft.setTextSize(2);
  tft.setTextColor(TEXT_MAIN, BG_COLOR);
  tft.setCursor(8, CONTENT_Y + 2);
  tft.print("Added");

  tft.setTextSize(2);
  tft.setTextColor(TEXT_MAIN, BG_COLOR);
  tft.setCursor(8, CONTENT_Y + 28);
  tft.print(fitText(name, 22));

  tft.setTextSize(2);
  tft.setTextColor(PRICE_COLOR, BG_COLOR);
  tft.setCursor(8, CONTENT_Y + 56);
  tft.print("$");
  tft.print(price, 2);

  tft.drawLine(0, CONTENT_Y + 84, SCREEN_W, CONTENT_Y + 84, BORDER_COLOR);

  tft.setTextSize(1);
  tft.setTextColor(TEXT_DIM, BG_COLOR);
  tft.setCursor(8, CONTENT_Y + 96);
  tft.print("TOTAL:");
  tft.setTextSize(3);
  tft.setTextColor(TEXT_MAIN, BG_COLOR);
  tft.setCursor(8, CONTENT_Y + 110);
  tft.print("$");
  tft.print(total, 2);
}

// ── Cart list with tappable delete buttons ────────────────────────────────────
void display_showCartList(CartList& items, float total) {
  drawHeader();
  drawBackHint();
  clearContent();

  if (items.empty()) {
    tft.setTextSize(2);
    tft.setTextColor(TEXT_DIM, BG_COLOR);
    tft.setCursor(40, SCREEN_H / 2 - 12);
    tft.print("Cart is empty");
    return;
  }

  int rows = min((int)items.size(), MAX_CART_ROWS);
  for (int i = 0; i < rows; i++) {
    int y = ITEM_START_Y + i * ITEM_ROW_H;
    int rowBottom = y + ITEM_ROW_H - 2;

    tft.setTextSize(1);
    tft.setTextColor(TEXT_MAIN, BG_COLOR);
    tft.setCursor(8, y + 5);
    tft.print(fitText(items[i].name, 28));

    tft.setTextColor(PRICE_COLOR, BG_COLOR);
    tft.setCursor(8, y + 22);
    tft.print("$");
    tft.print(items[i].price, 2);
    if (items[i].qty > 1) {
      tft.setTextColor(TEXT_DIM, BG_COLOR);
      tft.print(" qty ");
      tft.print(items[i].qty);
    }

    tft.fillRoundRect(SCREEN_W - DEL_BTN_W - 6, y + 4, DEL_BTN_W, ITEM_ROW_H - 10, 6, DEL_BG);
    tft.setTextColor(DEL_COLOR, DEL_BG);
    tft.setTextSize(1);
    tft.setCursor(SCREEN_W - DEL_BTN_W + 3, y + 10);
    tft.print("DEL");

    tft.drawLine(0, rowBottom, SCREEN_W, rowBottom, BORDER_COLOR);
  }

  tft.setTextSize(1);
  tft.setTextColor(TEXT_DIM, BG_COLOR);
  tft.setCursor(8, SCREEN_H - 18);
  if ((int)items.size() > MAX_CART_ROWS) {
    tft.print("+");
    tft.print(items.size() - MAX_CART_ROWS);
    tft.print(" more  ");
  }
  tft.setTextColor(TEXT_MAIN, BG_COLOR);
  tft.print("Total: $");
  tft.print(total, 2);
}

// ── Touch handler for cart list ───────────────────────────────────────────────
String display_getCartTap(CartList& items) {
  ScreenPoint p = getTouchPoint();
  if (!p.valid) return "";

  if (p.x < SCREEN_W - DEL_BTN_W - 6) return "";

  int row = (p.y - ITEM_START_Y) / ITEM_ROW_H;
  if (row < 0 || row >= (int)items.size() || row >= MAX_CART_ROWS) return "";

  waitForTouchRelease();
  return items[row].barcode;
}

// ── Recommendations ───────────────────────────────────────────────────────────
void display_showRecommendations(RecommendationList& recs) {
  drawHeader();
  drawBackHint();
  clearContent();

  tft.setTextSize(2);
  tft.setTextColor(ACCENT, BG_COLOR);
  tft.setCursor(8, CONTENT_Y);
  tft.print("Recommended");

  if (recs.empty()) {
    tft.setTextSize(1);
    tft.setTextColor(TEXT_DIM, BG_COLOR);
    tft.setCursor(8, CONTENT_Y + 28);
    tft.print("Add items to see suggestions.");
    return;
  }

  int y = CONTENT_Y + 30;
  for (int i = 0; i < (int)recs.size() && i < 4; i++) {
    tft.setTextColor(TEXT_MAIN, BG_COLOR);
    tft.setCursor(8, y);
    tft.print(fitText(recs[i].name, 25));

    tft.setTextColor(PRICE_COLOR, BG_COLOR);
    tft.setCursor(246, y);
    tft.print("$");
    tft.print(recs[i].price, 2);

    tft.setTextColor(TEXT_DIM, BG_COLOR);
    tft.setCursor(8, y + 13);
    tft.print("Aisle: ");
    tft.print(recs[i].aisle);

    y += 34;
    tft.drawLine(0, y - 4, SCREEN_W, y - 4, BORDER_COLOR);
  }
}

// ── Weight check ──────────────────────────────────────────────────────────────
void display_showWeightCheck(float measured, float expected, bool ok) {
  drawHeader();
  clearContent();

  uint16_t statusColor = ok ? PRICE_COLOR : WARN_COLOR;
  uint16_t bgTint = ok ? 0x0440 : 0x4200;

  tft.fillRoundRect(6, CONTENT_Y, SCREEN_W - 12, 44, 8, bgTint);
  tft.setTextSize(2);
  tft.setTextColor(statusColor, bgTint);
  tft.setCursor(16, CONTENT_Y + 13);
  tft.print(ok ? "Weight OK" : "Check Weight");

  tft.setTextSize(1);
  tft.setTextColor(TEXT_DIM, BG_COLOR);
  tft.setCursor(8, CONTENT_Y + 58);  tft.print("Measured:");
  tft.setTextColor(TEXT_MAIN, BG_COLOR);
  tft.setCursor(90, CONTENT_Y + 58); tft.print(measured, 1); tft.print("g");

  tft.setTextColor(TEXT_DIM, BG_COLOR);
  tft.setCursor(8, CONTENT_Y + 76);  tft.print("Expected:");
  tft.setTextColor(TEXT_MAIN, BG_COLOR);
  tft.setCursor(90, CONTENT_Y + 76); tft.print(expected, 1); tft.print("g");

  float diff = measured - expected;
  tft.setTextColor(TEXT_DIM, BG_COLOR);
  tft.setCursor(8, CONTENT_Y + 94);  tft.print("Diff:");
  tft.setTextColor(ok ? PRICE_COLOR : WARN_COLOR, BG_COLOR);
  tft.setCursor(90, CONTENT_Y + 94);
  if (diff >= 0) tft.print("+");
  tft.print(diff, 1);
  tft.print("g");

  if (!ok) {
    tft.setTextColor(WARN_COLOR, BG_COLOR);
    tft.setCursor(8, CONTENT_Y + 118);
    tft.print("Scan missing item or remove extra.");
  }
}

// ── Navigation tap handler ────────────────────────────────────────────────────
char display_getNavTap(Mode currentMode) {
  ScreenPoint p = getTouchPoint();
  if (!p.valid) return '\0';

  if (p.x > 220 && p.y < HEADER_H + 4 && currentMode != MODE_TOTAL) {
    waitForTouchRelease();
    return 'B';
  }

  if (p.y > SCREEN_H - NAV_H) {
    waitForTouchRelease();
    if (p.x < SCREEN_W / 2) return 'C';
    return 'R';
  }

  return '\0';
}
