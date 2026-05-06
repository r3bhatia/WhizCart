#include "whizcart.h"

#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

namespace {
constexpr uint8_t XPT2046_IRQ = 36;
constexpr uint8_t XPT2046_MOSI = 32;
constexpr uint8_t XPT2046_MISO = 39;
constexpr uint8_t XPT2046_CLK = 25;
constexpr uint8_t XPT2046_CS = 33;

constexpr size_t DRAW_BUF_SIZE = SCREEN_WIDTH * SCREEN_HEIGHT / 10 * (LV_COLOR_DEPTH / 8);

SPIClass touchscreenSPI(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

void viewCartEvent(lv_event_t * e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    refreshCartList();
    lv_screen_load(cartScreen);
  }
}

void backToMainEvent(lv_event_t * e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    lv_screen_load(mainScreen);
  }
}
}

lv_obj_t* itemLabel = nullptr;
lv_obj_t* priceLabel = nullptr;
lv_obj_t* totalLabel = nullptr;
lv_obj_t* statusLabel = nullptr;
lv_obj_t* mainScreen = nullptr;
lv_obj_t* cartScreen = nullptr;
lv_obj_t* cartList = nullptr;
lv_obj_t* cartTotalLabel = nullptr;

void log_print(lv_log_level_t level, const char * buf) {
  LV_UNUSED(level);
  Serial.println(buf);
  Serial.flush();
}

void touchscreen_read(lv_indev_t * indev, lv_indev_data_t * data) {
  LV_UNUSED(indev);

  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();

    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = map(p.x, 200, 3700, 1, SCREEN_WIDTH);
    data->point.y = map(p.y, 240, 3800, 1, SCREEN_HEIGHT);
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

void updateCartUI(const char* itemName, float itemPrice, float totalPrice) {
  lv_label_set_text(statusLabel, "Item Added");
  lv_label_set_text_fmt(itemLabel, "Last Item: %s", itemName);
  lv_label_set_text_fmt(priceLabel, "Price: $%.2f", itemPrice);
  lv_label_set_text_fmt(totalLabel, "Total: $%.2f", totalPrice);

  if (cartTotalLabel != nullptr) {
    lv_label_set_text_fmt(cartTotalLabel, "Total: $%.2f", totalPrice);
  }
}

void showItemNotFound() {
  lv_label_set_text(statusLabel, "Item Not Found");
  lv_label_set_text(itemLabel, "Last Item: Unknown");
  lv_label_set_text(priceLabel, "Price: $0.00");
}

void refreshCartList() {
  lv_obj_clean(cartList);

  if (cartCount == 0) {
    lv_obj_t * emptyLabel = lv_label_create(cartList);
    lv_label_set_text(emptyLabel, "No items added yet.");
  } else {
    for (int i = 0; i < cartCount; i++) {
      lv_obj_t * itemRow = lv_label_create(cartList);

      lv_label_set_text_fmt(
        itemRow,
        "%d. %s  $%.2f",
        i + 1,
        cart[i].name,
        cart[i].price
      );

      lv_obj_set_width(itemRow, 200);
      lv_label_set_long_mode(itemRow, LV_LABEL_LONG_WRAP);
    }
  }

  if (cartTotalLabel != nullptr) {
    lv_label_set_text_fmt(cartTotalLabel, "Total: $%.2f", price);
  }
}

void addToCart(const Item& item) {
  if (cartCount < MAX_CART_ITEMS) {
    cart[cartCount].name = item.name;
    cart[cartCount].price = item.price;
    cartCount++;
  } else {
    lv_label_set_text(statusLabel, "Cart Full");
  }
}

void createMainScreen() {
  mainScreen = lv_screen_active();
  lv_obj_t * screen = mainScreen;

  lv_obj_set_style_bg_color(screen, lv_color_hex(0xFFFFFF), 0);

  lv_obj_t * title = lv_label_create(screen);
  lv_label_set_text(title, "WhizCart");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

  statusLabel = lv_label_create(screen);
  lv_label_set_text(statusLabel, "Scanner Ready");
  lv_obj_align(statusLabel, LV_ALIGN_TOP_MID, 0, 50);

  itemLabel = lv_label_create(screen);
  lv_label_set_text(itemLabel, "Last Item: None");
  lv_obj_set_width(itemLabel, 220);
  lv_label_set_long_mode(itemLabel, LV_LABEL_LONG_WRAP);
  lv_obj_align(itemLabel, LV_ALIGN_TOP_LEFT, 15, 90);

  priceLabel = lv_label_create(screen);
  lv_label_set_text(priceLabel, "Price: $0.00");
  lv_obj_align(priceLabel, LV_ALIGN_TOP_LEFT, 15, 140);

  totalLabel = lv_label_create(screen);
  lv_label_set_text(totalLabel, "Total: $0.00");
  lv_obj_set_style_text_font(totalLabel, &lv_font_montserrat_24, 0);
  lv_obj_align(totalLabel, LV_ALIGN_TOP_LEFT, 15, 190);

  lv_obj_t * viewCartBtn = lv_button_create(screen);
  lv_obj_set_size(viewCartBtn, 140, 45);
  lv_obj_align(viewCartBtn, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_add_event_cb(viewCartBtn, viewCartEvent, LV_EVENT_CLICKED, NULL);

  lv_obj_t * viewCartLabel = lv_label_create(viewCartBtn);
  lv_label_set_text(viewCartLabel, "View Cart");
  lv_obj_center(viewCartLabel);
}

void createCartScreen() {
  cartScreen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(cartScreen, lv_color_hex(0xFFFFFF), 0);

  lv_obj_t * title = lv_label_create(cartScreen);
  lv_label_set_text(title, "Cart Items");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

  lv_obj_t * backBtn = lv_button_create(cartScreen);
  lv_obj_set_size(backBtn, 80, 40);
  lv_obj_align(backBtn, LV_ALIGN_TOP_LEFT, 10, 10);
  lv_obj_add_event_cb(backBtn, backToMainEvent, LV_EVENT_CLICKED, NULL);

  lv_obj_t * backLabel = lv_label_create(backBtn);
  lv_label_set_text(backLabel, "Back");
  lv_obj_center(backLabel);

  cartList = lv_obj_create(cartScreen);
  lv_obj_set_size(cartList, 220, 180);
  lv_obj_align(cartList, LV_ALIGN_TOP_MID, 0, 70);
  lv_obj_set_flex_flow(cartList, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(cartList, 8, 0);
  lv_obj_set_style_pad_row(cartList, 6, 0);
  lv_obj_set_scroll_dir(cartList, LV_DIR_VER);

  cartTotalLabel = lv_label_create(cartScreen);
  lv_label_set_text(cartTotalLabel, "Total: $0.00");
  lv_obj_set_style_text_font(cartTotalLabel, &lv_font_montserrat_24, 0);
  lv_obj_align(cartTotalLabel, LV_ALIGN_BOTTOM_MID, 0, -20);

  refreshCartList();
}

void initDisplayAndTouch() {
  lv_init();
  lv_log_register_print_cb(log_print);

  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(2);

  lv_display_t * disp = lv_tft_espi_create(SCREEN_WIDTH, SCREEN_HEIGHT, draw_buf, sizeof(draw_buf));
  lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270);

  lv_indev_t * indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touchscreen_read);

  createMainScreen();
  createCartScreen();
}
