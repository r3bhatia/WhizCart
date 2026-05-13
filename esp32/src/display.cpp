#include "whizcart.h"

#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>

namespace {
constexpr uint8_t XPT2046_IRQ = 36;
constexpr uint8_t XPT2046_MOSI = 32;
constexpr uint8_t XPT2046_MISO = 39;
constexpr uint8_t XPT2046_CLK = 25;
constexpr uint8_t XPT2046_CS = 33;

constexpr int TOUCH_MIN_X = 200;
constexpr int TOUCH_MAX_X = 3700;
constexpr int TOUCH_MIN_Y = 240;
constexpr int TOUCH_MAX_Y = 3800;
constexpr uint16_t DRAW_BUFFER_PIXELS = 320 * 24;

TFT_eSPI tft;
SPIClass touchscreenSPI(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);
lv_color_t drawBuf[DRAW_BUFFER_PIXELS];

int screenW = 320;
int screenH = 240;
uint32_t lastTickMs = 0;
bool displayReady = false;

lv_obj_t* itemLabel = nullptr;
lv_obj_t* priceLabel = nullptr;
lv_obj_t* totalLabel = nullptr;
lv_obj_t* statusLabel = nullptr;
lv_obj_t* mainScreen = nullptr;
lv_obj_t* cartScreen = nullptr;
lv_obj_t* cartList = nullptr;
lv_obj_t* cartTotalLabel = nullptr;

String pendingDeleteBarcode;
char pendingNav = '\0';
CartList currentCart;
float currentTotal = 0.0f;
std::vector<String> renderedDeleteBarcodes;

lv_color_t white = lv_color_hex(0xFFFFFF);
lv_color_t ink = lv_color_hex(0x151A1F);
lv_color_t muted = lv_color_hex(0x53616F);
lv_color_t accent = lv_color_hex(0x147D64);
lv_color_t danger = lv_color_hex(0xC83232);
lv_color_t line = lv_color_hex(0xE3E8E6);

void flushDisplay(lv_display_t* display, const lv_area_t* area, uint8_t* pixels) {
  uint32_t width = area->x2 - area->x1 + 1;
  uint32_t height = area->y2 - area->y1 + 1;

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, width, height);
  tft.pushColors(reinterpret_cast<uint16_t*>(pixels), width * height, true);
  tft.endWrite();

  lv_display_flush_ready(display);
}

void touchscreen_read(lv_indev_t* indev, lv_indev_data_t* data) {
  LV_UNUSED(indev);

  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = constrain(map(p.x, TOUCH_MIN_X, TOUCH_MAX_X, 0, screenW - 1), 0, screenW - 1);
    data->point.y = constrain(map(p.y, TOUCH_MIN_Y, TOUCH_MAX_Y, 0, screenH - 1), 0, screenH - 1);
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

void loadMainScreen() {
  pendingNav = 'B';
  if (mainScreen != nullptr) {
    lv_screen_load(mainScreen);
  }
}

void viewCartEvent(lv_event_t* e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    pendingNav = 'C';
    if (cartScreen != nullptr) {
      lv_screen_load(cartScreen);
    }
  }
}

void viewRecsEvent(lv_event_t* e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    pendingNav = 'R';
  }
}

void backToMainEvent(lv_event_t* e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    loadMainScreen();
  }
}

void deleteItemEvent(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

  int index = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
  if (index >= 0 && index < static_cast<int>(renderedDeleteBarcodes.size())) {
    pendingDeleteBarcode = renderedDeleteBarcodes[index];
  }
}

lv_obj_t* makeLabel(lv_obj_t* parent, const String& text, int x, int y, int width,
                    lv_color_t color = ink) {
  lv_obj_t* obj = lv_label_create(parent);
  lv_label_set_text(obj, text.c_str());
  lv_obj_set_width(obj, width);
  lv_label_set_long_mode(obj, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_set_style_text_font(obj, LV_FONT_DEFAULT, 0);
  lv_obj_set_style_text_color(obj, color, 0);
  lv_obj_align(obj, LV_ALIGN_TOP_LEFT, x, y);
  return obj;
}

lv_obj_t* makeButton(lv_obj_t* parent, const char* text, int x, int y, int width, int height,
                     lv_color_t color, lv_event_cb_t cb, void* userData = nullptr) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_size(btn, width, height);
  lv_obj_align(btn, LV_ALIGN_TOP_LEFT, x, y);
  lv_obj_set_style_bg_color(btn, color, 0);
  lv_obj_set_style_radius(btn, 6, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, userData);

  lv_obj_t* btnLabel = lv_label_create(btn);
  lv_label_set_text(btnLabel, text);
  lv_obj_set_style_text_font(btnLabel, LV_FONT_DEFAULT, 0);
  lv_obj_center(btnLabel);
  return btn;
}

