//Broken delete button
// firmware/src/display.cpp
// LVGL-based UI for CYD ESP32-2432S028R
//
// LVGL is used for the interface/widgets.
// TFT_eSPI is still used as the low-level display driver.
// XPT2046_Touchscreen is still used for touch input.


#include "display.h"


#include <lvgl.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>


// ── CYD XPT2046 touch pins ───────────────────────────────────────────────────
#define TOUCH_CS   33
#define TOUCH_IRQ  36
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CLK  25


// ── Screen layout ────────────────────────────────────────────────────────────
#define SCREEN_W 320
#define SCREEN_H 240


#define HEADER_H 36
#define NAV_H    42
#define MAX_CART_ROWS 4


// ── Touch calibration ────────────────────────────────────────────────────────
#define TOUCH_X_MIN   200
#define TOUCH_X_MAX  3800
#define TOUCH_Y_MIN   300
#define TOUCH_Y_MAX  3700


// ── Hardware objects ─────────────────────────────────────────────────────────
static TFT_eSPI tft = TFT_eSPI();
static SPIClass touchSPI(VSPI);
static XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);


// ── LVGL display buffer ──────────────────────────────────────────────────────
// Buffer height of 20 rows keeps memory reasonable.
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[SCREEN_W * 20];


// ── State used by LVGL button callbacks ──────────────────────────────────────
static String pendingDeleteBarcode = "";
static char pendingNavTap = '\0';


static String cartBarcodes[MAX_CART_ROWS];


static lv_obj_t* statusLabel = nullptr;


// ── Accessible colors ────────────────────────────────────────────────────────
static const lv_color_t COLOR_BG      = lv_color_hex(0xF8FAFC);
static const lv_color_t COLOR_SURFACE = lv_color_hex(0xFFFFFF);
static const lv_color_t COLOR_BORDER  = lv_color_hex(0xCBD5E1);
static const lv_color_t COLOR_TEXT    = lv_color_hex(0x111827);
static const lv_color_t COLOR_MUTED   = lv_color_hex(0x374151);
static const lv_color_t COLOR_PRIMARY = lv_color_hex(0x1D4ED8);
static const lv_color_t COLOR_DANGER  = lv_color_hex(0xB91C1C);
static const lv_color_t COLOR_WARN    = lv_color_hex(0x92400E);
static const lv_color_t COLOR_OK_BG   = lv_color_hex(0xD1FAE5);
static const lv_color_t COLOR_WARN_BG = lv_color_hex(0xFEF3C7);


// ── LVGL styles ──────────────────────────────────────────────────────────────
static lv_style_t styleScreen;
static lv_style_t styleHeader;
static lv_style_t styleHeaderText;
static lv_style_t styleCard;
static lv_style_t styleButtonPrimary;
static lv_style_t styleButtonDanger;
static lv_style_t styleSmallText;
static lv_style_t styleTitle;
static lv_style_t styleAmount;


// ── Forward declarations ─────────────────────────────────────────────────────
static void lvFlush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p);
static void lvTouchRead(lv_indev_drv_t* indev_driver, lv_indev_data_t* data);
static void lvTickTask(void* parameter);


static void setupStyles();
static void clearScreen();
static lv_obj_t* createHeader(const char* title, bool showBack);
static void createStatus(const char* msg);
static void createNavBar();
static lv_obj_t* createLabel(lv_obj_t* parent, const char* text, int x, int y);
static String fitText(String text, int maxChars);


static void navButtonEvent(lv_event_t* e);
static void deleteButtonEvent(lv_event_t* e);


// ── LVGL display flush callback ──────────────────────────────────────────────
static void lvFlush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;


  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t*)&color_p->full, w * h, true);
  tft.endWrite();


  lv_disp_flush_ready(disp);
}


// ── LVGL touch callback ──────────────────────────────────────────────────────
static void lvTouchRead(lv_indev_drv_t* indev_driver, lv_indev_data_t* data) {
  if (!ts.tirqTouched() || !ts.touched()) {
    data->state = LV_INDEV_STATE_REL;
    return;
  }


  TS_Point p = ts.getPoint();


  int sx = map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, SCREEN_W);
  int sy = map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, SCREEN_H);


  sx = constrain(sx, 0, SCREEN_W - 1);
  sy = constrain(sy, 0, SCREEN_H - 1);


  data->state = LV_INDEV_STATE_PR;
  data->point.x = sx;
  data->point.y = sy;
}


