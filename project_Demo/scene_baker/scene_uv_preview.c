#include "scene_uv_preview.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_Rendering_Pipeline.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
const GRErgb24* SceneUvPreview_Checker(int wide)
{
    static GRErgb24 pixels[2][400*200];static int ready[2];wide=!!wide;int width=wide?400:200;
    if(!ready[wide]){for(int y=0;y<200;y++)for(int x=0;x<width;x++) {
        GRErgb24 c=((x/20+y/20)%2)?(GRErgb24){78,138,201}:(GRErgb24){245,247,250};
        pixels[wide][y*width+x]=c;
    }ready[wide]=1;}return pixels[wide];
}
static int renderPreview(const SceneUvEdit* edit,GRE_List materials,int checker,GRErgb24 fallback,
    const GRErgb24* image,unsigned width,unsigned height,
    float yaw,float pitch,float zoom,unsigned size,GRErgb24* output)
{
    if(!edit||!edit->count||!output||size<16||size>512||!isfinite(yaw)||!isfinite(pitch)||!isfinite(zoom)||zoom<=0)return 0;
    unsigned parts=0;GRE_Object4d previous=NULL;
    for(unsigned i=0;i<edit->count;i++)if(edit->points[i].mesh!=previous){previous=edit->points[i].mesh;if(++parts>64)return 0;}
    /* Rasterize private staged vertices. Topology and material pixels are read-only;
       camera transforms and clipping use a separate workspace. */
    gre_object4d* objects=calloc(parts,sizeof(*objects));gre_material* surfaces=calloc(parts,sizeof(*surfaces));
    gre_listnode* materialNodes=calloc(parts,sizeof(*materialNodes));gre_vertex4d* vertices=calloc(edit->count,sizeof(*vertices));
    gre_vertex4d_wN* attributes=calloc(edit->count,sizeof(*attributes));
    GRE_Camera4d camera=NULL;GRE_RenderWorkspace workspace=NULL;int ok=0;
    if(!objects||!surfaces||!materialNodes||!vertices||!attributes)goto done;
    float lo[3]={INFINITY,INFINITY,INFINITY},hi[3]={-INFINITY,-INFINITY,-INFINITY};
    unsigned part=0;previous=NULL;
    for(unsigned i=0;i<edit->count;i++) {
        const SceneUvEditPoint* p=edit->points+i;GRE_Object4d mesh=p->mesh;
        if(mesh!=previous) {
            if(previous)part++;previous=mesh;objects[part]=*mesh;objects[part].pointList=vertices+i;objects[part].pointList_wN=attributes+i;
            objects[part].nextObject=part+1<parts?objects+part+1:NULL;
            objects[part].lightmap=NULL;objects[part].mirrorKs=0;objects[part].isVisible=1;objects[part].wireFrame=0;
            GRE_Material original=materials?YMGRE_Material_Find(materials,mesh->materiaName):NULL;
            if(original)surfaces[part]=*original;
            else surfaces[part].ambient=surfaces[part].diffuse=fallback;
            objects[part].materiaName=mesh->materiaName?mesh->materiaName:"__uv_preview";
            surfaces[part].name=objects[part].materiaName;surfaces[part].nameLen=strlen(surfaces[part].name)+1;surfaces[part].valid=1;surfaces[part].advanced=NULL;surfaces[part].unlit=0;surfaces[part].specular=(GRErgb24){0};
            if(checker) {
                surfaces[part].ambient=surfaces[part].diffuse=(GRErgb24){255,255,255};
                surfaces[part].pixel=(GRErgb24*)SceneUvPreview_Checker(checker==2);surfaces[part].width=checker==2?400:200;surfaces[part].height=200;
            }
            if(image){surfaces[part].ambient=surfaces[part].diffuse=(GRErgb24){255,255,255};surfaces[part].pixel=(GRErgb24*)image;surfaces[part].width=width;surfaces[part].height=height;}
            materialNodes[part]=(gre_listnode){sizeof(gre_material),surfaces+part,part+1<parts?materialNodes+part+1:NULL};
        }
        if(p->index>=mesh->pointNum||!mesh->pointList_wN||!isfinite(p->u)||!isfinite(p->v))goto done;
        vertices[i]=mesh->pointList[p->index];vertices[i].u=p->u;vertices[i].v=p->v;
        attributes[i]=mesh->pointList_wN[p->index];attributes[i].base=vertices[i];
        float q[3]={vertices[i].pos.x,vertices[i].pos.y,vertices[i].pos.z};
        for(int k=0;k<3;k++){if(!isfinite(q[k]))goto done;lo[k]=fminf(lo[k],q[k]);hi[k]=fmaxf(hi[k],q[k]);}
    }
    gre_fvector4d center={(lo[0]+hi[0])*.5f,(lo[1]+hi[1])*.5f,(lo[2]+hi[2])*.5f,1};
    float radius=0;
    for(unsigned i=0;i<edit->count;i++){gre_fvector4d p=vertices[i].pos;radius=fmaxf(radius,sqrtf((p.x-center.x)*(p.x-center.x)+(p.y-center.y)*(p.y-center.y)+(p.z-center.z)*(p.z-center.z)));}
    if(radius<1e-8f)goto done;
    camera=YMGRE_Creat_Camera(0,size,size,45,45,45,45);workspace=YMGRE_Creat_RenderWorkspace();if(!camera||!workspace)goto done;
    for(unsigned i=0;i<parts;i++) {
        YMGRE_RenderWorkspace_Reserve(workspace,objects[i].pointNum,objects[i].polygonNum,2);
        if(!workspace->pointList||!workspace->polygonHide||!workspace->lightPos||
           !YMGRE_RenderWorkspace_EnableVertexAttributes(workspace,objects[i].pointNum))goto done;
    }
    float distance=radius*(1.05f+.6f/zoom),a=yaw*.01745329252f,b=pitch*.01745329252f;
    gre_fvector4d eye={center.x+distance*sinf(a)*cosf(b),center.y+distance*sinf(b),center.z-distance*cosf(a)*cosf(b),1};
    YMGRE_UVNCamera_PositionInit(camera,&eye,&center,NULL,0);YMGRE_Camera_Frustum_Init(camera,radius*.001f,radius*100);
    gre_light4d ambient={0},lamp={0};ambient.type=GRE_GlobalLight;ambient.proper.strength=.35f;ambient.proper.lightcolor=(GRErgb24){255,255,255};
    lamp.type=GRE_PointLight;lamp.pos=eye;lamp.proper.strength=.65f;lamp.proper.lightcolor=(GRErgb24){255,255,255};lamp.proper.kc0=1;
    gre_listnode objectNode={sizeof(*objects),objects,NULL},lightNodes[2]={{sizeof(ambient),&ambient,lightNodes+1},{sizeof(lamp),&lamp,NULL}};
    gre_list objectList={&objectNode,1},lightList={lightNodes,2},materialList={materialNodes,(int)parts};
    YMGRE_Camera_TanglePipline_wN(camera,&lightList,&objectList,&materialList,workspace);
    for(unsigned i=0;i<size*size;i++)output[i]=GRE_FramePixel_To_RGB24(camera->img.data[i]);ok=1;
done:
    YMGRE_Free_RenderWorkspace(workspace);YMGRE_Free_Camera(camera);free(objects);free(surfaces);free(materialNodes);free(vertices);free(attributes);return ok;
}

int SceneUvPreview_Render(const SceneUvEdit* e,GRE_List m,int c,GRErgb24 f,float yaw,float pitch,float zoom,unsigned size,GRErgb24* out)
{return renderPreview(e,m,c,f,NULL,0,0,yaw,pitch,zoom,size,out);}
int SceneUvPreview_RenderImage(const SceneUvEdit* e,const GRErgb24* p,unsigned w,unsigned h,float yaw,float pitch,float zoom,unsigned size,GRErgb24* out)
{if(!p||!w||!h||w>4096||h>4096)return 0;return renderPreview(e,NULL,0,(GRErgb24){255,255,255},p,w,h,yaw,pitch,zoom,size,out);}
