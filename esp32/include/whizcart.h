#pragma once

#include <Arduino.h>
#include "api_client.h"
#include "display.h"

void display_init();
void display_tick();

void display_showStatus(String msg);
void display_showTotal(float total);
void display_showItem(String name, float price, float total);
void display_showCartList(CartList& items, float total);
void display_showRecommendations(RecommendationList& recs);
void display_showWeightCheck(float measured, float expected, bool ok);

String display_getCartTap(CartList& items);
char display_getNavTap(Mode mode);
