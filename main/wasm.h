#ifndef __WASM_MAIN_H_
#define __WASM_MAIN_H_

#ifdef __cplusplus
extern "C" {
#endif

bool WasmInit(void);
void WasmTask(void *pvParameter);

#ifdef __cplusplus
}
#endif

#endif // __WASM_MAIN_H_