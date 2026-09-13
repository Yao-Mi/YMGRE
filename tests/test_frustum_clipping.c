#include "YMGRE_CullingAndClipping.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_Coordinates_Transform.h"
#include "YMGRE_Rasterization.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_MathBase.h"
#include <stdio.h>
#include <string.h>

static int projectedCullingPass(GRE_Camera4d camera)
{
	float l=camera->perspectPlane.pL,r=camera->perspectPlane.pR;
	float d=camera->perspectPlane.pD,u=camera->perspectPlane.pU;
	/* No vertex is inside, yet this triangle covers the center of the viewport. */
	gre_vertex4d points[3]={{{l*2,d*2,100,1},0,0},{{r*2,d*2,100,1},1,0},{{0,u*2,100,1},.5f,1}};
	uint16 indices[3]={0,1,2};gre_polygon4d polygon={0};polygon.num=3;polygon.index=indices;
	gre_object4d object={0};object.pointNum=3;object.pointList_=points;object.polygonNum=1;object.polygonList=&polygon;
	gre_vertex4d_wN advanced[3]={0};
	for(int entry=0;entry<3;entry++) {
		uint8 hidden=0;polygon.ishide=0;
		if(entry==0){YMGRE_ObjectPoly_FrustumCulling(&object,camera);hidden=polygon.ishide;}
		else if(entry==1)YMGRE_ObjectPoly_FrustumCullingTo(&object,points,camera,&hidden);
		else {for(int i=0;i<3;i++)advanced[i].base=points[i];YMGRE_ObjectPoly_FrustumCullingTo_wN(&object,advanced,camera,&hidden);}
		if(hidden) {printf("projected culling entry %d incorrectly hides a crossing triangle\n",entry);return 0;}
	}
	/* Both exact boundary vertices and prior backface/visibility decisions survive. */
	gre_vertex4d original[3];memcpy(original,points,sizeof(points));
	points[0].pos=(gre_fvector4d){l,d,100,1};points[1].pos=(gre_fvector4d){r,d,100,1};points[2].pos=(gre_fvector4d){0,u,100,1};
	uint8 hidden=0;YMGRE_ObjectPoly_FrustumCullingTo(&object,points,camera,&hidden);if(hidden)return 0;
	hidden=1;YMGRE_ObjectPoly_FrustumCullingTo(&object,points,camera,&hidden);if(!hidden)return 0;
	for(int plane=0;plane<6;plane++) {
		memcpy(points,original,sizeof(points));
		for(int i=0;i<3;i++) {
			switch(plane){case 0:points[i].pos.x=l-1;break;case 1:points[i].pos.x=r+1;break;
			case 2:points[i].pos.y=d-1;break;case 3:points[i].pos.y=u+1;break;
			case 4:points[i].pos.z=camera->frustum.Znear-1;break;default:points[i].pos.z=camera->frustum.Zfar+1;break;}
		}
		hidden=0;YMGRE_ObjectPoly_FrustumCullingTo(&object,points,camera,&hidden);if(!hidden)return 0;
	}
	memcpy(points,original,sizeof(points));hidden=0;
	YMGRE_ObjectPoly_FrustumCullingTo(&object,points,camera,&hidden);
	YMGRE_VertexList_ViewPlaneToWindows(points,3,camera);
	YMGRE_CameraImage_Init(camera,(GRErgb24){50,50,50});GRErgb24 color={92,155,220};
	YMGRE_TrangleObject_Primitive_RasterizationTo(&object,points,&hidden,&color,NULL,camera);
	int center=(camera->img.height/2)*camera->img.width+camera->img.width/2;
	return GRE_FramePixel_Equals(camera->img.data[center],GRE_FramePixel_From_RGB24(color));
}

static int nearPlaneRenderingPass(void)
{
	GRE_Camera4d camera=YMGRE_Creat_Camera(0,560,540,45,45,45,45);
	YMGRE_Camera_Frustum_Init(camera,.001f,100);
	camera->perspectPlane.pL=-103.703705f;camera->perspectPlane.pR=103.703705f;
	camera->perspectPlane.kl=camera->perspectPlane.pL/100;camera->perspectPlane.kr=camera->perspectPlane.pR/100;
	memset(&camera->move.TMat,0,sizeof(camera->move.TMat));
	for(int i=0;i<4;i++)camera->move.TMat.val[i][i]=1;
	camera->pos=(gre_fvector4d){0,0,0,1};
	GRE_Object4d object=YMGRE_Creat_Object(3,1,"near-plane regression","near material");
	/* Actual missing ground triangle 616 from the saved editor state. */
	object->pointList[0]=(gre_vertex4d){{-2.4609689f,-1.5584301f,2.1899404f,1},0,0};
	object->pointList[1]=(gre_vertex4d){{-1.8338690f,-2.0286086f,-.07847003f,1},1,0};
	object->pointList[2]=(gre_vertex4d){{-.14434498f,-1.4311551f,2.8039882f,1},0,1};
	gre_vertex4d original[3];memcpy(original,object->pointList,sizeof(original));
	GRE_Polygon4d face=object->polygonList;face->num=3;face->index=GRE_PolyIndex_Malloc(3*sizeof(uint16));
	for(int i=0;i<3;i++)face->index[i]=i;
	face->pN=(gre_fvector4d){0,1,0,0};face->planeColor=(GRErgb24){92,155,220};
	object->isVisible=1;object->boundType=GRE_Bounding_Sphere_R;object->BoundingSphereR=10;
	object->WorldCoordinate=(gre_fvector4d){0,0,0,1};
	/* A 2x2 texture checks that clipped UVs still interpolate over the face. */
	GRErgb24 texels[4]={{240,40,30},{40,230,20},{30,50,240},{230,210,30}};
	gre_material material={0};material.name=object->materiaName;material.valid=1;material.unlit=1;
	material.nameLen=strlen(material.name)+1;
	material.pixel=texels;material.width=material.height=2;
	gre_listnode objectNode={sizeof(*object),object,NULL},materialNode={sizeof(material),&material,NULL};
	gre_list objects={&objectNode,1},materials={&materialNode,1},lights={0};
	GRE_RenderWorkspace workspace=YMGRE_Creat_RenderWorkspace();int ok=1;
	for(int external=0;external<2;external++) {
		if(external)YMGRE_Camera_TanglePipline_RenderingWithWorkspace(camera,&lights,&objects,&materials,workspace);
		else YMGRE_Camera_TanglePipline_Rendering(camera,&lights,&objects,&materials);
		int at=490*560+100;
		/* Ray/triangle intersection gives z=1.968858 and UV=(.215835,.437295). */
		if(!GRE_FramePixel_Equals(camera->img.data[at],GRE_FramePixel_From_RGB24(texels[0])) ||
			fabsf(camera->img.zbuff[at]-1.968858f)>.003f) {printf("near-plane rendering entry %d: depth=%g\n",external,camera->img.zbuff[at]);ok=0;}
		if(memcmp(original,object->pointList,sizeof(original)))ok=0;
	}
	YMGRE_Free_RenderWorkspace(workspace);YMGRE_Free_Object(object);YMGRE_Free_Camera(camera);return ok;
}

