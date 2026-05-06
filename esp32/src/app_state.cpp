#include "whizcart.h"

float price = 0.0f;
CartEntry cart[MAX_CART_ITEMS];
int cartCount = 0;

const Item items[] = {
  {"Coca Cola", "5449000000996", 2.99},
  {"Oreo", "8992760221028", 1.99},
  {"Sprite", "0000544926910", 2.99}
};

const size_t itemCount = sizeof(items) / sizeof(items[0]);
