// firmware/src/display.h
#pragma once
#include <Arduino.h>
#include <vector>

// ── Shared data types ─────────────────────────────────────────────────────────
struct RecItem  { String name; float price; String aisle; };
struct CartItem { String barcode; String name; float price; int qty; };

typedef std::vector<RecItem>  RecommendationList;
typedef std::vector<CartItem> CartList;

enum Mode { MODE_TOTAL, MODE_CART, MODE_RECS };

// ── Display functions ─────────────────────────────────────────────────────────
void   display_init();
void   display_showStatus(String msg);
void   display_showTotal(float total);
void   display_showItem(String name, float price, float total);
void   display_showRecommendations(RecommendationList& recs);
void   display_showWeightCheck(float measured, float expected, bool ok);
void   display_showCartList(CartList& items, float total);
void   display_tick();

// ── Touch functions ───────────────────────────────────────────────────────────
// Returns barcode of tapped delete row, or "" if no delete tap detect
String display_getCartTap(CartList& items);

// Returns 'C' (cart), 'R' (recs), 'B' (back/total), or '\0' (no tap)
char   display_getNavTap(Mode currentMode);
