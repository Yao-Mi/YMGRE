#include "demo_host.h"
#include "YMGRE_RayTracing.h"
#include "YMGRE_Camera.h"
#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Coordinates_Transform.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"
#include <stdio.h>

static GRErgb24 shade(GRErgb24 base, const gre_fvector4d* p, const gre_fvector4d* n,
	const gre_fvector4d* light, int shadow)
{
	gre_fvector4d l = { light->x-p->x, light->y-p->y, light->z-p->z, 0 };
	float32 d = YMGRE_Fvector4d_Len1(&l); if (d < 1e-5f) d = 1e-5f;
	YMGRE_Fvector4d_Normalize(&l);
	float32 ndl = n->x*l.x + n->y*l.y + n->z*l.z; if (ndl < 0) ndl = 0;
	float32 intensity = shadow ? 0.08f : 0.12f + 2.0f*ndl/(1.0f+0.025f*d*d);
	if (intensity > 1) intensity = 1;
	return (GRErgb24){(uint8)(base.R*intensity),(uint8)(base.G*intensity),(uint8)(base.B*intensity)};
}

int main(void)
{
	const uint16 width=560,height=420;
	GRE_Camera4d camera=YMGRE_Creat_Camera(0,width,height,38,38,31,31);
	GRE_Object4d cube=YMGRE_MeshGener_Cube(3.2f,(GRErgb24){210,135,45},"shadow_cube","ray");
	GRE_Object4d floor=YMGRE_MeshGener_RectPlane(13,12,12,12,(GRErgb24){170,180,190},"shadow_floor","ray");
	if(!camera||!cube||!floor)return 1;
	cube->WorldCoordinate=(gre_fvector4d){0,0,9,1}; YMGRE_Object_LocalToWorld(cube);
	floor->WorldCoordinate=(gre_fvector4d){0,-1.7f,10,1}; YMGRE_Object_LocalToWorld(floor);
	YMGRE_Camera_Frustum_Init(camera,1,100);
	gre_fvector4d eye={6,5,0,1},target={0,0,9,1}; YMGRE_UVNCamera_PositionInit(camera,&eye,&target,NULL,0);
	gre_list objects={0}; YMGRE_List_Append(&objects,sizeof(GRE_Object4d),cube); YMGRE_List_Append(&objects,sizeof(GRE_Object4d),floor);
	gre_fvector4d light={-4,6,3,1}; GRE_RenderTarget out=YMGRE_Camera_GetRenderTarget(camera);
	uint32 floorHits=0,shadowPixels=0,litPixels=0;
	for(uint16 y=0;y<height;y++)for(uint16 x=0;x<width;x++){
		gre_ray ray; gre_ray_scene_hit hit; GRErgb24 color={8,11,18};
		if(YMGRE_Ray_FromCameraPixel(camera,x,y,&ray)&&YMGRE_Ray_IntersectScene(&ray,&objects,.001f,100,&hit)){
			int blocked=0;
			if(hit.object==floor){
				floorHits++; gre_fvector4d toLight={light.x-hit.hit.position.x,light.y-hit.hit.position.y,light.z-hit.hit.position.z,0};
				float32 dist=YMGRE_Fvector4d_Len1(&toLight); YMGRE_Fvector4d_Normalize(&toLight);
				gre_ray shadow={{hit.hit.position.x+toLight.x*.01f,hit.hit.position.y+toLight.y*.01f,hit.hit.position.z+toLight.z*.01f,1},toLight}; gre_ray_scene_hit blocker;
				blocked=YMGRE_Ray_IntersectScene(&shadow,&objects,.001f,dist-.02f,&blocker)&&blocker.object==cube;
				if(blocked)shadowPixels++;else litPixels++;
			}
			color=shade(hit.object->polygonList[hit.polygonIndex].planeColor,&hit.hit.position,&hit.hit.normal,&light,blocked);
		}
		out->data[y*width+x]=GRE_FramePixel_From_RGB24(color);
	}
	int pass=floorHits>10000&&shadowPixels>100&&litPixels>100;
	printf("ray shadow: floor=%u shadow=%u lit=%u: %s\n",floorHits,shadowPixels,litPixels,pass?"PASS":"FAIL");
	YMGRE_DemoView view={out,0,0,width,height}; YMGRE_DemoHost_Show(width,height,&view,1,60);
	YMGRE_List_Clear(&objects,YMGRE_Free_Object); YMGRE_Free_Camera(camera); return pass?0:1;
}
