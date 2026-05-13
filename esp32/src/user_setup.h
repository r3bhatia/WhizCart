// firmware/src/User_Setup.h
// TFT_eSPI configuration for the CYD (ESP32-2432S028R)
//
// IMPORTANT:
// This file must be copied into:
//   Windows: Documents/Arduino/libraries/TFT_eSPI/User_Setup.h
//
// Do NOT place this only in your sketch/project folder.

#define USER_SETUP_INFO "CYD ESP32-2432S028R ILI9341"

// ── Driver ───────────────────────────────────────────────────────────────────
#define ILI9341_DRIVER

// ── Screen dimensions ────────────────────────────────────────────────────────
// Native portrait size. Landscape is handled in display.cpp using setRotation(1).
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// ── CYD TFT SPI pins ─────────────────────────────────────────────────────────
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1

// Backlight pin
#define TFT_BL   21
#define TFT_BACKLIGHT_ON HIGH

// ── SPI frequency ────────────────────────────────────────────────────────────
// 55 MHz can cause static/blur/corruption on many CYD boards.
// Start safe at 27 MHz. If stable, you can try 40 MHz later.
#define SPI_FREQUENCY       27000000
#define SPI_READ_FREQUENCY  16000000

// Touch is handled separately by XPT2046_Touchscreen in display.cpp.
// This line does not hurt, but TFT_eSPI is not controlling touch in your code.
#define SPI_TOUCH_FREQUENCY 2500000

// ── Fonts ────────────────────────────────────────────────────────────────────
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT