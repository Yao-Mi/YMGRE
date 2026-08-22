#include <stdio.h>
#include "../EGE_LCD/EGE_LCD.h"

//#include "../YMGRE/CORE/YMGRE_Rendering_Pipeline.h"
#include "../YMGRE/CORE/YMGRE_ScenceManager.h"

void main()
{
	LCD_Init(800, 600);
	printf("hello");

	gre_scence mysc = {0,};//初始化场景为空

	YMGRE_Scene_Rendering(&mysc);

	PAUSE();
}

