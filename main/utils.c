#include "utils.h"
#include "esp_system.h"
#include "esp_timer.h"

lv_color_t makeRandomColor(void)
{
    return lv_color_make((uint8_t)(randf()*255.0), (uint8_t)(randf()*255), (uint8_t)(randf()*255));
}

float randf(void)
{
    return (float)(1.0*esp_random()/UINT32_MAX);
}

uint64_t micros(void)
{
    return esp_timer_get_time();
}

uint64_t millis(void)
{
    return esp_timer_get_time() / 1000ULL;
}
