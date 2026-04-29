// firmware/src/display.h
#pragma once
#include <Arduino.h>
#include <vector>

struct RecItem { String name; float price; String aisle; };
typedef std::vector<RecItem> RecommendationList;

void display_init();
void display_showStatus(String msg);
void display_showTotal(float total);
void display_showItem(String name, float price, float total);
void display_showRecommendations(RecommendationList& recs);
