#include "oled_canvas.h"

// Limits keep the loops short and the arithmetic inside 32 bits, whatever a macro asks for.
#define MAX_COORDINATE 4000
#define MAX_RADIUS 2000

#define ROW_BYTES (OLED_CANVAS_WIDTH / 2)

static uint8_t pixels[ROW_BYTES * OLED_CANVAS_HEIGHT] __attribute__((aligned(4)));

static int32_t max32(int32_t a, int32_t b) { return a > b ? a : b; }
static int32_t min32(int32_t a, int32_t b) { return a < b ? a : b; }
static int32_t abs32(int32_t a) { return a < 0 ? -a : a; }
static bool inRange(int32_t a) { return -MAX_COORDINATE <= a && a <= MAX_COORDINATE; }

// Clearing the canvas with memset took about 1 ms on the keyboard, so this writes whole words.
// They are volatile so that the compiler does not turn the loop back into memset.
static void setBytes(uint8_t* at, uint32_t count, uint8_t value)
{
    for (; count > 0 && (uintptr_t)at % 4 != 0; count--) {
        *at++ = value;
    }
    for (; count >= 4; count -= 4, at += 4) {
        *(volatile uint32_t*)at = value * 0x01010101u;
    }
    for (; count > 0; count--) {
        *(volatile uint8_t*)at++ = value;
    }
}

void OledCanvas_Clear()
{
    setBytes(pixels, sizeof(pixels), 0);
}

const uint8_t* OledCanvas_Row(uint16_t y)
{
    return &pixels[y * ROW_BYTES];
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

// A shape may lie partly or fully outside, so its corner is limited before the sum.
void OledCanvas_ShapePixel(int32_t x, int32_t y, int32_t column, int32_t row, uint8_t shade)
{
    if (inRange(x) && inRange(y) && inRange(row)) {
        OledCanvas_Pixel(x + column, y + row, shade);
    }
}

static void span(int32_t x1, int32_t x2, int32_t y, uint8_t shade)
{
    x1 = max32(x1, 0);
    x2 = min32(x2, OLED_CANVAS_WIDTH - 1);
    if (x1 > x2 || y < 0 || y >= OLED_CANVAS_HEIGHT) {
        return;
    }
    if (x1 % 2 == 1) {
        OledCanvas_Pixel(x1++, y, shade);
    }
    if (x2 % 2 == 0) {
        OledCanvas_Pixel(x2--, y, shade);
    }
    if (x1 < x2) {
        setBytes(&pixels[y * ROW_BYTES + x1 / 2], (x2 - x1 + 1) / 2, shade << 4 | shade);
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

// Where the edge crosses the row, rounded to the nearest pixel.
static int32_t edgeX(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t y)
{
    int32_t den = 2 * (y2 - y1);
    int32_t num = 2 * (y - y1) * (x2 - x1) + (y2 - y1);
    return x1 + (num >= 0 ? num / den : -((den - 1 - num) / den));
}

void OledCanvas_Triangle(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x3, int32_t y3, uint8_t shade)
{
    int32_t x[3] = { x1, x2, x3 }, y[3] = { y1, y2, y3 };
    for (uint8_t i = 0; i < 3; i++) {
        if (!inRange(x[i]) || !inRange(y[i])) {
            return;
        }
    }
    int32_t top = max32(0, min32(y1, min32(y2, y3)));
    int32_t bottom = min32(OLED_CANVAS_HEIGHT - 1, max32(y1, max32(y2, y3)));
    for (int32_t row = top; row <= bottom; row++) {
        int32_t left = INT32_MAX, right = INT32_MIN;
        for (uint8_t i = 0; i < 3; i++) {
            uint8_t a = i, b = (i + 1) % 3;
            if (y[a] > y[b]) {
                uint8_t swap = a;
                a = b;
                b = swap;
            }
            if (row < y[a] || row > y[b]) {
                continue;
            }
            bool flat = y[a] == y[b];
            int32_t from = flat ? x[a] : edgeX(x[a], y[a], x[b], y[b], row);
            int32_t to = flat ? x[b] : from;
            left = min32(left, min32(from, to));
            right = max32(right, max32(from, to));
        }
        span(left, right, row, shade);
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
