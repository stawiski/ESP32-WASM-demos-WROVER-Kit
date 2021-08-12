// STL
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
// ESP32
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// Local
#include "screen.h"
#include "wasm.h"

static void systemTask(void *pvParameter)
{
    (void) pvParameter;

    while (true)
    {
        printf("[SYSTEM] Minimum free heap size: %d bytes\n", esp_get_minimum_free_heap_size());
        printf("[SYSTEM] StackHWM: %d\n", uxTaskGetStackHighWaterMark(NULL));
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_main(void)
{
    // Print chip information
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), WiFi%s%s, ",
            CONFIG_IDF_TARGET,
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);

    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %d bytes\n", esp_get_minimum_free_heap_size());

    xTaskCreate(&systemTask, "system", 2 * 1024, NULL, 1, NULL);

    /* If you want to use a task to create the graphic, you NEED to create a Pinned task
     * Otherwise there can be problem such as memory corruption and so on.
     * NOTE: When not using Wi-Fi nor Bluetooth you can pin the guiTask to core 0 */
    xTaskCreatePinnedToCore(&GuiTask, "gui", GUI_TASK_STACK, NULL, 0, NULL, 1);

    if (WasmInit())
    {
        xTaskCreate(&WasmTask, "wasm3", 32 * 1024, NULL, 2, NULL);
    }
    else
    {
        printf("WasmInit() failed\n");
    }
}
