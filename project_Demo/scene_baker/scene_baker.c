/* Headless reference bake using the same UV-space backend as the editor. */
#include "scene_bake.h"
#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include <stdio.h>

int main(int argc,char** argv)
{
    if(argc>2) {fprintf(stderr,"usage: %s [output-directory]\n",argv[0]);return 2;}
    GRE_Object4d cube=YMGRE_MeshGener_Cube(30,(GRErgb24){205,165,105},"Baked Cube","");
    GRE_Light4d ambient=YMGRE_Creat_Light(0,GRE_GlobalLight,(GRErgb24){255,255,255},0.15f);
    GRE_Light4d point=YMGRE_Creat_Light(1,GRE_PointLight,(GRErgb24){255,240,220},1.2f);
    if(!cube || !ambient || !point) {YMGRE_Free_Object(cube);YMGRE_Free_Light(ambient);YMGRE_Free_Light(point);return 1;}
    point->pos=(gre_fvector4d){-40,65,-45,1};
    gre_list lights={0};YMGRE_List_Append(&lights,sizeof(gre_light4d),ambient);YMGRE_List_Append(&lights,sizeof(gre_light4d),point);
    char error[256],path[4096];
    cube->lightmap=SceneBake_Create(cube,&lights,256,error,sizeof(error));
    gre_material surface={.ambient={205,165,105},.diffuse={205,165,105}};
    int ok=cube->lightmap && SceneBake_SaveMaterial(cube->lightmap,SceneBake_Fingerprint(cube,&lights),&surface,
        argc==2?argv[1]:"baked_scene",path,sizeof(path),error,sizeof(error));
    if(ok) printf("UV1 direct diffuse bake: PASS\n%s\n",path);else fprintf(stderr,"%s\n",error);
    YMGRE_Free_Object(cube);YMGRE_List_Clear(&lights,YMGRE_Free_Light);return ok?0:1;
}
