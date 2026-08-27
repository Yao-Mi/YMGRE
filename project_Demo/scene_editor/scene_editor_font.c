#include "scene_editor_font.h"

#include "YMGUI_Debug.h"
#include "YMGUI_Font.h"
#include <stdio.h>

#ifndef GB2312_BIN_PATH
#define GB2312_BIN_PATH "gb2312_glyphs.bin"
#endif

extern const uint16 YMGUI_GB2312_cps[];
extern const uint16 YMGUI_GB2312_glyph_count;

static FILE* g_glyphBlob;

static uint32 readGlyph(const GYfont* font, uint32 offset, uint32 length, uint8* buffer)
{
	(void)font;
	if (g_glyphBlob == NULL || fseek(g_glyphBlob, (long)offset, SEEK_SET) != 0)
		return 0;
	return (uint32)fread(buffer, 1, length, g_glyphBlob);
}

static GYfont g_gb2312Font = {
	NULL, YMGUI_GB2312_cps, 0, 0, 0,
	16, 16, 8, 4, NULL, readGlyph
};

int SceneEditorFont_Init(void)
{
	g_glyphBlob = fopen(GB2312_BIN_PATH, "rb");
	if (g_glyphBlob == NULL) {
		gy_log_print("warn: scene editor GB2312 font not found: %s\n", GB2312_BIN_PATH);
		return 0;
	}
	g_gb2312Font.glyph_count = YMGUI_GB2312_glyph_count;
	YMGUI_Font_SetFallback(&g_gb2312Font);
	return 1;
}

void SceneEditorFont_Shutdown(void)
{
	YMGUI_Font_SetFallback(NULL);
	if (g_glyphBlob != NULL) {
		fclose(g_glyphBlob);
		g_glyphBlob = NULL;
	}
}
