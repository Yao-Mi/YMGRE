#include "grass_impostor.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_CullingAndClipping.h"
#include <math.h>
#include <string.h>

int grass_impostor_bake(grass_impostor *out,GRE_Object4d source,GRE_List materials,GRE_List lights) {
    gre_fvector4d lo={1e9f,1e9f,1e9f,1},hi={-1e9f,-1e9f,-1e9f,1};
    for(GRE_Object4d o=source;o;o=o->nextObject) {
        if(!YMGRE_Object_GenerateVertexAttributes(o))return 0;
        o->renderMode=GRE_RenderMode_Vertex;o->mirrorKs=0;
        for(int i=0;i<o->pointNum;i++) {
            gre_fvector4d p=o->pointList[i].pos;
            lo.x=fminf(lo.x,p.x);lo.y=fminf(lo.y,p.y);lo.z=fminf(lo.z,p.z);
            hi.x=fmaxf(hi.x,p.x);hi.y=fmaxf(hi.y,p.y);hi.z=fmaxf(hi.z,p.z);
        }
    }
    out->center=(gre_fvector4d){(lo.x+hi.x)*.5f,(lo.y+hi.y)*.5f,(lo.z+hi.z)*.5f,1};
    float size=fmaxf(hi.y-lo.y,hypotf(hi.x-lo.x,hi.z-lo.z));
    size=fmaxf(size,.01f);out->span=size*1.3f;
    float distance=size*24,halfAngle=atanf(out->span*.5f/distance)*57.2957795f;
    GRE_Camera4d camera=YMGRE_Creat_Camera(0,GRASS_TILE,GRASS_TILE,halfAngle,halfAngle,halfAngle,halfAngle);
    GRE_RenderWorkspace ws=YMGRE_Creat_RenderWorkspace();
    if(!camera||!ws) { YMGRE_Free_Camera(camera);YMGRE_Free_RenderWorkspace(ws);return 0; }
    YMGRE_Camera_Frustum_Init(camera,size*.01f,distance*2);
    gre_listnode node={sizeof(gre_object4d),source,NULL};gre_list objects={&node,1};
    int opaquePixels=0;
    for(int view=0;view<GRASS_VIEWS;view++) {
        float angle=view*6.283185307f/GRASS_VIEWS;
        gre_fvector4d eye={out->center.x+sinf(angle)*distance,out->center.y,out->center.z+cosf(angle)*distance,1};
        YMGRE_UVNCamera_PositionInit(camera,&eye,&out->center,NULL,0);
        YMGRE_Camera_TanglePipline_wN(camera,lights,&objects,materials,ws);
        GRE_RenderTarget target=YMGRE_Camera_GetRenderTarget(camera);grass_view *v=&out->views[view];
        v->left=v->top=GRASS_TILE;v->right=v->bottom=-1;
        for(int y=0;y<GRASS_TILE;y++)for(int x=0;x<GRASS_TILE;x++) {
            int i=y*GRASS_TILE+x;v->color[i]=GRE_FramePixel_To_RGB24(target->data[i]);
            v->mask[i]=target->zbuff[i]<camera->frustum.Zfar;
            if(v->mask[i]) { opaquePixels++;if(x<v->left)v->left=x;if(x>v->right)v->right=x;if(y<v->top)v->top=y;if(y>v->bottom)v->bottom=y; }
        }
    }
    YMGRE_Free_RenderWorkspace(ws);YMGRE_Free_Camera(camera);return opaquePixels>0;
}
/* Opaque cutout rasterization: transparent pixels never write depth. Uses
 * camera z, perspective-correct UVs, and the same target as the mesh pass. */