static int vertexInside(GRE_Vertex4d vertex, GRE_Camera4d camera)
{
	return (vertex->pos.z >= camera->frustum.Znear - 0.001f) &&
		(vertex->pos.z <= camera->frustum.Zfar + 0.001f) &&
		(vertex->pos.x >= camera->perspectPlane.kl * vertex->pos.z - 0.001f) &&
		(vertex->pos.x <= camera->perspectPlane.kr * vertex->pos.z + 0.001f) &&
		(vertex->pos.y >= camera->perspectPlane.kd * vertex->pos.z - 0.001f) &&
		(vertex->pos.y <= camera->perspectPlane.ku * vertex->pos.z + 0.001f);
}

static int clippingCasePass(GRE_Camera4d camera, gre_vertex4d input[3])
{
	gre_vertex4d output[YMGRE_FRUSTUM_CLIP_VERTEX_MAX];
	uint16 outputNum = YMGRE_Polygon_FrustumClip(input, 3, output,
		YMGRE_FRUSTUM_CLIP_VERTEX_MAX, camera);
	if (outputNum < 3)
		return 0;
	for (uint16 i = 0; i < outputNum; i++)
	{
		if (!vertexInside(&output[i], camera))
			return 0;
	}
	return 1;
}

int main(void)
{
	if(!nearPlaneRenderingPass()) {printf("test_frustum_clipping: near-plane rendering FAILED\n");return 1;}
	GRE_Camera4d camera = YMGRE_Creat_Camera(0, 64, 64, 45.0f, 45.0f, 45.0f, 45.0f);
	YMGRE_Camera_Frustum_Init(camera, 40.0f, 150.0f);
	if(!projectedCullingPass(camera)) {printf("test_frustum_clipping: projected culling FAILED\n");return 1;}
	gre_vertex4d cases[6][3] = {
		{ { { -30, -30, 20, 1 }, 0, 0 }, { { 30, -30, 100, 1 }, 1, 0 }, { { 0, 30, 100, 1 }, 0.5f, 1 } },
		{ { { -30, -30, 180, 1 }, 0, 0 }, { { 30, -30, 100, 1 }, 1, 0 }, { { 0, 30, 100, 1 }, 0.5f, 1 } },
		{ { { -140, 0, 100, 1 }, 0, 0 }, { { 30, -30, 100, 1 }, 1, 0 }, { { 30, 30, 100, 1 }, 0.5f, 1 } },
		{ { { 140, 0, 100, 1 }, 0, 0 }, { { -30, -30, 100, 1 }, 1, 0 }, { { -30, 30, 100, 1 }, 0.5f, 1 } },
		{ { { 0, -140, 100, 1 }, 0, 0 }, { { -30, 30, 100, 1 }, 1, 0 }, { { 30, 30, 100, 1 }, 0.5f, 1 } },
		{ { { 0, 140, 100, 1 }, 0, 0 }, { { -30, -30, 100, 1 }, 1, 0 }, { { 30, -30, 100, 1 }, 0.5f, 1 } }
	};
	for (uint16 i = 0; i < 6; i++)
	{
		if (!clippingCasePass(camera, cases[i]))
		{
			printf("test_frustum_clipping: plane=%u FAILED\n", i);
			return 1;
		}
	}

	gre_vertex4d output[YMGRE_FRUSTUM_CLIP_VERTEX_MAX];
	gre_vertex4d behind[3] = {
		{ { -10.0f, -10.0f, -20.0f, 1.0f }, 0.0f, 0.0f },
		{ { 10.0f, -10.0f, -20.0f, 1.0f }, 1.0f, 0.0f },
		{ { 0.0f, 10.0f, -20.0f, 1.0f }, 0.5f, 1.0f }
	};
	if (YMGRE_Polygon_FrustumClip(behind, 3, output,
		YMGRE_FRUSTUM_CLIP_VERTEX_MAX, camera) != 0)
	{
		printf("test_frustum_clipping: behind-camera polygon survived FAILED\n");
		return 1;
	}

	printf("test_frustum_clipping: all six planes PASS\n");
	YMGRE_Free_Camera(camera);
	return 0;
}
