#include "grass_impostor.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static grass_impostor red,blue;
static GRE_FramePixel savedColor[64*64];
static float savedDepth[64*64];
static void prepare(grass_impostor *asset,GRErgb24 color) {
    asset->span=2;asset->center=(gre_fvector4d){0,1,0,1};
    for(int v=0;v<GRASS_VIEWS;v++) {
        asset->views[v].left=asset->views[v].top=0;
        asset->views[v].right=asset->views[v].bottom=63;
        for(int y=0;y<64;y++)for(int x=0;x<64;x++) {
            asset->views[v].color[y*64+x]=color;
            asset->views[v].mask[y*64+x]=x>=16&&x<48&&y>=16&&y<48;
        }
    }
}
static void clear(GRE_Camera4d camera,float depth) {
    YMGRE_CameraImage_Init(camera,(GRErgb24){50,20,10});
    for(int i=0;i<64*64;i++)camera->img.zbuff[i]=depth;
}
int main(void) {
    prepare(&red,(GRErgb24){255,0,0});prepare(&blue,(GRErgb24){0,0,255});
    GRE_Camera4d camera=YMGRE_Creat_Camera(0,64,64,35,35,35,35);
    YMGRE_Camera_Frustum_Init(camera,.1f,100);
    gre_fvector4d eye={0,1,0,1},target={0,1,5,1};YMGRE_UVNCamera_PositionInit(camera,&eye,&target,NULL,0);
    clear(camera,10);grass_impostor_draw(&red,camera,0,0,5,1,0);
    GRErgb24 center=GRE_FramePixel_To_RGB24(camera->img.data[32*64+32]);
    if(center.R<240||center.G||fabsf(camera->img.zbuff[32*64+32]-5)>.001f)goto fail;
    /* This pixel is inside the billboard rectangle but outside its mask. */
    GRErgb24 transparent=GRE_FramePixel_To_RGB24(camera->img.data[24*64+24]);
    if(transparent.R!=50||transparent.G!=20||camera->img.zbuff[24*64+24]!=10)goto fail;
    clear(camera,3);grass_impostor_draw(&red,camera,0,0,5,1,0);
    for(int i=0;i<64*64;i++)if(camera->img.zbuff[i]!=3||GRE_FramePixel_To_RGB24(camera->img.data[i]).R!=50)goto fail;
    clear(camera,100);grass_impostor_draw(&blue,camera,0,0,8,1,0);grass_impostor_draw(&red,camera,0,0,5,1,0);
    memcpy(savedColor,camera->img.data,sizeof savedColor);memcpy(savedDepth,camera->img.zbuff,sizeof savedDepth);
    clear(camera,100);grass_impostor_draw(&red,camera,0,0,5,1,0);grass_impostor_draw(&blue,camera,0,0,8,1,0);
    if(memcmp(savedColor,camera->img.data,sizeof savedColor)||memcmp(savedDepth,camera->img.zbuff,sizeof savedDepth))goto fail;
    puts("PASS: cutout pixels preserve background depth, foreground occludes, submission order does not change result");
    YMGRE_Free_Camera(camera);return 0;
fail:
    fprintf(stderr,"Grass impostor depth/mask regression\n");YMGRE_Free_Camera(camera);return 1;
}
