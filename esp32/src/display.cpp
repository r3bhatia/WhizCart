// firmware/src/display.cpp
// Arduino_GFX + GT911 driver for 7-inch ESP32-S3 800x480 capacitive displays.

#include "display.h"
#include <Arduino_GFX_Library.h>
#include <TAMC_GT911.h>
#include <Wire.h>
#include <qrcode.h>

// 7-inch ESP32-S3 RGB panel pins, commonly used by ESP32-8048S070C boards.
#define GFX_BL 2
#define GFX_BL_CHANNEL 0
#define GFX_BL_BRIGHTNESS 240

#define TOUCH_SDA 19
#define TOUCH_SCL 20
#define TOUCH_INT 18
#define TOUCH_RST 38

#define SCREEN_W 800
#define SCREEN_H 480

#define HEADER_H 56
#define STATUS_H 40
#define NAV_H 58
#define CONTENT_Y (HEADER_H + STATUS_H)
#define ITEM_ROW_H 72
#define ITEM_START_Y CONTENT_Y
#define MAX_CART_ROWS 4
#define DEL_BTN_W 96
#define PAY_BTN_W 132
#define PAY_BTN_H 40
#define QR_BTN_X 184
#define QR_BTN_Y 10
#define QR_BTN_W 72
#define QR_BTN_H 36

#define BACK_BTN_W 112
#define BACK_BTN_H 36
#define BACK_BTN_RIGHT 24
#define BACK_BTN_Y 10

#define RGB565(r, g, b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

#define BG_COLOR RGB565(247, 251, 244)
#define PANEL_COLOR 0xFFFF
#define SOFT_GREEN RGB565(238, 248, 232)
#define BORDER_COLOR RGB565(220, 235, 213)
#define ACCENT RGB565(34, 197, 94)
#define ACCENT_DARK RGB565(21, 128, 61)
#define HARVEST RGB565(250, 204, 21)
#define TEXT_MAIN RGB565(23, 50, 31)
#define TEXT_DIM RGB565(111, 127, 114)
#define PRICE_COLOR RGB565(22, 163, 74)
#define WARN_COLOR RGB565(239, 68, 68)
#define DEL_COLOR RGB565(239, 68, 68)
#define DEL_BG RGB565(255, 226, 226)

Arduino_ESP32RGBPanel* rgbpanel = new Arduino_ESP32RGBPanel(
  41 /* DE */, 40 /* VSYNC */, 39 /* HSYNC */, 42 /* PCLK */,
  14, 21, 47, 48, 45,  // R0-R4
  9, 46, 3, 8, 16, 1,  // G0-G5
  15, 7, 6, 5, 4,      // B0-B4
  0 /* hsync_polarity */, 180 /* hfp */, 30 /* hpw */, 16 /* hbp */,
  0 /* vsync_polarity */, 12 /* vfp */, 13 /* vpw */, 10 /* vbp */,
  1 /* pclk_active_neg */, 16000000 /* pclk */,
  false /* big endian */,
  0 /* de_idle_high */, 0 /* pclk_idle_high */, 0 /* bounce_buffer_size_px */
);

Arduino_RGB_Display* gfx = new Arduino_RGB_Display(
  SCREEN_W,
  SCREEN_H,
  rgbpanel,
  0 /* rotation */,
  true /* auto_flush */
);

TAMC_GT911 ts = TAMC_GT911(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, SCREEN_W, SCREEN_H);
bool touchReady = false;

struct ScreenPoint {
  int x;
  int y;
  bool valid;
};

int screenW() {
  return gfx->width();
}

int screenH() {
  return gfx->height();
}

String fitText(String text, int maxChars) {
  if ((int)text.length() <= maxChars) return text;
  return text.substring(0, maxChars - 3) + "...";
}

