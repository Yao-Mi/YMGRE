#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Rasterization.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define W 96
#define CHECK(c) do{if(!(c)){fprintf(stderr,"wire overlay line %d: %s\n",__LINE__,#c);exit(1);}}while(0)
static GRErgb24 wire={30,250,60};
static GRE_Object4d triangle(GRE_Camera4d c,const float xy[6],float depth)
{
    GRE_Object4d o=YMGRE_Creat_Object(3,1,"wire","");
    o->isVisible=1;o->boundType=GRE_Bounding_Sphere_R;o->BoundingSphereR=1000;
    for(int i=0;i<3;i++) {
        float x=xy[2*i],y=xy[2*i+1],z=depth>0?depth:1/(.004f+.00015f*x+.00008f*y);
        o->pointList[i].pos=(gre_fvector4d){
            (c->perspectPlane.pL+x/W*(c->perspectPlane.pR-c->perspectPlane.pL))*z/c->perspectPlane.Dis,
            (c->perspectPlane.pU-y/W*(c->perspectPlane.pU-c->perspectPlane.pD))*z/c->perspectPlane.Dis,z,1};
    }
    gre_polygon4d* p=o->polygonList;memset(p,0,sizeof(*p));
    p->num=3;p->index=GRE_PolyIndex_Malloc(3*sizeof(GRE_Index));
    for(int i=0;i<3;i++)p->index[i]=i;
    p->pN=(gre_fvector4d){0,0,-1,0};p->planeColor=(GRErgb24){70,100,210};
    return o;
}
static void render(GRE_Camera4d c,gre_list* objects,GRE_RenderWorkspace workspace)
{
    gre_list empty={0};YMGRE_Img_SetBrushColor(wire);
    if(workspace)YMGRE_Camera_TanglePipline_RenderingWithWorkspace(c,&empty,objects,&empty,workspace);
    else YMGRE_Camera_TanglePipline_Rendering(c,&empty,objects,&empty);
}
int main(void)
{
    GRE_Camera4d c=YMGRE_Creat_Camera(0,W,W,45,45,45,45);
    gre_fvector4d eye={0,0,0,1},target={0,0,1,1};
    YMGRE_UVNCamera_PositionInit(c,&eye,&target,NULL,0);YMGRE_Camera_Frustum_Init(c,1,500);
    GRE_RenderWorkspace workspace=YMGRE_Creat_RenderWorkspace();
    const float cases[][6]={{8.2f,7.8f,84.7f,22.35f,18.4f,85.2f},{-12.3f,14.2f,90.4f,33.8f,32.7f,113.1f}};
    for(int entry=0;entry<2;entry++)for(int k=0;k<2;k++) {
        GRE_Object4d o=triangle(c,cases[k],0);gre_list objects={0};
        YMGRE_List_Append(&objects,sizeof(*o),o);
        c->wireFrame=GRE_Render_Wireframe;o->wireFrame=0;render(c,&objects,entry?workspace:NULL);
        unsigned char mask[W*W];int count=0;
        for(int i=0;i<W*W;i++){mask[i]=GRE_FramePixel_Equals(c->img.data[i],GRE_FramePixel_From_RGB24(wire));count+=mask[i];}
        CHECK(count>100);
        c->wireFrame=GRE_Render_Solid;o->wireFrame=1;render(c,&objects,entry?workspace:NULL);
        int missing=0;
        for(int i=0;i<W*W;i++)if(mask[i]&&!GRE_FramePixel_Equals(c->img.data[i],GRE_FramePixel_From_RGB24(wire)))missing++;
        printf("entry=%d case=%d wire=%d missing=%d\n",entry,k,count,missing);
        CHECK(missing==0);
        // A nearer surface hides these wires in either submission order.
        const float front[6]={-96,-96,288,-96,-96,288};
        GRE_Object4d occluder=triangle(c,front,20);
        YMGRE_List_Append(&objects,sizeof(*occluder),occluder);
        for(int order=0;order<2;order++) {
            render(c,&objects,entry?workspace:NULL);
            for(int i=0;i<W*W;i++)CHECK(!GRE_FramePixel_Equals(c->img.data[i],GRE_FramePixel_From_RGB24(wire)));
            objects.listhead->data=occluder;objects.listhead->next->data=o;
        }
        YMGRE_List_Clear(&objects,YMGRE_Free_Object);
    }
    YMGRE_Free_RenderWorkspace(workspace);YMGRE_Free_Camera(c);
    puts("Wire overlay PASS: fractional/clipped edges continuous; foreground occlusion preserved");
    return 0;
}
