#include "canvas_screen.h"
#include "keyboard/oled/oled_buffer.h"
#include "keyboard/oled/oled.h"
#include "keyboard/oled/oled_canvas.h"
#include "keyboard/oled/widgets/widgets.h"
#include "keyboard/oled/screens/screen_manager.h"
#include "event_scheduler.h"
#include "timer.h"
#include <zephyr/sys/printk.h>

widget_t* CanvasScreen;

void CanvasScreen_Draw(uint16_t x, uint16_t y, uint8_t* data, uint16_t len) {
    ScreenManager_ActivateScreen(ScreenId_Canvas);
    widget_t canvas = (widget_t) {
        .x = x,
        .y = y,
        .w = MIN(DISPLAY_WIDTH-x, len),
        .h = 1,
    };
    // this is neccesary to mark the buffer region as dirty.
    Framebuffer_Clear(&canvas, OledBuffer);
    for (uint16_t i = 0; i < len; i++) {
        if (x >= DISPLAY_WIDTH) {
            printk("Usb attempting to draw outside of display range!\n");
        }
        Framebuffer_SetPixel(OledBuffer, x, y, data[i]);
        x++;
    }
    Oled_RequestRedraw();
}

// Copies the macro canvas to the screen. The widget's position carries the anti-burn-in shift.
// Both buffers hold two pixels per byte in the same order, so with an even horizontal offset
// whole bytes can be copied.
void CanvasScreen_Present() {
    ScreenManager_ActivateScreen(ScreenId_Canvas);
    if (ActiveScreen != ScreenId_Canvas) {
        return; // pairing has the screen
    }
    uint16_t offsetX = MIN(CanvasScreen->x, DISPLAY_WIDTH - OLED_CANVAS_WIDTH) & ~1;
    uint16_t offsetY = MIN(CanvasScreen->y, DISPLAY_HEIGHT - OLED_CANVAS_HEIGHT);
    for (uint16_t y = 0; y < OLED_CANVAS_HEIGHT; y++) {
        const uint8_t* from = OledCanvas_Row(y);
        pixel_t* to = Framebuffer_GetPixel(OledBuffer, offsetX, offsetY + y);
        for (uint16_t i = 0; i < OLED_CANVAS_WIDTH / 2; i++) {
            to[i].value = from[i];
        }
        OledBuffer->dirtyRanges[offsetY + y].min = 0;
        OledBuffer->dirtyRanges[offsetY + y].max = DISPLAY_WIDTH - 1;
    }
    Oled_FramePresented();
}

// Keeps the last frame for a moment, so that the normal screen does not flash up when another
// animation follows. Showing a frame moves this event out again.
void CanvasScreen_Release() {
    if (ActiveScreen == ScreenId_Canvas) {
        EventScheduler_Reschedule(Timer_GetCurrentTime() + CANVAS_RELEASE_DELAY, EventSchedulerEvent_SwitchScreen, "CanvasScreen - release");
    }
}

void CanvasScreen_Init() {
    CanvasScreen = &CanvasWidget;
}
