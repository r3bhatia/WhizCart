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
#define HEADER_H      24    // slightly smaller header
#define ITEM_ROW_H    36    // slightly smaller rows
#define ITEM_START_Y  30    // start items sooner
#define MAX_CART_ROWS  5
#define DEL_BTN_W     36    // width of the delete button column
#define SCREEN_W     320
#define SCREEN_H     240



// ── Color palette ─────────────────────────────────────────────────────────────
#define BG_COLOR    TFT_BLACK
#define ACCENT      0x04B3   // teal
#define TEXT_MAIN   TFT_WHITE
#define TEXT_DIM    TFT_DARKGREY
#define PRICE_COLOR 0xA7E3   // bright green
#define DEL_COLOR   TFT_RED
#define DEL_BG      0x3000   // dark red tint

// ── Touch calibration ─────────────────────────────────────────────────────────
// Raw touch values at screen corners for the CYD. If your taps feel off,
// adjust these by running a calibration sketch and printing raw TS_Point values.
#define TOUCH_X_MIN   300
#define TOUCH_X_MAX  3900
#define TOUCH_Y_MIN   200
#define TOUCH_Y_MAX  3800

TFT_eSPI tft = TFT_eSPI();
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

// ── Init ──────────────────────────────────────────────────────────────────────
void display_init() {
  // Display
  tft.init();
  tft.setRotation(0);
  tft.invertDisplay(true);
  tft.fillScreen(TFT_BLACK);
  delay(200);

  // Backlight
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);

  // Touch — MUST be initialized on separate VSPI bus
  touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  ts.begin(touchSPI);
  ts.setRotation(1);
  delay(100);

  Serial.println("Touch initialized");

  // Header
  tft.fillRect(0, 0, SCREEN_W, HEADER_H, ACCENT);
  tft.setTextSize(1);
  tft.setCursor(8, 8);
  tft.setTextColor(TFT_WHITE, ACCENT);
  tft.print("WhizCart");
}
// ── Helpers ───────────────────────────────────────────────────────────────────

// Map raw touch X/Y to screen pixel coordinates (landscape)
struct ScreenPoint { int x; int y; bool valid; };

ScreenPoint getTouchPoint() {
  if (!ts.tirqTouched() || !ts.touched()) return { 0, 0, false };
  TS_Point p = ts.getPoint();
  // Map raw → screen pixels
  int sx = map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, SCREEN_W);
  int sy = map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, SCREEN_H);
  sx = constrain(sx, 0, SCREEN_W - 1);
  sy = constrain(sy, 0, SCREEN_H - 1);
  return { sx, sy, true };
}

// ── Status line ───────────────────────────────────────────────────────────────
void display_showStatus(String msg) {
  tft.fillRect(0, HEADER_H, SCREEN_W, 18, BG_COLOR);
  tft.setTextSize(1);
  tft.setTextColor(TEXT_DIM, BG_COLOR);
  tft.setCursor(8, HEADER_H + 4);
  tft.print(msg);
}

// ── Running total (home screen) ───────────────────────────────────────────────
void display_showTotal(float total) {
  // Update header to show total
  //tft.invertDisplay(true);  // add this at the start

  tft.fillRect(0, 0, SCREEN_W, HEADER_H, ACCENT);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, ACCENT);
  tft.setCursor(8, 8);
  tft.print("WhizCart");
  tft.setCursor(180, 8);
  tft.print("$");
  tft.print(total, 2);

  // Main area
  tft.fillRect(0, HEADER_H, SCREEN_W, SCREEN_H - HEADER_H - 28, BG_COLOR);
  tft.setTextSize(3);
  tft.setTextColor(PRICE_COLOR, BG_COLOR);
  tft.setCursor(8, HEADER_H + 20);
  tft.print("$");
  tft.print(total, 2);

  tft.setTextSize(1);
  tft.setTextColor(TEXT_DIM, BG_COLOR);
  tft.setCursor(8, HEADER_H + 70);
  tft.print("Scan an item to begin");

  // Nav bar
  tft.fillRect(0, SCREEN_H - 28, SCREEN_W, 28, 0x1082);
  tft.drawLine(160, SCREEN_H - 28, 160, SCREEN_H, TEXT_DIM);
  tft.setTextColor(TFT_WHITE, 0x1082);
  tft.setCursor(52, SCREEN_H - 18); tft.print("[ CART ]");
  tft.setCursor(196, SCREEN_H - 18); tft.print("[ RECS ]");
}