// ── LVGL tick task ───────────────────────────────────────────────────────────
static void lvTickTask(void* parameter) {
  while (true) {
    lv_tick_inc(5);
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}


// ── Helpers ──────────────────────────────────────────────────────────────────
static String fitText(String text, int maxChars) {
  if ((int)text.length() <= maxChars) return text;
  return text.substring(0, maxChars - 3) + "...";
}


static void setupStyles() {
  lv_style_init(&styleScreen);
  lv_style_set_bg_color(&styleScreen, COLOR_BG);
  lv_style_set_text_color(&styleScreen, COLOR_TEXT);


  lv_style_init(&styleHeader);
  lv_style_set_bg_color(&styleHeader, COLOR_PRIMARY);
  lv_style_set_border_width(&styleHeader, 0);
  lv_style_set_radius(&styleHeader, 0);
  lv_style_set_pad_all(&styleHeader, 0);


  lv_style_init(&styleHeaderText);
  lv_style_set_text_color(&styleHeaderText, lv_color_white());
  lv_style_set_text_font(&styleHeaderText, &lv_font_montserrat_20);


  lv_style_init(&styleCard);
  lv_style_set_bg_color(&styleCard, COLOR_SURFACE);
  lv_style_set_border_color(&styleCard, COLOR_BORDER);
  lv_style_set_border_width(&styleCard, 1);
  lv_style_set_radius(&styleCard, 10);
  lv_style_set_pad_all(&styleCard, 10);


  lv_style_init(&styleButtonPrimary);
  lv_style_set_bg_color(&styleButtonPrimary, COLOR_PRIMARY);
  lv_style_set_text_color(&styleButtonPrimary, lv_color_white());
  lv_style_set_radius(&styleButtonPrimary, 8);
  lv_style_set_pad_all(&styleButtonPrimary, 8);


  lv_style_init(&styleButtonDanger);
  lv_style_set_bg_color(&styleButtonDanger, COLOR_DANGER);
  lv_style_set_text_color(&styleButtonDanger, lv_color_white());
  lv_style_set_radius(&styleButtonDanger, 8);
  lv_style_set_pad_all(&styleButtonDanger, 8);


  lv_style_init(&styleSmallText);
  lv_style_set_text_color(&styleSmallText, COLOR_MUTED);
  lv_style_set_text_font(&styleSmallText, &lv_font_montserrat_12);


  lv_style_init(&styleTitle);
  lv_style_set_text_color(&styleTitle, COLOR_TEXT);
  lv_style_set_text_font(&styleTitle, &lv_font_montserrat_18);


  lv_style_init(&styleAmount);
  lv_style_set_text_color(&styleAmount, COLOR_TEXT);
  lv_style_set_text_font(&styleAmount, &lv_font_montserrat_32);
}


static void clearScreen() {
  lv_obj_clean(lv_scr_act());
  lv_obj_add_style(lv_scr_act(), &styleScreen, 0);
  lv_obj_set_style_bg_color(lv_scr_act(), COLOR_BG, 0);
  statusLabel = nullptr;
}


static lv_obj_t* createHeader(const char* title, bool showBack) {
  lv_obj_t* header = lv_obj_create(lv_scr_act());
  lv_obj_add_style(header, &styleHeader, 0);
  lv_obj_set_size(header, SCREEN_W, HEADER_H);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);


  lv_obj_t* titleLabel = lv_label_create(header);
  lv_obj_add_style(titleLabel, &styleHeaderText, 0);
  lv_label_set_text(titleLabel, title);
  lv_obj_align(titleLabel, LV_ALIGN_LEFT_MID, 8, 0);


  if (showBack) {
    lv_obj_t* backBtn = lv_btn_create(header);
    lv_obj_set_size(backBtn, 78, 26);
    lv_obj_align(backBtn, LV_ALIGN_RIGHT_MID, -8, 0);
    lv_obj_add_event_cb(backBtn, navButtonEvent, LV_EVENT_CLICKED, (void*)'B');


    lv_obj_t* backLabel = lv_label_create(backBtn);
    lv_label_set_text(backLabel, "Back");
    lv_obj_center(backLabel);
  }


  return header;
}


static void createStatus(const char* msg) {
  statusLabel = lv_label_create(lv_scr_act());
  lv_obj_add_style(statusLabel, &styleSmallText, 0);
  lv_label_set_text(statusLabel, msg);
  lv_obj_set_pos(statusLabel, 10, HEADER_H + 6);
}


static void createNavBar() {
  lv_obj_t* nav = lv_obj_create(lv_scr_act());
  lv_obj_set_size(nav, SCREEN_W, NAV_H);
  lv_obj_align(nav, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_color(nav, lv_color_hex(0xE2E8F0), 0);
  lv_obj_set_style_border_width(nav, 1, 0);
  lv_obj_set_style_border_color(nav, COLOR_BORDER, 0);
  lv_obj_set_style_radius(nav, 0, 0);
  lv_obj_set_style_pad_all(nav, 4, 0);


  lv_obj_t* cartBtn = lv_btn_create(nav);
  lv_obj_add_style(cartBtn, &styleButtonPrimary, 0);
  lv_obj_set_size(cartBtn, 140, 32);
  lv_obj_align(cartBtn, LV_ALIGN_LEFT_MID, 8, 0);
  lv_obj_add_event_cb(cartBtn, navButtonEvent, LV_EVENT_CLICKED, (void*)'C');


  lv_obj_t* cartLabel = lv_label_create(cartBtn);
  lv_label_set_text(cartLabel, "Cart");
  lv_obj_center(cartLabel);


  lv_obj_t* recBtn = lv_btn_create(nav);
  lv_obj_add_style(recBtn, &styleButtonPrimary, 0);
  lv_obj_set_size(recBtn, 140, 32);
  lv_obj_align(recBtn, LV_ALIGN_RIGHT_MID, -8, 0);
  lv_obj_add_event_cb(recBtn, navButtonEvent, LV_EVENT_CLICKED, (void*)'R');


  lv_obj_t* recLabel = lv_label_create(recBtn);
  lv_label_set_text(recLabel, "Ideas");
  lv_obj_center(recLabel);
}


static lv_obj_t* createLabel(lv_obj_t* parent, const char* text, int x, int y) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_pos(label, x, y);
  return label;
}


// ── Button events ────────────────────────────────────────────────────────────
static void navButtonEvent(lv_event_t* e) {
  pendingNavTap = (char)(uintptr_t)lv_event_get_user_data(e);
}


static void deleteButtonEvent(lv_event_t* e) {
  int index = (int)(uintptr_t)lv_event_get_user_data(e);

  Serial.print("Delete button pressed. Row index: ");
  Serial.println(index);

  if (index >= 0 && index < MAX_CART_ROWS) {
    pendingDeleteBarcode = cartBarcodes[index];

    Serial.print("Pending delete barcode: ");
    Serial.println(pendingDeleteBarcode);
  }
}


// ── Init ─────────────────────────────────────────────────────────────────────
void display_init() {
  Serial.begin(115200);


  // TFT setup
  tft.init();


  // Landscape.
  // If upside down, change this and ts.setRotation(1) to 3.
  tft.setRotation(0);


  // If colors look inverted, try true.
  tft.invertDisplay(false);


  tft.fillScreen(TFT_WHITE);


  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);


  Serial.print("TFT width: ");
  Serial.println(tft.width());
  Serial.print("TFT height: ");
  Serial.println(tft.height());


  // Touch setup
  touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  ts.begin(touchSPI);
  ts.setRotation(1);


  // LVGL setup
  lv_init();


  lv_disp_draw_buf_init(&draw_buf, buf1, NULL, SCREEN_W * 20);


  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = SCREEN_W;
  disp_drv.ver_res = SCREEN_H;
  disp_drv.flush_cb = lvFlush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);


  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = lvTouchRead;
  lv_indev_drv_register(&indev_drv);


  setupStyles();


  xTaskCreatePinnedToCore(
    lvTickTask,
    "lv_tick",
    2048,
    NULL,
    1,
    NULL,
    1
  );


  clearScreen();
  createHeader("WhizCart", false);
  createStatus("Ready. Scan an item.");
  createNavBar();


  lv_timer_handler();
}


