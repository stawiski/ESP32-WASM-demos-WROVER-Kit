#ifndef SCREEN_H_INCLUDED
#define SCREEN_H_INCLUDED

#define GUI_TASK_STACK 8192

#ifdef __cplusplus
extern "C" {
#endif

void GuiTask(void *pvParameter);
void GuiWriteDinoRgb565Buffer(uint16_t width, uint16_t height, const uint16_t* pBuffer);

#ifdef __cplusplus
extern }
#endif

#endif // SCREEN_H_INCLUDED