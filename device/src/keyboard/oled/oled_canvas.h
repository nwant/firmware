#ifndef __OLED_CANVAS_H__
#define __OLED_CANVAS_H__

// PROTOTYPE: off-screen drawing surface for macros. 16 shades, drawing is clipped to the surface.

// Includes:

    #include <inttypes.h>
    #include <stdbool.h>
    #include "lvgl/lvgl.h"

// Macros:

    #define OLED_CANVAS_WIDTH 252
    #define OLED_CANVAS_HEIGHT 60

// Functions:

    void OledCanvas_Clear();
    const uint8_t* OledCanvas_Row(uint16_t y);
    uint8_t OledCanvas_Get(uint16_t x, uint16_t y);
    void OledCanvas_Pixel(int32_t x, int32_t y, uint8_t shade);
    void OledCanvas_Line(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint8_t shade);
    void OledCanvas_Rect(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t shade);
    void OledCanvas_Fill(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t shade);
    void OledCanvas_Circle(int32_t cx, int32_t cy, int32_t r, uint8_t shade);
    void OledCanvas_Disc(int32_t cx, int32_t cy, int32_t r, uint8_t shade);
    void OledCanvas_Triangle(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x3, int32_t y3, uint8_t shade);
    int32_t OledCanvas_TextWidth(const lv_font_t* font, const char* text);
    void OledCanvas_Text(int32_t x, int32_t y, const lv_font_t* font, uint8_t shade, const char* text);

#endif