// ── Status line ──────────────────────────────────────────────────────────────
void display_showStatus(String msg) {
  if (statusLabel) {
    lv_label_set_text(statusLabel, fitText(msg, 48).c_str());
  }


  lv_timer_handler();
}


// ── Running total screen ─────────────────────────────────────────────────────
void display_showTotal(float total) {
  clearScreen();


  createHeader("WhizCart", false);
  createStatus("Ready. Scan an item.");


  lv_obj_t* title = lv_label_create(lv_scr_act());
  lv_obj_add_style(title, &styleTitle, 0);
  lv_label_set_text(title, "Current Total");
  lv_obj_set_pos(title, 14, 66);


  lv_obj_t* amount = lv_label_create(lv_scr_act());
  lv_obj_add_style(amount, &styleAmount, 0);
  String totalText = "$" + String(total, 2);
  lv_label_set_text(amount, totalText.c_str());
  lv_obj_set_pos(amount, 14, 96);


  lv_obj_t* card = lv_obj_create(lv_scr_act());
  lv_obj_add_style(card, &styleCard, 0);
  lv_obj_set_size(card, 292, 44);
  lv_obj_set_pos(card, 14, 154);


  lv_obj_t* scanLabel = lv_label_create(card);
  lv_obj_add_style(scanLabel, &styleTitle, 0);
  lv_label_set_text(scanLabel, "Scan item to begin");
  lv_obj_center(scanLabel);


  createNavBar();


  lv_timer_handler();
}


// ── Just-scanned item screen ─────────────────────────────────────────────────
void display_showItem(String name, float price, float total) {
  clearScreen();


  createHeader("WhizCart", false);


  lv_obj_t* addedCard = lv_obj_create(lv_scr_act());
  lv_obj_add_style(addedCard, &styleCard, 0);
  lv_obj_set_style_bg_color(addedCard, COLOR_OK_BG, 0);
  lv_obj_set_size(addedCard, 300, 50);
  lv_obj_set_pos(addedCard, 10, 48);


  lv_obj_t* addedLabel = lv_label_create(addedCard);
  lv_obj_add_style(addedLabel, &styleTitle, 0);
  lv_label_set_text(addedLabel, "Item Added");
  lv_obj_center(addedLabel);


  lv_obj_t* itemName = lv_label_create(lv_scr_act());
  lv_obj_add_style(itemName, &styleTitle, 0);
  lv_label_set_text(itemName, fitText(name, 24).c_str());
  lv_obj_set_pos(itemName, 12, 112);


  lv_obj_t* priceLabel = lv_label_create(lv_scr_act());
  lv_obj_add_style(priceLabel, &styleTitle, 0);
  String priceText = "Price: $" + String(price, 2);
  lv_label_set_text(priceLabel, priceText.c_str());
  lv_obj_set_pos(priceLabel, 12, 142);


  lv_obj_t* totalLabel = lv_label_create(lv_scr_act());
  lv_obj_add_style(totalLabel, &styleAmount, 0);
  String totalText = "Total: $" + String(total, 2);
  lv_label_set_text(totalLabel, totalText.c_str());
  lv_obj_set_pos(totalLabel, 12, 176);


  lv_timer_handler();
}


