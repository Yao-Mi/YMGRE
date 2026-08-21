#include "../YMGRE/PORT/YMGRE_YMGUI_LCD.h"
#include "../YMGRE/CORE/YMGRE_ScenceManager.h"

int main(void)
{
	gre_scence scene = { 0 };
	LCD_Init(800, 600);
	YMGRE_Scene_Rendering(&scene);
	LCD_Destory();
	return 0;
}
