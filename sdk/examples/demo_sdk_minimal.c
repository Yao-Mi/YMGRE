/* Only the precompiled core archive and libm; no SDL/YMGUI or engine sources. */
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_Camera.h"
#include <stdio.h>

int main(void)
{
    gre_list objects={0}, lights={0}, materials={0};
    GRE_Object4d cube=YMGRE_MeshGener_Cube(30,(GRErgb24){50,100,230},"cube","");
    cube->WorldCoordinate=(gre_fvector4d){0,0,80,1};
    YMGRE_Object_LocalToWorld(cube);
    GRE_Light4d ambient=YMGRE_Creat_Light(0,GRE_GlobalLight,(GRErgb24){255,255,255},1);
    YMGRE_List_Append(&objects,sizeof(*cube),cube);
    YMGRE_List_Append(&lights,sizeof(*ambient),ambient);
    GRE_Camera4d camera=YMGRE_Creat_Camera(0,128,128,35,35,35,35);
    YMGRE_Camera_Frustum_Init(camera,1,500);
    gre_fvector4d eye={0,0,0,1},target={0,0,80,1};
    YMGRE_UVNCamera_PositionInit(camera,&eye,&target,NULL,0);
    GRE_RenderWorkspace workspace=YMGRE_Creat_RenderWorkspace();
    YMGRE_Camera_TanglePipline_RenderingWithWorkspace(camera,&lights,&objects,&materials,workspace);
    unsigned visible=0;
    for(int i=0;i<128*128;i++)if(camera->img.zbuff[i]<500)visible++;
    FILE* image=fopen("cube.ppm","wb");
    int ok=visible>100 && image!=NULL;
    if(image) {
        fprintf(image,"P6\n128 128\n255\n");
        for(int i=0;i<128*128;i++) {
            GRErgb24 c=GRE_FramePixel_To_RGB24(camera->img.data[i]);
            fputc(c.R,image);fputc(c.G,image);fputc(c.B,image);
        }
        if(ferror(image))ok=0;
        if(fclose(image))ok=0;
    }
    printf("GRE_Index=%zu bytes, pixel=%zu bytes, visible=%u: %s\n",
        sizeof(GRE_Index),sizeof(GRE_FramePixel),visible,ok?"PASS":"FAIL");
    YMGRE_Free_RenderWorkspace(workspace);YMGRE_Free_Camera(camera);
    YMGRE_List_Clear(&lights,YMGRE_Free_Light);YMGRE_List_Clear(&objects,YMGRE_Free_Object);
    return ok?0:1;
}
