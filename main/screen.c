/* Standard Includes */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* FreeRTOS and ESP Includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_freertos_hooks.h"
#include "freertos/semphr.h"
#include "esp_system.h"

#include "screen.h"

/* Littlevgl specific */
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#include "lvgl_helpers.h"
#include "utils.h"

#define LV_TICK_PERIOD_MS 1
#define CANVAS_WIDTH 240
#define CANVAS_HEIGHT 75

// Forward declarations

static void lv_tick_task(void *arg);

// Private variables

/* Creates a semaphore to handle concurrent call to lvgl stuff
 * If you wish to call *any* lvgl function from other threads/tasks
 * you should lock on the very same semaphore! */
SemaphoreHandle_t xGuiSemaphore;
static EXT_RAM_ATTR lv_color_t ssCanvasBuffer[CANVAS_WIDTH * CANVAS_HEIGHT * sizeof(lv_color_t)];
static lv_obj_t *ssCanvasPtr;

// Implementation

// Private functions

static void create_demo_app(void)
{
    ssCanvasPtr = lv_canvas_create(lv_scr_act(), NULL);
    lv_canvas_set_buffer(ssCanvasPtr, ssCanvasBuffer, CANVAS_WIDTH, CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);
    lv_canvas_fill_bg(ssCanvasPtr, LV_COLOR_WHITE, LV_OPA_COVER);
    lv_obj_align(ssCanvasPtr, NULL, LV_ALIGN_CENTER, 0, 0);
}

static void lv_tick_task(void *arg)
{
    (void) arg;
    lv_tick_inc(LV_TICK_PERIOD_MS);
}

// Public functions

void GuiTask(void *pvParameter)
{
    (void) pvParameter;
    xGuiSemaphore = xSemaphoreCreateMutex();

    xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);

    lv_init();

    /* Initialize SPI or I2C bus used by the drivers */
    lvgl_driver_init();

    lv_color_t* buf1 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1 != NULL);

    /* Use double buffered when not working with monochrome displays */
#ifndef CONFIG_LV_TFT_DISPLAY_MONOCHROME
    lv_color_t* buf2 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf2 != NULL);
#else
    static lv_color_t *buf2 = NULL;
#endif

    static lv_disp_buf_t disp_buf;
    uint32_t size_in_px = DISP_BUF_SIZE;

#if defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_IL3820         \
    || defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_JD79653A    \
    || defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_UC8151D     \
    || defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_SSD1306

    /* Actual size in pixels, not bytes. */
    size_in_px *= 8;
#endif

    /* Initialize the working buffer depending on the selected display.
     * NOTE: buf2 == NULL when using monochrome displays. */
    lv_disp_buf_init(&disp_buf, buf1, buf2, size_in_px);

    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = disp_driver_flush;

    /* When using a monochrome display we need to register the callbacks:
     * - rounder_cb
     * - set_px_cb */
#ifdef CONFIG_LV_TFT_DISPLAY_MONOCHROME
    disp_drv.rounder_cb = disp_driver_rounder;
    disp_drv.set_px_cb = disp_driver_set_px;
#endif

    disp_drv.buffer = &disp_buf;
    lv_disp_drv_register(&disp_drv);

    /* Register an input device when enabled on the menuconfig */
#if CONFIG_LV_TOUCH_CONTROLLER != TOUCH_CONTROLLER_NONE
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.read_cb = touch_driver_read;
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    lv_indev_drv_register(&indev_drv);
#endif

    /* Create and start a periodic timer interrupt to call lv_tick_inc */
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_task,
        .name = "periodic_gui"
    };

    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, LV_TICK_PERIOD_MS * 1000));

    // Create demo
    create_demo_app();

    // Give the semaphore as the GUI task is ready
    xSemaphoreGive(xGuiSemaphore);

    uint32_t guiTaskLoop = 0;
    uint32_t lastLogPrintTimeMs = 0;

    while (1)
    {
        if (millis() - lastLogPrintTimeMs >= 1000)
        {
            printf("[GUI] Loop: %d\n", guiTaskLoop);
            printf("[GUI] StackHWM: %d\n", uxTaskGetStackHighWaterMark(NULL));
            lastLogPrintTimeMs = millis();
        }

        // Try to take the semaphore, call lvgl related function on success
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY))
        {
            lv_task_handler();
            xSemaphoreGive(xGuiSemaphore);
        }

        guiTaskLoop++;
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    printf("[GUI] Task ended\n");

    /* A task should NEVER return */
    free(buf1);
#ifndef CONFIG_LV_TFT_DISPLAY_MONOCHROME
    free(buf2);
#endif
    vTaskDelete(NULL);
}

void GuiWriteDinoRgb565Buffer(uint16_t width, uint16_t height, const uint16_t* pBuffer)
{
    for (uint16_t y = 0; y < height; y++)
    {
        for (uint16_t x = 0; x < width; x++)
        {
            uint16_t rgb565 = *pBuffer++;
            // Set pixel in canvas buffer directly
            ssCanvasBuffer[CANVAS_WIDTH*y + x] = (lv_color16_t)rgb565;
            // Calling set pixel function is much slower
            // lv_canvas_set_px(ssCanvasPtr, x, y, (lv_color_t)rgb565);
        }
    }

    if (xGuiSemaphore && xSemaphoreTake(xGuiSemaphore, portMAX_DELAY) == pdTRUE)
    {
        lv_obj_clean(ssCanvasPtr);
        lv_obj_invalidate(ssCanvasPtr);
        xSemaphoreGive(xGuiSemaphore);
    }
}
