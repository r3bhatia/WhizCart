// firmware/src/User_Setup.h
// TFT_eSPI configuration for the CYD (ESP32-2432S028R)
// ─────────────────────────────────────────────────────────────────────────────
// IMPORTANT: This file must be copied into your TFT_eSPI library folder,
// replacing the existing User_Setup.h there. The library folder is usually at:
//   Windows: Documents/Arduino/libraries/TFT_eSPI/
//   macOS:   ~/Documents/Arduino/libraries/TFT_eSPI/
//
// Do NOT place this file only in your sketch folder — TFT_eSPI reads it
// from the library directory at compile time.
// ─────────────────────────────────────────────────────────────────────────────

// ── Driver ───────────────────────────────────────────────────────────────────
#define ILI9341_DRIVER

// ── Screen dimensions ─────────────────────────────────────────────────────────
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// ── CYD TFT SPI pins ─────────────────────────────────────────────────────────
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1   // connected to ESP32 EN pin; -1 = not controlled by library
#define TFT_BL   21   // backlight — set HIGH to turn on

// ── SPI frequency ────────────────────────────────────────────────────────────
#define SPI_FREQUENCY       20000000
#define SPI_READ_FREQUENCY  20000000

// ── Touch SPI is on a SEPARATE bus (VSPI) ────────────────────────────────────
// The XPT2046 touch controller uses different pins from the display.
// It is driven by the XPT2046_Touchscreen library, not TFT_eSPI.
// Touch pins are defined in display.cpp.
#define SPI_TOUCH_FREQUENCY  2500000

// ── Fonts ─────────────────────────────────────────────────────────────────────
#define LOAD_GLCD    // built-in font 1
#define LOAD_FONT2   // small font
#define LOAD_FONT4   // medium font  ← used for prices
#define LOAD_FONT6   // large font
#define LOAD_FONT7   // 7-segment font
#define LOAD_FONT8   // large font
#define LOAD_GFXFF   // FreeFont support
#define SMOOTH_FONT