void styleScreen(lv_obj_t* screen) {
  lv_obj_set_style_bg_color(screen, white, 0);
  lv_obj_set_style_pad_all(screen, 0, 0);
}

void clearScreen(lv_obj_t*& screen) {
  if (screen != nullptr) {
    lv_obj_delete(screen);
  }
  screen = lv_obj_create(nullptr);
  styleScreen(screen);
}

void refreshCartList() {
  if (cartList == nullptr) return;

  lv_obj_clean(cartList);
  renderedDeleteBarcodes.clear();

  if (currentCart.empty()) {
    makeLabel(cartList, "No items added yet.", 8, 8, screenW - 64, muted);
  } else {
    int maxRows = min(static_cast<int>(currentCart.size()), 4);
    for (int i = 0; i < maxRows; i++) {
      CartItem& item = currentCart[i];
      renderedDeleteBarcodes.push_back(item.barcode);

      lv_obj_t* row = lv_obj_create(cartList);
      lv_obj_set_size(row, screenW - 42, 42);
      lv_obj_set_style_bg_color(row, white, 0);
      lv_obj_set_style_border_width(row, 1, 0);
      lv_obj_set_style_border_color(row, line, 0);
      lv_obj_set_style_radius(row, 4, 0);
      lv_obj_set_style_pad_all(row, 0, 0);

      makeLabel(row, String(i + 1) + ". " + item.name, 6, 4, screenW - 130, ink);
      makeLabel(row, "$" + String(item.price, 2), 6, 22, screenW - 130, muted);
      makeButton(row, "Del", screenW - 112, 6, 56, 30, danger, deleteItemEvent,
                 reinterpret_cast<void*>(static_cast<intptr_t>(i)));
    }

    if (static_cast<int>(currentCart.size()) > maxRows) {
      makeLabel(cartList, "+" + String(currentCart.size() - maxRows) + " more item(s)", 8, 188,
                screenW - 64, muted);
    }
  }

  if (cartTotalLabel != nullptr) {
    lv_label_set_text_fmt(cartTotalLabel, "Total: $%.2f", currentTotal);
  }
}

void createMainScreen() {
  clearScreen(mainScreen);

  lv_obj_t* title = makeLabel(mainScreen, "WhizCart", 14, 14, screenW - 28, ink);
  lv_obj_set_style_text_font(title, LV_FONT_DEFAULT, 0);

  statusLabel = makeLabel(mainScreen, "Scanner Ready", 14, 48, screenW - 28, accent);
  itemLabel = makeLabel(mainScreen, "Last Item: None", 14, 84, screenW - 104, ink);
  priceLabel = makeLabel(mainScreen, "Price: $0.00", 14, 126, screenW - 104, muted);
  totalLabel = makeLabel(mainScreen, "Total: $0.00", 14, 166, screenW - 104, ink);

  makeButton(mainScreen, "View Cart", screenW - 118, 82, 104, 44, accent, viewCartEvent);
  makeButton(mainScreen, "Recs", screenW - 118, 136, 104, 36, muted, viewRecsEvent);

  lv_screen_load(mainScreen);
}

void createCartScreen() {
  clearScreen(cartScreen);

  makeLabel(cartScreen, "Cart Items", 96, 15, screenW - 120, ink);
  makeButton(cartScreen, "Back", 10, 10, 76, 38, muted, backToMainEvent);

  cartList = lv_obj_create(cartScreen);
  lv_obj_set_size(cartList, screenW - 28, screenH - 112);
  lv_obj_align(cartList, LV_ALIGN_TOP_MID, 0, 62);
  lv_obj_set_style_bg_color(cartList, white, 0);
  lv_obj_set_style_border_color(cartList, line, 0);
  lv_obj_set_style_border_width(cartList, 1, 0);
  lv_obj_set_style_radius(cartList, 6, 0);
  lv_obj_set_style_pad_all(cartList, 8, 0);
  lv_obj_set_style_pad_row(cartList, 6, 0);
  lv_obj_set_flex_flow(cartList, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(cartList, LV_DIR_VER);

  cartTotalLabel = makeLabel(cartScreen, "Total: $0.00", 14, screenH - 38, screenW - 28, ink);
  refreshCartList();
}

void initDisplayAndTouch() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.begin();
  tft.setRotation(1);
  screenW = tft.width();
  screenH = tft.height();
  tft.fillScreen(TFT_WHITE);

  lv_init();

  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(1);

  lv_display_t* disp = lv_display_create(screenW, screenH);
  lv_display_set_flush_cb(disp, flushDisplay);
  lv_display_set_buffers(disp, drawBuf, nullptr, sizeof(drawBuf), LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_indev_t* indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touchscreen_read);

  createMainScreen();
  createCartScreen();
}
}

void display_init() {
  lastTickMs = millis();
  initDisplayAndTouch();
  displayReady = true;
  display_showTotal(0.0f);
}

void display_tick() {
  if (!displayReady) return;

  uint32_t now = millis();
  uint32_t elapsed = now - lastTickMs;
  if (elapsed > 0) {
    lv_tick_inc(elapsed);
    lastTickMs = now;
  }
  lv_timer_handler();
}

void display_showStatus(String msg) {
  if (statusLabel == nullptr || mainScreen == nullptr) {
    createMainScreen();
  }
  lv_label_set_text(statusLabel, msg.c_str());
  lv_screen_load(mainScreen);
  display_tick();
}

void display_showTotal(float total) {
  currentTotal = total;
  if (mainScreen == nullptr) {
    createMainScreen();
  }
  lv_label_set_text(statusLabel, "Scanner Ready");
  lv_label_set_text(itemLabel, "Last Item: None");
  lv_label_set_text(priceLabel, "Price: $0.00");
  lv_label_set_text_fmt(totalLabel, "Total: $%.2f", total);
  if (cartTotalLabel != nullptr) {
    lv_label_set_text_fmt(cartTotalLabel, "Total: $%.2f", currentTotal);
  }
  lv_screen_load(mainScreen);
  display_tick();
}

void display_showItem(String name, float price, float total) {
  currentTotal = total;
  if (mainScreen == nullptr) {
    createMainScreen();
  }
  lv_label_set_text(statusLabel, "Item Added");
  lv_label_set_text_fmt(itemLabel, "Last Item: %s", name.c_str());
  lv_label_set_text_fmt(priceLabel, "Price: $%.2f", price);
  lv_label_set_text_fmt(totalLabel, "Total: $%.2f", total);
  if (cartTotalLabel != nullptr) {
    lv_label_set_text_fmt(cartTotalLabel, "Total: $%.2f", total);
  }
  lv_screen_load(mainScreen);
  display_tick();
}

void display_showCartList(CartList& items, float total) {
  currentCart = items;
  currentTotal = total;
  if (cartScreen == nullptr) {
    createCartScreen();
  }
  refreshCartList();
  lv_screen_load(cartScreen);
  display_tick();
}

void display_showRecommendations(RecommendationList& recs) {
  lv_obj_t* recScreen = lv_obj_create(nullptr);
  styleScreen(recScreen);
  makeButton(recScreen, "Back", 10, 10, 76, 38, muted, backToMainEvent);
  makeLabel(recScreen, "Recommended", 96, 15, screenW - 120, ink);

  if (recs.empty()) {
    makeLabel(recScreen, "No recommendations yet.", 14, 70, screenW - 28, muted);
  } else {
    int y = 64;
    int maxRows = min(static_cast<int>(recs.size()), 4);
    for (int i = 0; i < maxRows; i++) {
      makeLabel(recScreen, recs[i].name, 14, y, screenW - 28, ink);
      makeLabel(recScreen, "Aisle " + recs[i].aisle + "  $" + String(recs[i].price, 2),
                14, y + 22, screenW - 28, muted);
      y += 46;
    }
  }

  lv_screen_load(recScreen);
  display_tick();
}

void display_showWeightCheck(float measured, float expected, bool ok) {
  lv_obj_t* weightScreen = lv_obj_create(nullptr);
  styleScreen(weightScreen);
  makeLabel(weightScreen, ok ? "Weight OK" : "Check Weight", 14, 15, screenW - 28, ok ? accent : danger);
  makeLabel(weightScreen, "Measured: " + String(measured, 1) + " g", 14, 78, screenW - 28, ink);
  makeLabel(weightScreen, "Expected: " + String(expected, 1) + " g", 14, 112, screenW - 28, muted);
  lv_screen_load(weightScreen);
  display_tick();
}

String display_getCartTap(CartList& items) {
  (void)items;
  display_tick();
  String barcode = pendingDeleteBarcode;
  pendingDeleteBarcode = "";
  return barcode;
}

char display_getNavTap(Mode currentMode) {
  (void)currentMode;
  display_tick();
  char nav = pendingNav;
  pendingNav = '\0';
  return nav;
}