// ── Just-scanned item flash ───────────────────────────────────────────────────
void display_showItem(String name, float price, float total) {
  //tft.invertDisplay(true);  // add this at the start

  tft.fillRect(0, HEADER_H, SCREEN_W, SCREEN_H - HEADER_H, BG_COLOR);

  tft.setTextSize(1);
  tft.setTextColor(TEXT_MAIN, BG_COLOR);
  tft.setCursor(8, HEADER_H + 6);
  tft.print("Added:");

  tft.setTextSize(2);
  tft.setTextColor(TEXT_MAIN, BG_COLOR);
  tft.setCursor(8, HEADER_H + 20);
  tft.print(name.substring(0, 22));

  tft.setTextSize(2);
  tft.setTextColor(PRICE_COLOR, BG_COLOR);
  tft.setCursor(8, HEADER_H + 44);
  tft.print("$");
  tft.print(price, 2);

  tft.drawLine(0, HEADER_H + 68, SCREEN_W, HEADER_H + 68, TEXT_DIM);

  tft.setTextSize(1);
  tft.setTextColor(TEXT_DIM, BG_COLOR);
  tft.setCursor(8, HEADER_H + 76);
  tft.print("TOTAL:");
  tft.setTextSize(3);
  tft.setTextColor(TFT_WHITE, BG_COLOR);
  tft.setCursor(8, HEADER_H + 90);
  tft.print("$");
  tft.print(total, 2);
}

// ── Cart list with tappable delete buttons ────────────────────────────────────
//
// Screen layout per row (landscape 320×240, rows start at Y=ITEM_START_Y):
//
//  ┌─────────────────────────────────┬──────┐
//  │ Item name           $x.xx  ×qty │  ✕   │
//  │ (row height = ITEM_ROW_H)       │      │
//  └─────────────────────────────────┴──────┘
//   0                             284    320
//
// The red ✕ zone is the rightmost DEL_BTN_W pixels of each row.

void display_showCartList(CartList& items, float total) {
  tft.invertDisplay(true);  // fixes static
  tft.fillRect(0, HEADER_H, SCREEN_W, SCREEN_H - HEADER_H, TFT_BLACK);

  // Update header to show we're in cart view
  tft.fillRect(0, 0, SCREEN_W, HEADER_H, ACCENT);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, ACCENT);
  tft.setCursor(8, 8);
  tft.print("WhizCart  |  Cart");
  tft.setCursor(220, 8);
  tft.print("< Back");

  if (items.empty()) {
    tft.setTextSize(1);
    tft.setTextColor(TEXT_DIM, TFT_BLACK);
    tft.setCursor(8, SCREEN_H / 2);
    tft.print("Cart is empty");
    return;
  }

  int rows = min((int)items.size(), MAX_CART_ROWS);
  for (int i = 0; i < rows; i++) {
    int y = ITEM_START_Y + i * ITEM_ROW_H;

    // Clear row background
    tft.fillRect(0, y, SCREEN_W, ITEM_ROW_H - 1, TFT_BLACK);

    // Red X button — right side, vertically centered in row
    tft.fillRoundRect(SCREEN_W - 32, y + 4, 28, ITEM_ROW_H - 10, 4, TFT_RED);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.setTextSize(2);
    tft.setCursor(SCREEN_W - 24, y + 9);
    tft.print("X");

    // Item name — truncated to avoid overlapping X button
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(4, y + 5);
    tft.print(items[i].name.substring(0, 22));

    // Price + qty on second line
    tft.setTextColor(PRICE_COLOR, TFT_BLACK);
    tft.setCursor(4, y + 19);
    tft.print("$");
    tft.print(items[i].price, 2);
    if (items[i].qty > 1) {
      tft.setTextColor(TEXT_DIM, TFT_BLACK);
      tft.print("  x");
      tft.print(items[i].qty);
    }

    // Row divider
    tft.drawLine(0, y + ITEM_ROW_H - 1, SCREEN_W - 34, y + ITEM_ROW_H - 1, 0x2104);
  }

  // Footer with total and overflow count
  tft.fillRect(0, SCREEN_H - 22, SCREEN_W, 22, 0x1082);
  tft.setTextSize(1);
  tft.setTextColor(TEXT_DIM, 0x1082);
  tft.setCursor(6, SCREEN_H - 14);
  if ((int)items.size() > MAX_CART_ROWS) {
    tft.print("+");
    tft.print(items.size() - MAX_CART_ROWS);
    tft.print(" more  ");
  }
  tft.setTextColor(TFT_WHITE, 0x1082);
  tft.print("Total: $");
  tft.print(total, 2);
}

// ── Touch handler for cart list ───────────────────────────────────────────────
// Call this every loop() iteration while the cart list screen is active.
// Returns the barcode of the tapped item, or "" if no tap detected.
String display_getCartTap(CartList& items) {
  ScreenPoint p = getTouchPoint();
  if (!p.valid) return "";

  Serial.printf("Touch at x=%d y=%d\n", p.x, p.y);

  // X button is at x > 250 (right side)
  if (p.x < 250) return "";

  // Map y to row
  int row = (p.y - ITEM_START_Y) / ITEM_ROW_H;
  Serial.printf("Delete row: %d\n", row);

  if (row < 0 || row >= (int)items.size() || row >= MAX_CART_ROWS) return "";

  while (ts.touched()) delay(10);
  delay(80);

  return items[row].barcode;
}