static void raster(const grass_view *tile,gre_vertex4d_wN *v,GRE_Camera4d camera) {
    float ax=v[0].base.pos.x,ay=v[0].base.pos.y,bx=v[1].base.pos.x,by=v[1].base.pos.y,cx=v[2].base.pos.x,cy=v[2].base.pos.y;
    float den=(by-cy)*(ax-cx)+(cx-bx)*(ay-cy);if(fabsf(den)<1e-6f)return;
    int x0=(int)fmaxf(ceilf(fminf(ax,fminf(bx,cx))),0),x1=(int)fminf(floorf(fmaxf(ax,fmaxf(bx,cx))),camera->img.width-1);
    int y0=(int)fmaxf(ceilf(fminf(ay,fminf(by,cy))),0),y1=(int)fminf(floorf(fmaxf(ay,fmaxf(by,cy))),camera->img.height-1);
    float iz[3]={1/v[0].base.pos.z,1/v[1].base.pos.z,1/v[2].base.pos.z};
    for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++) {
        float a=((by-cy)*(x-cx)+(cx-bx)*(y-cy))/den,b=((cy-ay)*(x-cx)+(ax-cx)*(y-cy))/den,c=1-a-b;
        if(a<0||b<0||c<0)continue;
        a*=iz[0];b*=iz[1];c*=iz[2];float invz=a+b+c;if(invz<=0)continue;
        float z=1/invz;int dst=y*camera->img.width+x;
        if(z<=camera->frustum.Znear||z>=camera->img.zbuff[dst])continue;
        float u=(a*v[0].base.u+b*v[1].base.u+c*v[2].base.u)*z;
        float w=(a*v[0].base.v+b*v[1].base.v+c*v[2].base.v)*z;
        int tx=(int)fminf(fmaxf(u*GRASS_TILE,0),GRASS_TILE-1),ty=(int)fminf(fmaxf(w*GRASS_TILE,0),GRASS_TILE-1);
        int src=ty*GRASS_TILE+tx;if(!tile->mask[src])continue;
        camera->img.data[dst]=GRE_FramePixel_From_RGB24(tile->color[src]);camera->img.zbuff[dst]=z;
    }
}
void grass_impostor_draw(const grass_impostor *asset,GRE_Camera4d source,float x,float y,float z,float scale,float angle) {
    float c=cosf(angle),s=sinf(angle);
    gre_fvector4d center={x+scale*(asset->center.x*c-asset->center.z*s),y+scale*asset->center.y,z+scale*(asset->center.x*s+asset->center.z*c),1};
    /* plant() rotates local coordinates with +X toward +Z: invert that
     * transform when selecting the baked camera azimuth. */
    float bearing=atan2f(source->pos.x-center.x,source->pos.z-center.z)+angle;
    int view=(int)floorf(bearing*(GRASS_VIEWS/6.283185307f)+.5f)%GRASS_VIEWS;if(view<0)view+=GRASS_VIEWS;
    const grass_view *tile=&asset->views[view];if(tile->right<tile->left)return;
    gre_camera4d camera=*source;camera.img=*YMGRE_Camera_GetRenderTarget(source);camera.target=&camera.img;
    gre_vertex4d_wN corners[4]={0};
    float us[4]={tile->left/(float)GRASS_TILE,(tile->right+1)/(float)GRASS_TILE,(tile->right+1)/(float)GRASS_TILE,tile->left/(float)GRASS_TILE};
    float vs[4]={tile->top/(float)GRASS_TILE,tile->top/(float)GRASS_TILE,(tile->bottom+1)/(float)GRASS_TILE,(tile->bottom+1)/(float)GRASS_TILE};
    for(int i=0;i<4;i++) {
        float side=(us[i]-.5f)*asset->span*scale,up=(.5f-vs[i])*asset->span*scale;
        gre_fvector4d p={center.x+source->move.cu.x*side,center.y+up,center.z+source->move.cu.z*side,1};
        YMGRE_Fvector4d_MatMultTo(&camera.move.TMat,&p,&corners[i].base.pos);corners[i].base.u=us[i];corners[i].base.v=vs[i];
    }
    int indices[6]={0,1,2,0,2,3};
    for(int t=0;t<2;t++) {
        gre_vertex4d_wN input[3],clipped[YMGRE_FRUSTUM_CLIP_VERTEX_MAX];for(int i=0;i<3;i++)input[i]=corners[indices[t*3+i]];
        int n=YMGRE_Polygon_FrustumClip_wN(input,3,clipped,YMGRE_FRUSTUM_CLIP_VERTEX_MAX,&camera);if(n<3)continue;
        YMGRE_VertexList_CameraToViewPlane_wN(clipped,n,camera.perspectPlane.Dis);YMGRE_VertexList_ViewPlaneToWindows_wN(clipped,n,&camera);
        for(int i=1;i+1<n;i++) { gre_vertex4d_wN tri[3]={clipped[0],clipped[i],clipped[i+1]};raster(tile,tri,&camera); }
    }
}