bool i2cDevicePresent(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

void logTouchBus() {
  Serial.printf("[Touch] I2C scan SDA=%d SCL=%d: 0x14=%s 0x5D=%s\n",
                TOUCH_SDA,
                TOUCH_SCL,
                i2cDevicePresent(GT911_ADDR2) ? "yes" : "no",
                i2cDevicePresent(GT911_ADDR1) ? "yes" : "no");
}

uint8_t findTouchAddress() {
  bool has14 = i2cDevicePresent(GT911_ADDR2);
  bool has5D = i2cDevicePresent(GT911_ADDR1);

  if (has5D) return GT911_ADDR1;
  if (has14) return GT911_ADDR2;

  Serial.print("[Touch] No GT911 at 0x14 or 0x5D. I2C devices found:");
  bool foundAny = false;
  for (uint8_t address = 1; address < 127; address++) {
    if (i2cDevicePresent(address)) {
      Serial.printf(" 0x%02X", address);
      foundAny = true;
    }
  }
  if (!foundAny) Serial.print(" none");
  Serial.println();
  return 0;
}

void drawHeader() {
  gfx->fillRect(0, 0, screenW(), HEADER_H, PANEL_COLOR);
  gfx->drawLine(0, HEADER_H - 1, screenW(), HEADER_H - 1, BORDER_COLOR);
  gfx->setTextSize(3);
  gfx->setTextColor(ACCENT_DARK, PANEL_COLOR);
  gfx->setCursor(16, 16);
  gfx->print("WhizCart");

  gfx->fillRoundRect(QR_BTN_X, QR_BTN_Y, QR_BTN_W, QR_BTN_H, 6, ACCENT);
  gfx->setTextSize(2);
  gfx->setTextColor(0xFFFF, ACCENT);
  gfx->setCursor(QR_BTN_X + 22, QR_BTN_Y + 11);
  gfx->print("QR");
}

void drawStatus(String msg) {
  gfx->fillRect(0, HEADER_H, screenW(), STATUS_H, SOFT_GREEN);
  gfx->setTextSize(2);
  gfx->setTextColor(TEXT_DIM, SOFT_GREEN);
  gfx->setCursor(16, HEADER_H + 12);
  gfx->print(fitText(msg, 44));
}

void drawNavBar() {
  int y = screenH() - NAV_H;
  gfx->fillRect(0, y, screenW(), NAV_H, SOFT_GREEN);
  gfx->drawLine(0, y, screenW(), y, BORDER_COLOR);
  gfx->drawLine(screenW() / 2, y, screenW() / 2, screenH(), BORDER_COLOR);

  gfx->setTextSize(3);
  gfx->setTextColor(ACCENT_DARK, SOFT_GREEN);
  gfx->setCursor((screenW() / 4) - 42, y + 18);
  gfx->print("Cart");
  gfx->setCursor((screenW() * 3 / 4) - 42, y + 18);
  gfx->print("Recs");
}

void drawBackHint() {
  int x = screenW() - BACK_BTN_W - BACK_BTN_RIGHT;
  gfx->fillRoundRect(x, BACK_BTN_Y, BACK_BTN_W, BACK_BTN_H, 6, HARVEST);
  gfx->setTextSize(2);
  gfx->setTextColor(TEXT_MAIN, HARVEST);
  gfx->setCursor(x + 28, BACK_BTN_Y + 11);
  gfx->print("BACK");
}

ScreenPoint getTouchPoint() {
  if (!touchReady) return { 0, 0, false };

  ts.read();
  if (!ts.isTouched) return { 0, 0, false };

  int sx = ts.points[0].x;
  int sy = ts.points[0].y;
  sx = screenW() - 1 - sx;
  sy = screenH() - 1 - sy;
  sx = constrain(sx, 0, screenW() - 1);
  sy = constrain(sy, 0, screenH() - 1);

  static unsigned long lastTouchLogMs = 0;
  unsigned long now = millis();
  if (now - lastTouchLogMs > 250) {
    Serial.printf("[Touch] screen=(%d,%d)\n", sx, sy);
    lastTouchLogMs = now;
  }

  return { sx, sy, true };
}

void waitForTouchRelease() {
  do {
    delay(10);
    ts.read();
  } while (ts.isTouched);
  delay(25);
}

void display_init() {
  ledcSetup(GFX_BL_CHANNEL, 5000, 8);
  ledcAttachPin(GFX_BL, GFX_BL_CHANNEL);
  ledcWrite(GFX_BL_CHANNEL, GFX_BL_BRIGHTNESS);

  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  logTouchBus();

  uint8_t touchAddress = findTouchAddress();
  if (touchAddress != 0) {
    Serial.printf("[Touch] Starting GT911 at 0x%02X\n", touchAddress);
    ts.begin(touchAddress);
    delay(100);
    ts.setRotation(ROTATION_NORMAL);
    touchReady = true;
    logTouchBus();
  } else {
    Serial.println("[Touch] Touch disabled because no GT911 controller was found.");
  }

  if (!gfx->begin()) {
    Serial.println("[Display] Arduino_GFX init failed");
  }

  gfx->fillScreen(BG_COLOR);
  Serial.printf("[Display] 7-inch init width=%d height=%d\n", screenW(), screenH());

  drawHeader();
  display_showStatus("Ready");
}

void display_showStatus(String msg) {
  drawStatus(msg);
}

void display_showTotal(float total, float measuredWeightG) {
  gfx->fillScreen(BG_COLOR);
  drawHeader();
  drawStatus("Ready to scan");

  int navY = screenH() - NAV_H;
  int contentH = navY - CONTENT_Y;

  gfx->fillRect(0, CONTENT_Y, screenW(), contentH, SOFT_GREEN);

  gfx->setTextSize(3);
  gfx->setTextColor(TEXT_DIM, SOFT_GREEN);
  gfx->setCursor(24, CONTENT_Y + 24);
  gfx->print("Running Total");

  gfx->fillRect(0, CONTENT_Y + 82, screenW(), 150, PANEL_COLOR);
  gfx->setTextSize(7);
  gfx->setTextColor(ACCENT_DARK, PANEL_COLOR);
  gfx->setCursor(24, CONTENT_Y + 122);
  gfx->print("$");
  gfx->print(total, 2);

  gfx->fillRect(0, CONTENT_Y + 232, screenW(), contentH - 232, RGB565(221, 255, 204));
  gfx->setTextSize(3);
  gfx->setTextColor(TEXT_DIM, RGB565(221, 255, 204));
  gfx->setCursor(24, CONTENT_Y + 264);
  if (measuredWeightG >= 0) {
    gfx->print("Cart weight: ");
    gfx->print(measuredWeightG, 1);
    gfx->print("g");
  } else {
    gfx->print("Scan an item to begin");
  }
  gfx->setTextSize(2);
  gfx->setCursor(24, CONTENT_Y + 310);
  gfx->print("Tap Cart or Recs");

  drawNavBar();
}

void display_showItem(String name, float price, float weightG, float total) {
  gfx->fillScreen(BG_COLOR);
  drawHeader();
  drawStatus("Added item");

  int navY = screenH() - NAV_H;
  gfx->fillRect(0, CONTENT_Y, screenW(), navY - CONTENT_Y, SOFT_GREEN);

  gfx->setTextSize(4);
  gfx->setTextColor(TEXT_MAIN, SOFT_GREEN);
  gfx->setCursor(24, CONTENT_Y + 34);
  gfx->print(fitText(name, 24));

  gfx->setTextSize(4);
  gfx->setTextColor(PRICE_COLOR, SOFT_GREEN);
  gfx->setCursor(24, CONTENT_Y + 100);
  gfx->print("$");
  gfx->print(price, 2);

  if (weightG > 0) {
    gfx->setTextSize(3);
    gfx->setTextColor(TEXT_DIM, SOFT_GREEN);
    gfx->setCursor(24, CONTENT_Y + 152);
    gfx->print("Weight: ");
    gfx->print(weightG, 0);
    gfx->print("g");
  }

  gfx->fillRect(0, CONTENT_Y + 218, screenW(), navY - (CONTENT_Y + 218), PANEL_COLOR);
  gfx->setTextSize(3);
  gfx->setTextColor(TEXT_DIM, PANEL_COLOR);
  gfx->setCursor(24, CONTENT_Y + 242);
  gfx->print("TOTAL");
  gfx->setTextSize(5);
  gfx->setTextColor(ACCENT_DARK, PANEL_COLOR);
  gfx->setCursor(24, CONTENT_Y + 286);
  gfx->print("$");
  gfx->print(total, 2);

  drawNavBar();
}

void display_showPendingItem(String name, float weightG) {
  gfx->fillScreen(BG_COLOR);
  drawHeader();
  drawStatus("Waiting for weight");

  int navY = screenH() - NAV_H;
  gfx->fillRect(0, CONTENT_Y, screenW(), navY - CONTENT_Y, SOFT_GREEN);

  gfx->setTextSize(4);
  gfx->setTextColor(TEXT_MAIN, SOFT_GREEN);
  gfx->setCursor(24, CONTENT_Y + 32);
  gfx->print("Place item");

  gfx->setTextSize(3);
  gfx->setTextColor(TEXT_MAIN, SOFT_GREEN);
  gfx->setCursor(24, CONTENT_Y + 96);
  gfx->print(fitText(name, 28));

  gfx->fillRect(0, CONTENT_Y + 160, screenW(), 114, PANEL_COLOR);
  gfx->setTextSize(3);
  gfx->setTextColor(TEXT_DIM, PANEL_COLOR);
  gfx->setCursor(24, CONTENT_Y + 188);
  gfx->print("Expected weight");

  gfx->setTextSize(4);
  gfx->setTextColor(ACCENT_DARK, PANEL_COLOR);
  gfx->setCursor(24, CONTENT_Y + 228);
  gfx->print(weightG, 0);
  gfx->print("g");

  gfx->setTextSize(2);
  gfx->setTextColor(TEXT_DIM, SOFT_GREEN);
  gfx->setCursor(24, CONTENT_Y + 308);
  gfx->print("Item is added only after weight matches.");

  drawNavBar();
}

void display_showCartList(CartList& items, float total, float measuredWeightG) {
  gfx->fillScreen(BG_COLOR);
  drawHeader();
  drawBackHint();
  drawStatus("Cart items");

  if (items.empty()) {
    int navY = screenH() - NAV_H;
    gfx->fillRect(0, CONTENT_Y, screenW(), navY - CONTENT_Y, SOFT_GREEN);
    gfx->setTextSize(3);
    gfx->setTextColor(TEXT_DIM, SOFT_GREEN);
    gfx->setCursor((screenW() - 240) / 2, CONTENT_Y + 120);
    gfx->print("Cart is empty");
    gfx->setCursor((screenW() - 260) / 2, CONTENT_Y + 170);
    gfx->print("Scan something");
    drawNavBar();
    return;
  }

  int footerY = screenH() - NAV_H - 56;
  int rows = min((int)items.size(), min(MAX_CART_ROWS, (footerY - ITEM_START_Y) / ITEM_ROW_H));

  for (int i = 0; i < rows; i++) {
    int y = ITEM_START_Y + i * ITEM_ROW_H;
    int rowBottom = y + ITEM_ROW_H - 2;
    uint16_t rowColor = (i % 2 == 0) ? PANEL_COLOR : SOFT_GREEN;

    gfx->fillRect(0, y, screenW(), ITEM_ROW_H - 2, rowColor);

    gfx->setTextSize(2);
    gfx->setTextColor(TEXT_MAIN, rowColor);
    gfx->setCursor(16, y + 12);
    gfx->print(fitText(items[i].name, 32));

    gfx->setTextColor(PRICE_COLOR, rowColor);
    gfx->setCursor(16, y + 42);
    gfx->print("$");
    gfx->print(items[i].price, 2);
    if (items[i].weightG > 0) {
      gfx->setTextColor(TEXT_DIM, rowColor);
      gfx->print("  ");
      gfx->print(items[i].weightG, 0);
      gfx->print("g");
    }
    if (items[i].qty > 1) {
      gfx->setTextColor(TEXT_DIM, rowColor);
      gfx->print("  qty ");
      gfx->print(items[i].qty);
    }

    gfx->fillRect(screenW() - DEL_BTN_W, y, DEL_BTN_W, ITEM_ROW_H - 2, DEL_BG);
    gfx->setTextSize(2);
    gfx->setTextColor(DEL_COLOR, DEL_BG);
    gfx->setCursor(screenW() - DEL_BTN_W + 25, y + 28);
    gfx->print("DEL");

    gfx->drawLine(0, rowBottom, screenW(), rowBottom, BORDER_COLOR);
  }

  gfx->fillRect(0, footerY, screenW(), 56, PANEL_COLOR);
  gfx->drawLine(0, footerY, screenW(), footerY, BORDER_COLOR);
  gfx->setTextSize(2);
  gfx->setTextColor(TEXT_MAIN, PANEL_COLOR);
  gfx->setCursor(16, footerY + 10);
  gfx->print("Total: $");
  gfx->print(total, 2);
  gfx->setTextColor(TEXT_DIM, PANEL_COLOR);
  gfx->setCursor(16, footerY + 34);
  if (measuredWeightG >= 0) {
    gfx->print("Weight: ");
    gfx->print(measuredWeightG, 0);
    gfx->print("g");
  } else {
    gfx->print("Tap BACK for total");
  }

  gfx->fillRoundRect(screenW() - PAY_BTN_W - 16, footerY + 8, PAY_BTN_W, PAY_BTN_H, 8, ACCENT);
  gfx->setTextSize(2);
  gfx->setTextColor(0xFFFF, ACCENT);
  gfx->setCursor(screenW() - PAY_BTN_W + 26, footerY + 21);
  gfx->print("PAY");

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
  gfx->fillScreen(BG_COLOR);
  drawHeader();
  drawBackHint();
  drawStatus("Recommendations");

  int navY = screenH() - NAV_H;
  gfx->fillRect(0, CONTENT_Y, screenW(), navY - CONTENT_Y, BG_COLOR);

  gfx->setTextSize(3);
  gfx->setTextColor(ACCENT, BG_COLOR);
  gfx->setCursor(16, CONTENT_Y + 18);
  gfx->print("Recommended");

  if (recs.empty()) {
    gfx->fillRect(0, CONTENT_Y + 76, screenW(), navY - (CONTENT_Y + 76), SOFT_GREEN);
    gfx->setTextSize(3);
    gfx->setTextColor(TEXT_DIM, SOFT_GREEN);
    gfx->setCursor(24, CONTENT_Y + 140);
    gfx->print("Add items to see suggestions.");
    drawNavBar();
    return;
  }

  int y = CONTENT_Y + 78;
  for (int i = 0; i < (int)recs.size() && i < 5; i++) {
    uint16_t recBg = (i % 2 == 0) ? PANEL_COLOR : SOFT_GREEN;
    gfx->fillRect(0, y - 8, screenW(), 62, recBg);
    gfx->setTextSize(2);
    gfx->setTextColor(TEXT_MAIN, recBg);
    gfx->setCursor(16, y + 2);
    gfx->print(fitText(recs[i].name, 34));

    gfx->setTextColor(PRICE_COLOR, recBg);
    gfx->setCursor(screenW() - 150, y + 2);
    gfx->print("$");
    gfx->print(recs[i].price, 2);

    gfx->setTextSize(2);
    gfx->setTextColor(TEXT_DIM, recBg);
    gfx->setCursor(16, y + 32);
    gfx->print("Aisle: ");
    gfx->print(recs[i].aisle);

    y += 66;
  }

  drawNavBar();
}

void display_showWeightCheck(float measured, float expected, bool ok) {
  gfx->fillScreen(BG_COLOR);
  drawHeader();
  drawStatus(ok ? "Weight verified" : "Weight mismatch");

  uint16_t statusColor = ok ? PRICE_COLOR : WARN_COLOR;
  uint16_t bgTint = ok ? RGB565(221, 255, 204) : DEL_BG;

  gfx->fillRect(0, CONTENT_Y, screenW(), 104, bgTint);
  gfx->setTextSize(4);
  gfx->setTextColor(statusColor, bgTint);
  gfx->setCursor(24, CONTENT_Y + 38);
  gfx->print(ok ? "Weight OK" : "Check Weight");

  gfx->fillRect(0, CONTENT_Y + 104, screenW(), screenH() - NAV_H - (CONTENT_Y + 104), PANEL_COLOR);
  gfx->setTextSize(3);
  gfx->setTextColor(TEXT_DIM, PANEL_COLOR);
  gfx->setCursor(24, CONTENT_Y + 140);
  gfx->print("Measured:");
  gfx->setTextColor(TEXT_MAIN, PANEL_COLOR);
  gfx->setCursor(190, CONTENT_Y + 140);
  gfx->print(measured, 1);
  gfx->print("g");

  gfx->setTextColor(TEXT_DIM, PANEL_COLOR);
  gfx->setCursor(24, CONTENT_Y + 185);
  gfx->print("Expected:");
  gfx->setTextColor(TEXT_MAIN, PANEL_COLOR);
  gfx->setCursor(190, CONTENT_Y + 185);
  gfx->print(expected, 1);
  gfx->print("g");

  float diff = measured - expected;
  gfx->setTextColor(TEXT_DIM, PANEL_COLOR);
  gfx->setCursor(24, CONTENT_Y + 230);
  gfx->print("Diff:");
  gfx->setTextColor(ok ? PRICE_COLOR : WARN_COLOR, PANEL_COLOR);
  gfx->setCursor(190, CONTENT_Y + 230);
  if (diff >= 0) gfx->print("+");
  gfx->print(diff, 1);
  gfx->print("g");

  if (!ok) {
    gfx->setTextColor(WARN_COLOR, PANEL_COLOR);
    gfx->setCursor(24, CONTENT_Y + 290);
    gfx->print("Scan missing item or remove extra.");
  }

  drawNavBar();
}

void display_showPayment(float total, String status) {
  gfx->fillScreen(BG_COLOR);
  drawHeader();
  drawBackHint();
  drawStatus("Checkout");

  int navY = screenH() - NAV_H;
  gfx->fillRect(0, CONTENT_Y, screenW(), navY - CONTENT_Y, SOFT_GREEN);

  gfx->setTextSize(3);
  gfx->setTextColor(TEXT_DIM, SOFT_GREEN);
  gfx->setCursor(24, CONTENT_Y + 24);
  gfx->print("Amount due");

  gfx->fillRect(0, CONTENT_Y + 76, screenW(), 104, PANEL_COLOR);
  gfx->setTextSize(5);
  gfx->setTextColor(ACCENT_DARK, PANEL_COLOR);
  gfx->setCursor(24, CONTENT_Y + 112);
  gfx->print("$");
  gfx->print(total, 2);

  int buttonY = CONTENT_Y + 220;
  int buttonX = 24;
  int buttonW = screenW() - 48;
  gfx->fillRoundRect(buttonX, buttonY, buttonW, 88, 8, ACCENT);
  gfx->drawRoundRect(buttonX, buttonY, buttonW, 88, 8, ACCENT_DARK);
  gfx->setTextSize(4);
  gfx->setTextColor(0xFFFF, ACCENT);
  gfx->setCursor(buttonX + (buttonW / 2) - 96, buttonY + 28);
  gfx->print("SHOW QR");

  if (status.length() > 0) {
    gfx->fillRect(0, buttonY + 124, screenW(), 50, PANEL_COLOR);
    gfx->setTextSize(2);
    gfx->setTextColor(status.indexOf("Paid") >= 0 ? ACCENT_DARK : TEXT_DIM, PANEL_COLOR);
    gfx->setCursor(24, buttonY + 142);
    gfx->print(fitText(status, 52));
  }

  drawNavBar();
}

void display_showCheckoutQr(String checkoutUrl, float total, String status) {
  gfx->fillScreen(BG_COLOR);
  drawHeader();
  drawBackHint();
  drawStatus(status.length() > 0 ? status : "Scan QR to pay");

  int navY = screenH() - NAV_H;
  gfx->fillRect(0, CONTENT_Y, screenW(), navY - CONTENT_Y, SOFT_GREEN);

  gfx->setTextSize(3);
  gfx->setTextColor(TEXT_MAIN, SOFT_GREEN);
  gfx->setCursor(24, CONTENT_Y + 24);
  gfx->print("Stripe Checkout");

  gfx->setTextSize(5);
  gfx->setTextColor(ACCENT_DARK, SOFT_GREEN);
  gfx->setCursor(24, CONTENT_Y + 76);
  gfx->print("$");
  gfx->print(total, 2);

  gfx->setTextSize(2);
  gfx->setTextColor(TEXT_DIM, SOFT_GREEN);
  gfx->setCursor(24, CONTENT_Y + 150);
  gfx->print("Scan with your phone camera");

  if (checkoutUrl.length() == 0) {
    gfx->setTextColor(WARN_COLOR, SOFT_GREEN);
    gfx->setCursor(24, CONTENT_Y + 200);
    gfx->print("No checkout URL received");
    drawNavBar();
    return;
  }

  QRCode qrcode;
  static uint8_t qrcodeData[3917];
  int qrResult = qrcode_initText(&qrcode, qrcodeData, 20, ECC_LOW, checkoutUrl.c_str());

  if (qrResult != 0) {
    gfx->setTextColor(WARN_COLOR, SOFT_GREEN);
    gfx->setCursor(24, CONTENT_Y + 200);
    gfx->print("QR URL is too long");
    drawNavBar();
    return;
  }

  int qrMax = min(320, navY - CONTENT_Y - 34);
  int moduleSize = max(2, qrMax / qrcode.size);
  int qrSize = qrcode.size * moduleSize;
  int qrX = screenW() - qrSize - 42;
  int qrY = CONTENT_Y + ((navY - CONTENT_Y - qrSize) / 2);
  int quiet = moduleSize * 4;

  gfx->fillRect(qrX - quiet, qrY - quiet, qrSize + quiet * 2, qrSize + quiet * 2, 0xFFFF);

  for (uint8_t y = 0; y < qrcode.size; y++) {
    for (uint8_t x = 0; x < qrcode.size; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        gfx->fillRect(qrX + x * moduleSize, qrY + y * moduleSize, moduleSize, moduleSize, 0x0000);
      }
    }
  }

  gfx->setTextSize(1);
  gfx->setTextColor(TEXT_DIM, SOFT_GREEN);
  gfx->setCursor(24, navY - 30);
  gfx->print(fitText(checkoutUrl, 70));

  drawNavBar();
}

