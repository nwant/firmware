#include "oled_canvas.h"
#include <string.h>

// Limits keep the loops short and the arithmetic inside 32 bits, whatever a macro asks for.
#define MAX_COORDINATE 4000
#define MAX_RADIUS 2000

static uint8_t pixels[OLED_CANVAS_WIDTH * OLED_CANVAS_HEIGHT / 2];

static int32_t max32(int32_t a, int32_t b) { return a > b ? a : b; }
static int32_t min32(int32_t a, int32_t b) { return a < b ? a : b; }
static int32_t abs32(int32_t a) { return a < 0 ? -a : a; }
static bool inRange(int32_t a) { return -MAX_COORDINATE <= a && a <= MAX_COORDINATE; }

void OledCanvas_Clear()
{
    memset(pixels, 0, sizeof(pixels));
}

uint8_t OledCanvas_Get(uint16_t x, uint16_t y)
{
    uint8_t pair = pixels[(y * OLED_CANVAS_WIDTH + x) / 2];
    return x % 2 ? pair >> 4 : pair & 0x0f;
}

void OledCanvas_Pixel(int32_t x, int32_t y, uint8_t shade)
{
    if (x < 0 || y < 0 || x >= OLED_CANVAS_WIDTH || y >= OLED_CANVAS_HEIGHT) {
        return;
    }
    uint8_t* pair = &pixels[(y * OLED_CANVAS_WIDTH + x) / 2];
    *pair = x % 2 ? (*pair & 0x0f) | (shade << 4) : (*pair & 0xf0) | shade;
}

static void span(int32_t x1, int32_t x2, int32_t y, uint8_t shade)
{
    x1 = max32(x1, 0);
    x2 = min32(x2, OLED_CANVAS_WIDTH - 1);
    for (int32_t x = x1; x <= x2; x++) {
        OledCanvas_Pixel(x, y, shade);
    }
}

void OledCanvas_Line(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint8_t shade)
{
    if (!inRange(x1) || !inRange(y1) || !inRange(x2) || !inRange(y2)) {
        return;
    }
    int32_t dx = abs32(x2 - x1), dy = -abs32(y2 - y1);
    int32_t sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1;
    int32_t error = dx + dy;
    while (true) {
        OledCanvas_Pixel(x1, y1, shade);
        if (x1 == x2 && y1 == y2) {
            return;
        }
        int32_t doubled = 2 * error;
        if (doubled >= dy) {
            error += dy;
            x1 += sx;
        }
        if (doubled <= dx) {
            error += dx;
            y1 += sy;
        }
    }
}

void OledCanvas_Fill(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t shade)
{
    if (!inRange(x) || !inRange(y) || !inRange(w) || !inRange(h)) {
        return;
    }
    for (int32_t row = max32(0, y); row < min32(OLED_CANVAS_HEIGHT, y + h); row++) {
        span(x, x + w - 1, row, shade);
    }
}

void OledCanvas_Rect(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t shade)
{
    if (w < 1 || h < 1 || !inRange(x) || !inRange(y) || !inRange(w) || !inRange(h)) {
        return;
    }
    OledCanvas_Fill(x, y, w, 1, shade);
    OledCanvas_Fill(x, y + h - 1, w, 1, shade);
    OledCanvas_Fill(x, y, 1, h, shade);
    OledCanvas_Fill(x + w - 1, y, 1, h, shade);
}

void OledCanvas_Circle(int32_t cx, int32_t cy, int32_t r, uint8_t shade)
{
    if (r < 0 || r > MAX_RADIUS || !inRange(cx) || !inRange(cy)) {
        return;
    }
    int32_t x = r, y = 0, error = 1 - r;
    while (x >= y) {
        OledCanvas_Pixel(cx + x, cy + y, shade);
        OledCanvas_Pixel(cx + y, cy + x, shade);
        OledCanvas_Pixel(cx - x, cy + y, shade);
        OledCanvas_Pixel(cx - y, cy + x, shade);
        OledCanvas_Pixel(cx + x, cy - y, shade);
        OledCanvas_Pixel(cx + y, cy - x, shade);
        OledCanvas_Pixel(cx - x, cy - y, shade);
        OledCanvas_Pixel(cx - y, cy - x, shade);
        if (error < 0) {
            error += 2 * y + 3;
        } else {
            x--;
            error += 2 * (y - x) + 3;
        }
        y++;
    }
}

static int32_t squareRoot(int32_t value)
{
    int32_t root = 0;
    for (int32_t bit = 1 << 15; bit > 0; bit >>= 1) {
        if ((root + bit) <= value / (root + bit)) {
            root += bit;
        }
    }
    return root;
}

void OledCanvas_Disc(int32_t cx, int32_t cy, int32_t r, uint8_t shade)
{
    if (r < 0 || r > MAX_RADIUS || !inRange(cx) || !inRange(cy)) {
        return;
    }
    for (int32_t y = max32(-r, -cy); y <= min32(r, OLED_CANVAS_HEIGHT - 1 - cy); y++) {
        int32_t half = squareRoot(r * r + r - y * y);
        span(cx - half, cx + half, cy + y, shade);
    }
}

static const lv_font_fmt_txt_glyph_dsc_t* glyphOf(const lv_font_t* font, char c)
{
    uint8_t code = (uint8_t)c;
    if (code < 32 || code > 126) {
        code = '*';
    }
    return &font->dsc->glyph_dsc[code - 31];
}

int32_t OledCanvas_TextWidth(const lv_font_t* font, const char* text)
{
    int32_t width = 0;
    for (; *text != '\0'; text++) {
        width += glyphOf(font, *text)->adv_w / 16;
    }
    return width;
}

void OledCanvas_Text(int32_t x, int32_t y, const lv_font_t* font, uint8_t shade, const char* text)
{
    if (!inRange(x) || !inRange(y)) {
        return;
    }
    for (; *text != '\0'; text++) {
        const lv_font_fmt_txt_glyph_dsc_t* glyph = glyphOf(font, *text);
        int32_t top = font->line_height - font->base_line - glyph->box_h - glyph->ofs_y;
        for (int32_t row = 0; row < glyph->box_h; row++) {
            for (int32_t column = 0; column < glyph->box_w; column++) {
                uint32_t index = row * glyph->box_w + column;
                uint8_t pair = font->dsc->glyph_bitmap[glyph->bitmap_index + index / 2];
                uint8_t value = index % 2 ? pair & 0x0f : pair >> 4;
                uint8_t shaded = (value * shade * 2 + 15) / 30;
                if (value != 0 && (shade == 0 || shaded != 0)) {
                    OledCanvas_Pixel(x + glyph->ofs_x + column, y + top + row, shaded);
                }
            }
        }
        x += glyph->adv_w / 16;
    }
}
