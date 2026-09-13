#include "scene_ray_renderer.h"
#include "scene_bake.h"
#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_Coordinates_Transform.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"ray renderer line %d: %s\n",__LINE__,#c);return 1;}}while(0)
static int finish(SceneRayRenderer* r,GRE_Camera4d c,GRE_List o,GRE_List l,GRE_List m)
{int p=0;for(int i=0;i<1000&&p>=0&&p<100;i++)p=SceneRay_Render(r,c,o,l,m);return p;}
static GRErgb24 center(GRE_Camera4d c){return GRE_FramePixel_To_RGB24(c->img.data[(c->img.height/2)*c->img.width+c->img.width/2]);}
int main(void)
{
    gre_list objects={0},lights={0},materials={0};
    SceneRayRenderer* r=SceneRay_Create();CHECK(r);
    GRE_Camera4d camera=YMGRE_Creat_Camera(0,64,64,45,45,45,45);CHECK(camera);
    gre_fvector4d eye={0,0,-6,1},target={0,0,0,1};
    YMGRE_UVNCamera_PositionInit(camera,&eye,&target,NULL,0);YMGRE_Camera_Frustum_Init(camera,.1f,100);
    GRE_Object4d cube=YMGRE_MeshGener_Cube(2,(GRErgb24){255,255,255},"cube","ray-test");CHECK(cube);
    cube->mirrorKs=0;CHECK(YMGRE_Object_GenerateVertexAttributes(cube));
    YMGRE_List_Append(&objects,sizeof(GRE_Object4d),cube);
    GRE_Material m=YMGRE_Creat_Material("ray-test");CHECK(m);
    m->ambient=m->diffuse=m->specular=(GRErgb24){255,255,255};m->width=m->height=1;
    m->pixel=GRE_ImageBuff_Malloc(sizeof(GRErgb24));CHECK(m->pixel);m->pixel[0]=(GRErgb24){255,255,255};
    YMGRE_List_Append(&materials,sizeof(GRE_Material),m);
    GRE_Light4d ambient=YMGRE_Creat_Light(0,GRE_GlobalLight,(GRErgb24){255,255,255},.1f);
    GRE_Light4d lamp=YMGRE_Creat_Light(1,GRE_PointLight,(GRErgb24){255,255,255},.8f);CHECK(ambient&&lamp);
    lamp->pos=(gre_fvector4d){-3,0,-4,1};lamp->proper.kc0=1;lamp->proper.kc1=lamp->proper.kc2=0;lamp->proper.shadowK=.1f;
    YMGRE_List_Append(&lights,sizeof(GRE_Light4d),ambient);YMGRE_List_Append(&lights,sizeof(GRE_Light4d),lamp);
    CHECK(finish(r,camera,&objects,&lights,&materials)==100);GRErgb24 lit=center(camera);
    CHECK(lit.R>140);CHECK(fabsf(camera->img.zbuff[32*64+32]-5)<.01f);
    GRE_Object4d blocker=YMGRE_MeshGener_Cube(.8f,(GRErgb24){255,255,255},"blocker","ray-test");CHECK(blocker);
    blocker->mirrorKs=0;
    for(int i=0;i<blocker->pointNum;i++){blocker->pointList[i].pos.x-=1.5f;blocker->pointList[i].pos.z-=2.5f;}
    CHECK(YMGRE_Object_GenerateVertexAttributes(blocker));cube->nextObject=blocker;
    CHECK(finish(r,camera,&objects,&lights,&materials)==100);GRErgb24 shadow=center(camera);
    CHECK(shadow.R<40 && lit.R-shadow.R>100);
    lamp->proper.shadowK=0;CHECK(finish(r,camera,&objects,&lights,&materials)==100);CHECK(center(camera).R==lit.R);
    lamp->proper.shadowK=.1f;blocker->isVisible=0;CHECK(finish(r,camera,&objects,&lights,&materials)==100);CHECK(center(camera).R==lit.R);
    lamp->type=GRE_SpotLight;lamp->proper.spot.direct=(gre_fvector4d){3,0,3,0};
    lamp->proper.spot.cs_inner_angle=.94f;lamp->proper.spot.cs_outer_angle=.76f;lamp->proper.spot.cs_div_=1.0f/(.94f-.76f);
    CHECK(finish(r,camera,&objects,&lights,&materials)==100);CHECK(center(camera).R==lit.R);
    lamp->proper.spot.direct=(gre_fvector4d){-3,0,-3,0};
    CHECK(finish(r,camera,&objects,&lights,&materials)==100);CHECK(center(camera).R<40);
    lamp->type=GRE_PointLight;lamp->ishide=1;
    CHECK(finish(r,camera,&objects,&lights,&materials)==100);CHECK(center(camera).R<40);lamp->ishide=0;
    YMGRE_Camera_Frustum_Init(camera,6,100);
    CHECK(finish(r,camera,&objects,&lights,&materials)==100);CHECK(center(camera).R==50);
    YMGRE_Camera_Frustum_Init(camera,.1f,4);
    CHECK(finish(r,camera,&objects,&lights,&materials)==100);CHECK(center(camera).R==50);
    YMGRE_Camera_Frustum_Init(camera,.1f,100);
    /* Albedo sampling, no-light imported materials, and in-place texture changes invalidate cache. */
    m->unlit=1;GRE_ImageBuff_Free(m->pixel);m->pixel=GRE_ImageBuff_Malloc(4*sizeof(GRErgb24));CHECK(m->pixel);
    m->width=m->height=2;memset(m->pixel,0,4*sizeof(GRErgb24));m->pixel[3]=(GRErgb24){92,155,220};
    for(int i=0;i<cube->pointNum;i++)cube->pointList[i].u=cube->pointList[i].v=.75f;
    CHECK(finish(r,camera,&objects,&lights,&materials)==100);CHECK(center(camera).B==220 && center(camera).R==92);
    lamp->proper.strength=0;ambient->proper.strength=0;CHECK(finish(r,camera,&objects,&lights,&materials)==100);CHECK(center(camera).B==220);
    m->pixel[3].B=170;CHECK(finish(r,camera,&objects,&lights,&materials)==100);CHECK(center(camera).B==170);
    m->unlit=0;CHECK(finish(r,camera,&objects,&lights,&materials)==100);CHECK(center(camera).B==0);
    /* Additive specular survives baking; lighting changes never light the atlas a second time. */
    cube->nextObject=NULL;ambient->proper.strength=.1f;lamp->proper.strength=.8f;
    lamp->pos=(gre_fvector4d){0,0,-4,1};lamp->proper.shadowK=0;cube->mirrorKs=.6f;
    CHECK(finish(r,camera,&objects,&lights,&materials)==100);GRErgb24 glossy=center(camera);CHECK(glossy.R>130);
    char error[256];cube->lightmap=SceneBake_CreateSurface(cube,&lights,256,1,&eye,m,error,sizeof(error));
    CHECK(cube->lightmap);
    CHECK(finish(r,camera,&objects,&lights,&materials)==100);GRErgb24 baked=center(camera);CHECK(abs(baked.R-glossy.R)<8);
    ambient->proper.strength=lamp->proper.strength=0;
    CHECK(finish(r,camera,&objects,&lights,&materials)==100);CHECK(center(camera).R==baked.R);
    cube->lightmap->enabled=0;CHECK(finish(r,camera,&objects,&lights,&materials)==100);CHECK(center(camera).R==0);
    /* Reflection must see geometry behind the camera, not a local specular highlight. */
    GRE_Material markerMaterial=YMGRE_Creat_Material("marker");CHECK(markerMaterial);
    markerMaterial->unlit=1;markerMaterial->diffuse=(GRErgb24){230,30,20};
    YMGRE_List_Append(&materials,sizeof(GRE_Material),markerMaterial);
    GRE_Object4d marker=YMGRE_MeshGener_Cube(3,(GRErgb24){255,255,255},"marker","marker");CHECK(marker);
    marker->mirrorKs=0;for(int i=0;i<marker->pointNum;i++)marker->pointList[i].pos.z-=10;
    CHECK(YMGRE_Object_GenerateVertexAttributes(marker));YMGRE_List_Append(&objects,sizeof(GRE_Object4d),marker);
    m->advanced=GRE_malloc0(sizeof(*m->advanced));CHECK(m->advanced);memset(m->advanced,0,sizeof(*m->advanced));
    m->advanced->rayType=1;m->advanced->reflectivity=1;m->advanced->ior=1.5f;m->advanced->transmissionColor=(GRErgb24){255,255,255};
    CHECK(finish(r,camera,&objects,&lights,&materials)==100);CHECK(center(camera).R==230&&center(camera).G==30);
    marker->isVisible=0;CHECK(finish(r,camera,&objects,&lights,&materials)==100);CHECK(center(camera).R==50);
    marker->isVisible=1;for(int i=0;i<marker->pointNum;i++)marker->pointList[i].pos.z+=14;
    markerMaterial->diffuse=(GRErgb24){20,230,30};m->advanced->rayType=2;
    /* A closed triangle mesh must hit its exit faces when tracing refraction. */
    CHECK(finish(r,camera,&objects,&lights,&materials)==100);CHECK(center(camera).G>190&&center(camera).R<40);
    SceneRayPrimitive primitive={.object=cube,.type=1,.radius=1,.halfHeight=1,
        .axis={{1,0,0,0},{0,1,0,0},{0,0,1,0}}};
    SceneRay_SetPrimitives(r,&primitive,1);CHECK(finish(r,camera,&objects,&lights,&materials)==100);
    CHECK(center(camera).G>190&&center(camera).R<40);
    gre_ray ray;gre_ray_hit analyticHit;YMGRE_Ray_FromCameraPixel(camera,32,32,&ray);
    gre_fvector4d zero={0};CHECK(YMGRE_Ray_IntersectSphere(&ray,&zero,1,.1f,100,&analyticHit));
    CHECK(fabsf(camera->img.zbuff[32*64+32]-analyticHit.distance*ray.direction.z)<1e-4f);
    m->advanced->transmissionColor=(GRErgb24){255,80,255};CHECK(finish(r,camera,&objects,&lights,&materials)==100);CHECK(center(camera).G<100);
    m->advanced->transmissionColor=(GRErgb24){255,255,255};
    /* A second glass boundary exercises the medium stack; tint is applied at entry. */
    GRE_Object4d inner=YMGRE_MeshGener_Cube(.8f,(GRErgb24){255,255,255},"inner","ray-test");CHECK(inner);
    inner->mirrorKs=0;CHECK(YMGRE_Object_GenerateVertexAttributes(inner));cube->nextObject=inner;
    CHECK(finish(r,camera,&objects,&lights,&materials)==100);CHECK(center(camera).G>170);
    cube->nextObject=NULL;YMGRE_Free_Object(inner);
    /* Cylinder rotation is applied in local space, including cap intersections. */
    primitive.type=2;primitive.axis[0]=(gre_fvector4d){1,0,0,0};primitive.axis[1]=(gre_fvector4d){0,0,1,0};primitive.axis[2]=(gre_fvector4d){0,-1,0,0};
    primitive.halfHeight=1.7f;SceneRay_SetPrimitives(r,&primitive,1);
    CHECK(finish(r,camera,&objects,&lights,&materials)==100);CHECK(fabsf(camera->img.zbuff[32*64+32]-4.3f)<.001f);
    /* Starting inside a dielectric still finds the exit and transmits the background. */
    eye=(gre_fvector4d){0,0,0,1};target=(gre_fvector4d){0,0,4,1};YMGRE_UVNCamera_PositionInit(camera,&eye,&target,NULL,0);
    CHECK(finish(r,camera,&objects,&lights,&materials)==100);CHECK(center(camera).G>190);
    eye=(gre_fvector4d){0,0,-6,1};target=(gre_fvector4d){0,0,0,1};YMGRE_UVNCamera_PositionInit(camera,&eye,&target,NULL,0);
    SceneRay_SetPrimitives(r,NULL,0);marker->isVisible=0;m->advanced->rayType=0;
    /* Visibility, replacement resources, camera movement, target resize, and an empty scene. */
    cube->isVisible=0;CHECK(finish(r,camera,&objects,&lights,&materials)==100);CHECK(center(camera).R==50);
    cube->isVisible=1;m->unlit=1;
    eye.x=20;target.x=20;YMGRE_UVNCamera_PositionInit(camera,&eye,&target,NULL,0);
    CHECK(finish(r,camera,&objects,&lights,&materials)==100);CHECK(center(camera).R==50);
    GRE_FramePixel pixels[35*21];float depths[35*21];gre_render_target resized={35,21,pixels,depths};camera->target=&resized;
    CHECK(finish(r,camera,&objects,&lights,&materials)==100);CHECK(depths[0]==100);
    gre_list empty={0};CHECK(finish(r,camera,&empty,&lights,&materials)==100);
    /* External overlays must not become part of the persistent image. */
    pixels[0]=GRE_FramePixel_From_RGB24((GRErgb24){255,0,0});depths[0]=0;
    CHECK(SceneRay_Render(r,camera,&empty,&lights,&materials)==100);CHECK(GRE_FramePixel_To_RGB24(pixels[0]).R==50&&depths[0]==100);
    SceneRay_Destroy(r);YMGRE_Free_Camera(camera);YMGRE_Free_Object(blocker);
    YMGRE_List_Clear(&objects,YMGRE_Free_Object);YMGRE_List_Clear(&lights,YMGRE_Free_Light);YMGRE_List_Clear(&materials,YMGRE_Free_Material);
    puts("Ray renderer PASS: depth, BVH/submesh shadows, visibility, textures, unlit, specular, baked atlas, camera, resize, cache.");return 0;
}