void display_showBasketQr(String basketUrl, String cartId) {
  gfx->fillScreen(BG_COLOR);
  drawHeader();
  drawBackHint();
  drawStatus("Scan to join basket");

  int navY = screenH() - NAV_H;
  gfx->fillRect(0, CONTENT_Y, screenW(), navY - CONTENT_Y, SOFT_GREEN);

  gfx->setTextSize(2);
  gfx->setTextColor(TEXT_MAIN, SOFT_GREEN);
  gfx->setCursor(24, CONTENT_Y + 24);
  gfx->print("Open WhizCart");
  gfx->setCursor(24, CONTENT_Y + 52);
  gfx->print("on your phone");

  gfx->setTextSize(2);
  gfx->setTextColor(TEXT_DIM, SOFT_GREEN);
  gfx->setCursor(24, CONTENT_Y + 92);
  gfx->print("Basket ID: ");
  gfx->print(cartId);

  QRCode qrcode;
  static uint8_t qrcodeData[3917];
  int qrResult = qrcode_initText(&qrcode, qrcodeData, 20, ECC_LOW, basketUrl.c_str());
  if (qrResult != 0) {
    gfx->setTextColor(WARN_COLOR, SOFT_GREEN);
    gfx->setCursor(24, CONTENT_Y + 140);
    gfx->print("Basket URL too long for QR");
    drawNavBar();
    return;
  }

  int qrMax = min(320, navY - CONTENT_Y - 34);
  int moduleSize = max(2, qrMax / qrcode.size);
  int qrSize = qrcode.size * moduleSize;
  int qrX = screenW() - qrSize - 42;
  int qrY = CONTENT_Y + ((navY - CONTENT_Y - qrSize) / 2);
  int quiet = moduleSize * 4;

  gfx->fillRect(qrX - quiet, qrY - quiet, qrSize + quiet * 2, qrSize + quiet * 2, 0xFFFF);
  for (uint8_t y = 0; y < qrcode.size; y++) {
    for (uint8_t x = 0; x < qrcode.size; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        gfx->fillRect(qrX + x * moduleSize, qrY + y * moduleSize, moduleSize, moduleSize, 0x0000);
      }
    }
  }

  gfx->setTextSize(2);
  gfx->setTextColor(ACCENT_DARK, SOFT_GREEN);
  gfx->setCursor(24, CONTENT_Y + 148);
  gfx->print("Search, view cart, and checkout");

  int buttonX = 24;
  int buttonY = CONTENT_Y + 206;
  int buttonW = 230;
  int buttonH = 52;
  gfx->fillRoundRect(buttonX, buttonY, buttonW, buttonH, 8, WARN_COLOR);
  gfx->drawRoundRect(buttonX, buttonY, buttonW, buttonH, 8, WARN_COLOR);
  gfx->setTextSize(2);
  gfx->setTextColor(0xFFFF, WARN_COLOR);
  gfx->setCursor(buttonX + 28, buttonY + 18);
  gfx->print("Not right now");

  gfx->setTextSize(1);
  gfx->setTextColor(TEXT_DIM, SOFT_GREEN);
  gfx->setCursor(24, navY - 30);
  gfx->print(fitText(basketUrl, 70));

  drawNavBar();
}

