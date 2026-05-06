#pragma once

#include <Arduino.h>
#include <lvgl.h>

constexpr int SCREEN_WIDTH = 240;
constexpr int SCREEN_HEIGHT = 320;
constexpr int MAX_CART_ITEMS = 30;

struct CartEntry {
  const char* name;
  float price;
};

struct Item {
  const char* name;
  const char* barcode;
  float price;
};

extern float price;
extern CartEntry cart[MAX_CART_ITEMS];
extern int cartCount;
extern const Item items[];
extern const size_t itemCount;

extern lv_obj_t* itemLabel;
extern lv_obj_t* priceLabel;
extern lv_obj_t* totalLabel;
extern lv_obj_t* statusLabel;
extern lv_obj_t* mainScreen;
extern lv_obj_t* cartScreen;
extern lv_obj_t* cartList;
extern lv_obj_t* cartTotalLabel;

void log_print(lv_log_level_t level, const char * buf);
void touchscreen_read(lv_indev_t * indev, lv_indev_data_t * data);
void initDisplayAndTouch();
void createMainScreen();
void createCartScreen();
void updateCartUI(const char* itemName, float itemPrice, float totalPrice);
void showItemNotFound();
void refreshCartList();
void addToCart(const Item& item);
void initScanner();
void pollScanner();
