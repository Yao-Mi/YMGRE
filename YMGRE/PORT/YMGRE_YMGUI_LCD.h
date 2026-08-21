#ifndef YMGRE_YMGUI_LCD_H
#define YMGRE_YMGUI_LCD_H

#include "../CONFIG/YMGRE_PubType.h"

#ifdef __cplusplus
extern "C" {
#endif

void LCD_Init(uint32 width, uint32 height);
void LCD_Destory(void);
int LCD_Update(int fps);
int LCD_GetXY(int startx, int starty, int rew, int reh, int* px, int* py);
int LCD_GetChar(char* c);
void PAUSE(void);
void Delay(int ms);
void LCD_Clear(int color);
void LCD_Fill_RgbRect(int startx, int starty, int width, int height,
                     GRE_FrameBuffer data);

#ifdef __cplusplus
}
#endif

#endif