char display_getBasketQrTap() {
  ScreenPoint p = getTouchPoint();
  if (!p.valid) return '\0';

  int backX = screenW() - BACK_BTN_W - BACK_BTN_RIGHT;
  if (p.x >= backX &&
      p.x <= backX + BACK_BTN_W &&
      p.y >= BACK_BTN_Y &&
      p.y <= BACK_BTN_Y + BACK_BTN_H) {
    waitForTouchRelease();
    Serial.println("[Touch] Basket QR back");
    return 'B';
  }

  int buttonX = 24;
  int buttonY = CONTENT_Y + 206;
  int buttonW = 230;
  int buttonH = 52;

  if (p.x >= buttonX &&
      p.x <= buttonX + buttonW &&
      p.y >= buttonY &&
      p.y <= buttonY + buttonH) {
    waitForTouchRelease();
    Serial.println("[Touch] Not right now");
    return 'N';
  }

  return '\0';
}

char display_getPaymentTap() {
  ScreenPoint p = getTouchPoint();
  if (!p.valid) return '\0';

  int buttonY = CONTENT_Y + 220;
  int buttonX = 24;
  int buttonW = screenW() - 48;

  if (p.y < buttonY || p.y > buttonY + 88) return '\0';

  if (p.x >= buttonX && p.x <= buttonX + buttonW) {
    waitForTouchRelease();
    Serial.println("[Touch] Show Stripe QR");
    return 'Q';
  }

  return '\0';
}

