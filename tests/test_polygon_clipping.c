#include "YMGRE_Rasterization.h"
#include "YMGRE_CullingAndClipping.h"
#include "YMGRE_Camera.h"
#include <stdio.h>
#include <string.h>

#define TEST_W 64
#define TEST_H 64
#define SOURCE_LINE_NUM 5

/* Captured from scene_editor while browsing myscene.scene: all four viewport
 * planes cut the triangle, producing seven edges rather than six. */
static int sevenEdgeRegression(void)
{
	gre_vertex4d points[3] = {0};
	points[0].pos = (gre_fvector4d){-209.926453f, 635.152588f, 100, 1};
	points[1].pos = (gre_fvector4d){2673.96704f, -2289.23267f, 100, 1};
	points[2].pos = (gre_fvector4d){152.514435f, 533.976685f, 100, 1};
	gre_fline edges[3];
	for (int i=0;i<3;i++) {
		gre_fvector4d a=points[i].pos,b=points[(i+1)%3].pos;
		edges[i]=(gre_fline){a.x,a.y,b.x,b.y};
	}
	gre_flineslist input={3,3,edges};
	gre_frect window={0,0,559,539};
	struct {gre_line edges[7]; unsigned guard;} storage;
	memset(&storage,0x5a,sizeof(storage));
	gre_lineslist output={7,storage.edges};
	YMGRE_Polygon_clip2D(&input,&output,&window);
	if(output.lineNum!=7 || storage.guard!=0x5a5a5a5aU) return 0;
	for(unsigned i=0;i<7;i++) {
		gre_line e=output.data[i];
		if(e.x0<0||e.x0>559||e.x1<0||e.x1>559||e.y0<0||e.y0>539||e.y1<0||e.y1>539) return 0;
	}
	/* Undersized destinations must remain untouched and return no partial shape. */
	for(unsigned capacity=0;capacity<7;capacity++) {
		memset(&storage,0x5a,sizeof(storage)); output.lineNum=capacity;
		YMGRE_Polygon_clip2D(&input,&output,&window);
		if(output.lineNum) return 0;
		const unsigned char* bytes=(const unsigned char*)&storage;
		for(unsigned i=0;i<sizeof(storage);i++) if(bytes[i]!=0x5a) return 0;
	}
	static GRE_FramePixel frame[560*540]; static float32 depth[560*540];
	gre_camera4d camera={0};camera.img.width=560;camera.img.height=540;
	camera.img.data=frame;camera.img.zbuff=depth;camera.frustum.Znear=1;camera.frustum.Zfar=500;
	GRE_Index indices[3]={0,1,2};gre_polygon4d polygon={0};polygon.num=3;polygon.index=indices;
	gre_object4d object={0};object.pointNum=3;object.pointList_=points;object.polygonNum=1;object.polygonList=&polygon;
	uint8 hide=0;GRErgb24 ink={40,255,80};YMGRE_Img_SetBrushColor(ink);
	for(int external=0;external<2;external++) {
		YMGRE_CameraImage_Init(&camera,(GRErgb24){0,0,0});
		if(external) YMGRE_TrangleObject_WiresTo(&object,points,&hide,&camera);
		else YMGRE_TrangleObject_Wires(&object,&camera);
		int drawn=0;for(int i=0;i<560*540;i++) if(GRE_FramePixel_Equals(frame[i],GRE_FramePixel_From_RGB24(ink))) drawn++;
		if(drawn<100) return 0;
	}
	return 1;
}

int main(void)
{
	if(!sevenEdgeRegression()) {printf("test_polygon_clipping: seven-edge scene regression FAILED\n");return 1;}
	gre_fline sourceLines[SOURCE_LINE_NUM] = {
		{ 5, 25, 25, 5 }, { 25, 5, 58, 22 }, { 58, 22, 52, 58 },
		{ 52, 58, 12, 54 }, { 12, 54, 5, 25 }
	};
	gre_flineslist input = { SOURCE_LINE_NUM, SOURCE_LINE_NUM, sourceLines };
	gre_line clippedLines[10];
	gre_lineslist output = { 10, clippedLines };
	gre_frect window = { 16, 16, 48, 48 };
	YMGRE_Polygon_clip2D(&input, &output, &window);

	if (output.lineNum == 0)
	{
		printf("test_polygon_clipping: empty result FAILED\n");
		return 1;
	}
	for (uint16 i = 0; i < output.lineNum; i++)
	{
		GRE_LINE line = &output.data[i];
		if ((line->x0 < window.x0) || (line->x0 > window.x1) ||
			(line->y0 < window.y0) || (line->y0 > window.y1) ||
			(line->x1 < window.x0) || (line->x1 > window.x1) ||
			(line->y1 < window.y0) || (line->y1 > window.y1))
		{
			printf("test_polygon_clipping: endpoint outside window FAILED\n");
			return 1;
		}
	}

	GRE_FramePixel frame[TEST_W * TEST_H];
	float32 depth[TEST_W * TEST_H];
	gre_camera4d camera = { 0 };
	camera.img.width = TEST_W;
	camera.img.height = TEST_H;
	camera.img.data = frame;
	camera.img.zbuff = depth;
	camera.frustum.Znear = 1.0f;
	camera.frustum.Zfar = 500.0f;
	GRErgb24 background = { 0, 0, 0 };
	GRErgb24 color = { 180, 120, 60 };
	gre_fvector4d plane = { 0, 0, 1, -100 };
	YMGRE_CameraImage_Init(&camera, background);
	YMGRE_Img_Scanline_AreaFill(frame, TEST_W, TEST_H, &output, &plane, depth, color);

	GRE_FramePixel fillPixel = GRE_FramePixel_From_RGB24(color);
	int fillCount = 0;
	for (int y = 0; y < TEST_H; y++)
	{
		for (int x = 0; x < TEST_W; x++)
		{
			if (GRE_FramePixel_Equals(frame[y * TEST_W + x], fillPixel))
			{
				fillCount++;
				if ((x < window.x0) || (x > window.x1) || (y < window.y0) || (y > window.y1))
				{
					printf("test_polygon_clipping: fill outside window FAILED\n");
					return 1;
				}
			}
		}
	}

	if (fillCount == 0)
	{
		printf("test_polygon_clipping: empty fill FAILED\n");
		return 1;
	}
	printf("test_polygon_clipping: lines=%u fill=%d PASS\n", output.lineNum, fillCount);
	return 0;
}