// ── Cart list with tappable delete buttons ───────────────────────────────────
void display_showCartList(CartList& items, float total) {
  clearScreen();


  createHeader("Cart", true);


  if (items.empty()) {
    lv_obj_t* empty = lv_label_create(lv_scr_act());
    lv_obj_add_style(empty, &styleTitle, 0);
    lv_label_set_text(empty, "Cart is empty");
    lv_obj_align(empty, LV_ALIGN_CENTER, 0, -10);


    lv_obj_t* helper = lv_label_create(lv_scr_act());
    lv_obj_add_style(helper, &styleSmallText, 0);
    lv_label_set_text(helper, "Scan an item to begin.");
    lv_obj_align(helper, LV_ALIGN_CENTER, 0, 20);


    lv_timer_handler();
    return;
  }


  int rows = min((int)items.size(), MAX_CART_ROWS);


  for (int i = 0; i < rows; i++) {
    cartBarcodes[i] = items[i].barcode;


    int y = HEADER_H + 8 + i * 54;


    lv_obj_t* row = lv_obj_create(lv_scr_act());
    lv_obj_add_style(row, &styleCard, 0);
    lv_obj_set_size(row, 304, 50);
    lv_obj_set_pos(row, 8, y);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* nameLabel = lv_label_create(row);
    lv_label_set_text(nameLabel, fitText(items[i].name, 24).c_str());
    lv_obj_set_style_text_color(nameLabel, COLOR_TEXT, 0);
    lv_obj_set_width(nameLabel, 190);
    lv_label_set_long_mode(nameLabel, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(nameLabel, 6, 3);


    lv_obj_t* priceLabel = lv_label_create(row);
    String priceText;


    if (items[i].qty > 1) {
      priceText = "$" + String(items[i].price, 2) + "  Qty " + String(items[i].qty);
    } else {
      priceText = "$" + String(items[i].price, 2);
    }


    lv_label_set_text(priceLabel, priceText.c_str());
    lv_obj_set_style_text_color(priceLabel, COLOR_MUTED, 0);
    lv_obj_set_pos(priceLabel, 6, 24);


    lv_obj_t* delBtn = lv_btn_create(row);
    lv_obj_add_style(delBtn, &styleButtonDanger, 0);
    lv_obj_set_size(delBtn, 82, 34);
    lv_obj_align(delBtn, LV_ALIGN_RIGHT_MID, -2, 0);

    lv_obj_clear_flag(delBtn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(delBtn, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_add_event_cb(delBtn, deleteButtonEvent, LV_EVENT_RELEASED, (void*)(uintptr_t)i);

    lv_obj_t* delLabel = lv_label_create(delBtn);
    lv_label_set_text(delLabel, "Delete");
    lv_obj_center(delLabel);
  }


  lv_obj_t* footer = lv_obj_create(lv_scr_act());
  lv_obj_set_size(footer, SCREEN_W, 28);
  lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_color(footer, lv_color_hex(0xE2E8F0), 0);
  lv_obj_set_style_border_width(footer, 0, 0);
  lv_obj_set_style_radius(footer, 0, 0);


  lv_obj_t* totalLabel = lv_label_create(footer);
  String footerText;


  if ((int)items.size() > MAX_CART_ROWS) {
    footerText = "+" + String((int)items.size() - MAX_CART_ROWS) +
                " more   Total: $" + String(total, 2);
  } else {
    footerText = "Total: $" + String(total, 2);
  }


  lv_label_set_text(totalLabel, footerText.c_str());


  lv_obj_set_style_text_color(totalLabel, COLOR_TEXT, 0);
  lv_obj_align(totalLabel, LV_ALIGN_LEFT_MID, 10, 0);


  lv_timer_handler();
}


// ── Cart delete event polling ────────────────────────────────────────────────
String display_getCartTap(CartList& items) {
  lv_timer_handler();


  if (pendingDeleteBarcode.length() == 0) {
    return "";
  }


  String barcode = pendingDeleteBarcode;
  pendingDeleteBarcode = "";


  return barcode;
}


// ── Recommendations ──────────────────────────────────────────────────────────
void display_showRecommendations(RecommendationList& recs) {
  clearScreen();


  createHeader("Ideas", true);


  lv_obj_t* title = lv_label_create(lv_scr_act());
  lv_obj_add_style(title, &styleTitle, 0);
  lv_label_set_text(title, "Suggested Items");
  lv_obj_set_pos(title, 12, 48);


  if (recs.empty()) {
    lv_obj_t* empty = lv_label_create(lv_scr_act());
    lv_obj_add_style(empty, &styleSmallText, 0);
    lv_label_set_text(empty, "Add items to see suggestions.");
    lv_obj_set_pos(empty, 12, 82);


    lv_timer_handler();
    return;
  }


  for (int i = 0; i < (int)recs.size() && i < 4; i++) {
    int y = 78 + i * 38;


    lv_obj_t* row = lv_obj_create(lv_scr_act());
    lv_obj_add_style(row, &styleCard, 0);
    lv_obj_set_size(row, 304, 34);
    lv_obj_set_pos(row, 8, y);


    lv_obj_t* nameLabel = lv_label_create(row);
    lv_label_set_text(nameLabel, fitText(recs[i].name, 22).c_str());
    lv_obj_set_style_text_color(nameLabel, COLOR_TEXT, 0);
    lv_obj_set_pos(nameLabel, 6, 2);


    lv_obj_t* priceLabel = lv_label_create(row);
    String recPrice = "$" + String(recs[i].price, 2);
    lv_label_set_text(priceLabel, recPrice.c_str());
    lv_obj_set_style_text_color(priceLabel, COLOR_TEXT, 0);
    lv_obj_align(priceLabel, LV_ALIGN_TOP_RIGHT, -6, 2);


    lv_obj_t* aisleLabel = lv_label_create(row);
    lv_label_set_text_fmt(aisleLabel, "Aisle %s", recs[i].aisle.c_str());
    lv_obj_set_style_text_color(aisleLabel, COLOR_MUTED, 0);
    lv_obj_set_pos(aisleLabel, 6, 17);
  }


  lv_timer_handler();
}


// ── Weight check ─────────────────────────────────────────────────────────────
void display_showWeightCheck(float measured, float expected, bool ok) {
  clearScreen();


  createHeader("Weight", false);


  lv_obj_t* card = lv_obj_create(lv_scr_act());
  lv_obj_add_style(card, &styleCard, 0);
  lv_obj_set_size(card, 300, 62);
  lv_obj_set_pos(card, 10, 48);
  lv_obj_set_style_bg_color(card, ok ? COLOR_OK_BG : COLOR_WARN_BG, 0);


  lv_obj_t* status = lv_label_create(card);
  lv_obj_add_style(status, &styleTitle, 0);
  lv_label_set_text(status, ok ? "OK: Weight Match" : "Check Cart");
  lv_obj_set_pos(status, 8, 6);


  lv_obj_t* helper = lv_label_create(card);
  lv_obj_add_style(helper, &styleSmallText, 0);
  lv_label_set_text(
    helper,
    ok ? "Item weight looks correct." : "Weight does not match scan."
  );
  lv_obj_set_pos(helper, 8, 34);


  lv_obj_t* measuredLabel = lv_label_create(lv_scr_act());
  lv_obj_add_style(measuredLabel, &styleTitle, 0);
  lv_label_set_text_fmt(measuredLabel, "Measured: %.1fg", measured);
  lv_obj_set_pos(measuredLabel, 12, 128);


  lv_obj_t* expectedLabel = lv_label_create(lv_scr_act());
  lv_obj_add_style(expectedLabel, &styleTitle, 0);
  lv_label_set_text_fmt(expectedLabel, "Expected: %.1fg", expected);
  lv_obj_set_pos(expectedLabel, 12, 158);


  float diff = measured - expected;


  lv_obj_t* diffLabel = lv_label_create(lv_scr_act());
  lv_obj_add_style(diffLabel, &styleTitle, 0);


  if (diff >= 0) {
    lv_label_set_text_fmt(diffLabel, "Diff: +%.1fg", diff);
  } else {
    lv_label_set_text_fmt(diffLabel, "Diff: %.1fg", diff);
  }


  lv_obj_set_pos(diffLabel, 12, 188);


  lv_timer_handler();
}


// ── Navigation event polling ─────────────────────────────────────────────────
char display_getNavTap(Mode currentMode) {
  lv_timer_handler();


  if (pendingNavTap == '\0') {
    return '\0';
  }


  char tap = pendingNavTap;
  pendingNavTap = '\0';


  if (tap == 'B' && currentMode == MODE_TOTAL) {
    return '\0';
  }


  return tap;
}