// ── Recommendations ───────────────────────────────────────────────────────────
void display_showRecommendations(RecommendationList& recs) {
  tft.fillRect(0, HEADER_H, SCREEN_W, SCREEN_H - HEADER_H, BG_COLOR);
  tft.setTextSize(1);
  tft.setTextColor(ACCENT, BG_COLOR);
  tft.setCursor(8, HEADER_H + 4);
  tft.print("-- You might also want --");

  int y = HEADER_H + 20;
  for (int i = 0; i < (int)recs.size() && i < 4; i++) {
    tft.setTextColor(TEXT_MAIN, BG_COLOR);
    tft.setCursor(8, y);
    tft.print(recs[i].name.substring(0, 26));

    tft.setTextColor(PRICE_COLOR, BG_COLOR);
    tft.setCursor(240, y);
    tft.print("$");
    tft.print(recs[i].price, 2);

    tft.setTextColor(TEXT_DIM, BG_COLOR);
    tft.setCursor(8, y + 13);
    tft.print("Aisle: ");
    tft.print(recs[i].aisle);

    y += 36;
    tft.drawLine(0, y - 4, SCREEN_W, y - 4, 0x2104);
  }
}

// ── Weight check ──────────────────────────────────────────────────────────────
void display_showWeightCheck(float measured, float expected, bool ok) {
  tft.fillRect(0, HEADER_H, SCREEN_W, SCREEN_H - HEADER_H, BG_COLOR);

  uint16_t statusColor = ok ? PRICE_COLOR : TFT_RED;
  uint16_t bgTint      = ok ? 0x0440 : 0x3000;

  tft.fillRoundRect(6, HEADER_H + 4, SCREEN_W - 12, 40, 8, bgTint);
  tft.setTextSize(2);
  tft.setTextColor(statusColor, bgTint);
  tft.setCursor(16, HEADER_H + 14);
  tft.print(ok ? "Weight OK" : "Weight Mismatch!");

  tft.setTextSize(1);
  tft.setTextColor(TEXT_DIM, BG_COLOR);
  tft.setCursor(8, HEADER_H + 54);  tft.print("Measured:");
  tft.setTextColor(TEXT_MAIN, BG_COLOR);
  tft.setCursor(90, HEADER_H + 54); tft.print(measured, 1); tft.print("g");

  tft.setTextColor(TEXT_DIM, BG_COLOR);
  tft.setCursor(8, HEADER_H + 70);  tft.print("Expected:");
  tft.setTextColor(TEXT_MAIN, BG_COLOR);
  tft.setCursor(90, HEADER_H + 70); tft.print(expected, 1); tft.print("g");

  float diff = measured - expected;
  tft.setTextColor(TEXT_DIM, BG_COLOR);
  tft.setCursor(8, HEADER_H + 86);  tft.print("Diff:    ");
  tft.setTextColor(ok ? PRICE_COLOR : TFT_RED, BG_COLOR);
  tft.setCursor(90, HEADER_H + 86);
  if (diff >= 0) tft.print("+");
  tft.print(diff, 1); tft.print("g");

  if (!ok) {
    tft.setTextColor(TFT_RED, BG_COLOR);
    tft.setCursor(8, HEADER_H + 108);
    tft.print("Please scan all items!");
  }
}

// ── Navigation tap handler ────────────────────────────────────────────────────
// Bottom strip (y > 210) is split into nav zones:
//   Left  half (x < 160): "Cart"  → returns 'C'
//   Right half (x >= 160): "Recs" → returns 'R'
// Top-left corner (x < 50, y < 60): "Back"  → returns 'B'
// Returns '\0' if no tap detected.
//
// Call this every loop() from any mode.
// Also draws the nav bar when the total screen is first rendered
// (display_showTotal calls this area implicitly via the hint text).

char display_getNavTap(Mode currentMode) {
  ScreenPoint p = getTouchPoint();
  if (!p.valid) return '\0';
  Serial.printf("Touch detected at x=%d y=%d\n", p.x, p.y);  // add this

  // Back tap: top-left corner (the WhizCart header acts as a back button
  // when in cart or recs mode)
  if (p.x < 80 && p.y < HEADER_H + 4 && currentMode != MODE_TOTAL) {
    while (ts.touched()) delay(10);
    delay(80);
    return 'B';
  }

  // Nav strip at bottom of screen (y > 210)
  if (p.y > 210) {
    while (ts.touched()) delay(10);
    delay(80);
    if (p.x < 160) return 'C'; // left half → Cart
    else            return 'R'; // right half → Recs
  }

  return '\0';
}