char display_getNavTap(Mode currentMode) {
  ScreenPoint p = getTouchPoint();
  if (!p.valid) return '\0';

  if (currentMode != MODE_BASKET_QR &&
      p.x >= QR_BTN_X &&
      p.x <= QR_BTN_X + QR_BTN_W &&
      p.y >= QR_BTN_Y &&
      p.y <= QR_BTN_Y + QR_BTN_H) {
    waitForTouchRelease();
    Serial.println("[Touch] Basket QR");
    return 'Q';
  }

  if (p.x >= 10 && p.x <= 170 && p.y >= 430 && p.y <= 479) {
    waitForTouchRelease();
    if (currentMode != MODE_TOTAL) {
      Serial.println("[Touch] Back lower-left");
      return 'B';
    }
    Serial.println("[Touch] Lower-left reserved");
    return '\0';
  }

  if (currentMode == MODE_CART && p.x >= 1 && p.x <= 180 && p.y >= 70 && p.y <= 110) {
    waitForTouchRelease();
    Serial.println("[Touch] Pay upper-left");
    return 'P';
  }

  if (currentMode != MODE_PAYMENT && p.y <= 80) {
    waitForTouchRelease();
    if (p.x < screenW() / 2) {
      Serial.println("[Touch] Recs top-mapped");
      return 'R';
    }
    Serial.println("[Touch] Cart top-mapped");
    return 'C';
  }

  if (currentMode == MODE_CART) {
    int footerY = screenH() - NAV_H - 56;
    int payX = screenW() - PAY_BTN_W - 16;
    if (p.x >= payX &&
        p.x <= payX + PAY_BTN_W &&
        p.y >= footerY + 8 &&
        p.y <= footerY + 8 + PAY_BTN_H) {
      waitForTouchRelease();
      Serial.println("[Touch] Pay");
      return 'P';
    }
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
