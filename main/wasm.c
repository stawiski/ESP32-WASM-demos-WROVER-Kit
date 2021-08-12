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
// Components
#include <wasm3.h>
// Local
#include "utils.h"
#include "screen.h"
#include "dino.wasm.h"

#define FATAL(...)                                      \
    {                                                   \
        printf("[WASM] Fatal error: %s\r\n", __func__); \
        printf(__VA_ARGS__);                            \
        return false;                                   \
    }

// The Math.random() function returns a floating-point,
// pseudo-random number in the range 0 to less than 1
m3ApiRawFunction(Math_random)
{
    m3ApiReturnType(float);
    float r = randf();
    m3ApiReturn(r);
}

// Memcpy is generic, and much faster in native code
m3ApiRawFunction(Dino_memcpy)
{
    m3ApiGetArgMem(uint8_t *, dst);
    m3ApiGetArgMem(uint8_t *, src);
    m3ApiGetArgMem(uint8_t *, dstEnd);

    do
    {
        *dst++ = *src++;
    }
    while (dst < dstEnd);

    m3ApiSuccess();
}

IM3Environment env;
IM3Runtime runtime;
IM3Module module;
IM3Function func_run;
uint8_t *mem;

bool loadWasm(void)
{
    M3Result result = m3Err_none;

    if (!env)
    {
        env = m3_NewEnvironment();
        if (!env)
        {
            FATAL("NewEnvironment failed");
        }
    }

    m3_FreeRuntime(runtime);

    runtime = m3_NewRuntime(env, 1024, NULL);
    if (!runtime)
    {
        FATAL("NewRuntime failed");
    }

    result = m3_ParseModule(env, &module, dino_wasm, dino_wasm_len);
    if (result)
    {
        FATAL("ParseModule: %s", result);
    }

    result = m3_LoadModule(runtime, module);
    if (result)
    {
        FATAL("LoadModule: %s", result);
    }

    result = m3_LinkRawFunction(module, "Math", "random", "f()", &Math_random);
    if (result)
    {
        FATAL("LinkRawFunction (Math.random): %s", result);
    }

    result = m3_LinkRawFunction(module, "Dino", "memcpy", "v(iii)", &Dino_memcpy);
    if (result)
    {
        FATAL("LinkRawFunction (Dino.memcpy): %s", result);
    }

    mem = m3_GetMemory(runtime, NULL, 0);
    if (!mem)
    {
        FATAL("GetMemory failed");
    }

    result = m3_FindFunction(&func_run, runtime, "run");
    if (result)
    {
        FATAL("FindFunction: %s", result);
    }

    return true;
}

bool WasmInit(void)
{
    uint64_t timeStartUs, timeEndUs;
    bool result = false;

    printf("[WASM] Wasm3 v %s, (%s), build %s %s\n", M3_VERSION, M3_ARCH, __DATE__, __TIME__);

    timeStartUs = micros();
    result = loadWasm();
    timeEndUs = micros();

    if (result)
    {
        printf("[WASM] loadWasm() took %lld us\n", timeEndUs - timeStartUs);
    }

    return result;
}

typedef enum
{
    BUTTON_STATE_NONE,
    BUTTON_STATE_DOWN,
    BUTTON_STATE_UP,

} BUTTON_STATE_enum_t;

static const char* buttonStateToStr(BUTTON_STATE_enum_t ssButtonState)
{
    const char* result = "unknown";
    switch (ssButtonState)
    {
        default:
        case BUTTON_STATE_NONE:
        {
            result = "none";
            break;
        }
        case BUTTON_STATE_DOWN:
        {
            result = "down";
            break;
        }
        case BUTTON_STATE_UP:
        {
            result = "up";
            break;
        }
    }
    return result;
}

static BUTTON_STATE_enum_t ssButtonState = BUTTON_STATE_NONE;
static uint32_t ssButtonHoldTimeMs, ssButtonTimestampMs;

static void handleRandomButtonInput(uint32_t* mem)
{
    if ((millis() - ssButtonTimestampMs) >= ssButtonHoldTimeMs)
    {
        // Randomize new button state
        float r = randf();
        if (r < 0.7)
        {
            ssButtonState = BUTTON_STATE_NONE;
        }
        else if (r < 0.9)
        {
            ssButtonState = BUTTON_STATE_UP;
        }
        else
        {
            ssButtonState = BUTTON_STATE_DOWN;
        }
        ssButtonHoldTimeMs = randf()*1000 + 250;
        ssButtonTimestampMs = millis();
        printf("[WASM] New random button state: %s time: %d ms\n", buttonStateToStr(ssButtonState), ssButtonHoldTimeMs);
    }

    switch (ssButtonState)
    {
        default:
        case BUTTON_STATE_NONE:
        {
            *mem = 0;
            break;
        }
        case BUTTON_STATE_DOWN:
        {
            *mem = 0x2;
            break;
        }
        case BUTTON_STATE_UP:
        {
            *mem = 0x1;
            break;
        }
    }
}

static uint64_t ssFrameRenderTimeSum;
static uint32_t ssFrameRenderCount;
static uint64_t ssLastLogPrintTimeMs;
static uint64_t ssLastFrameTimestampUs;

void WasmTask(void *pvParameter)
{
    (void)pvParameter;
    M3Result result;

    while (true)
    {
        // Button input to Dino game
        handleRandomButtonInput((uint32_t*)(mem + 0x0000));

        uint64_t frameRenderTimestampStartUs = micros();
        result = m3_CallV(func_run);
        uint64_t frameRenderTimestampEndUs = micros();

        if (result)
        {
            break;
        }

        // Calculate frame render times
        ssFrameRenderTimeSum += frameRenderTimestampEndUs - frameRenderTimestampStartUs;
        ssFrameRenderCount++;

        // Pass the frame buffer from WASM to GUI (double buffering)
        GuiWriteDinoRgb565Buffer(240, 75, (const uint16_t *)(mem + 0x5000));

        uint32_t frameTimeUs = (uint32_t)(micros() - ssLastFrameTimestampUs);
        ssLastFrameTimestampUs = micros();

        if (millis() - ssLastLogPrintTimeMs >= 1000)
        {
            printf("[WASM] Average frame render time: %lld us\n", ssFrameRenderTimeSum / ssFrameRenderCount);
            printf("[WASM] FPS: %d\n", (uint32_t)(1000000ULL / frameTimeUs));
            ssFrameRenderCount = 0;
            ssFrameRenderTimeSum = 0;
            ssLastLogPrintTimeMs = millis();
        }

        // TODO: We don't hold the FPS steady.
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    printf("[WASM] Task ended\n");

    if (result != m3Err_none)
    {
        M3ErrorInfo info;
        m3_GetErrorInfo(runtime, &info);
        printf("[WASM] Error: %s (%s)", result, info.message);

        if (info.file && strlen(info.file) && info.line)
        {
            printf(" at %s: %d\n", info.file, info.line);
        }

        printf("\n");
    }
}
