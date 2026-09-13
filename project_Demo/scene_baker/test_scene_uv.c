#include "scene_uv.h"
#include "scene_uv_data.h"
#include "scene_uv_edit.h"
#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_Mem.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"UV test line %d: %s\n",__LINE__,#c);return 1;}}while(0)
/* Independently check nonzero UV areas and test interior overlap by rasterizing dense barycentric samples. */
static int atlasValid(GRE_Object4d mesh)
{
    int* pixels=malloc(1024*1024*sizeof(int));if(!pixels)return 0;
    for(int i=0;i<1024*1024;i++)pixels[i]=-1;
    int id=0,ok=1;
    for(GRE_Object4d m=mesh;m&&ok;m=m->nextObject)for(int i=0;i<m->polygonNum&&ok;i++,id++) {
        gre_vertex4d p[3];for(int j=0;j<3;j++)p[j]=m->pointList[m->polygonList[i].index[j]];
        float area=(p[1].u-p[0].u)*(p[2].v-p[0].v)-(p[1].v-p[0].v)*(p[2].u-p[0].u);
        if(!(fabsf(area)>1e-10f)){ok=0;break;}
        for(int j=0;j<3;j++)if(!(p[j].u>=0&&p[j].u<=1&&p[j].v>=0&&p[j].v<=1))ok=0;
        int x0=1023,y0=1023,x1=0,y1=0;
        for(int j=0;j<3;j++){int x=p[j].u*1024,y=p[j].v*1024;x0=x<x0?x:x0;x1=x>x1?x:x1;y0=y<y0?y:y0;y1=y>y1?y:y1;}
        for(int y=y0;y<=y1&&y<1024&&ok;y++)for(int x=x0;x<=x1&&x<1024;x++) {
            float u=(x+.5f)/1024-p[0].u,v=(y+.5f)/1024-p[0].v;
            float a=(u*(p[2].v-p[0].v)-v*(p[2].u-p[0].u))/area;
            float b=((p[1].u-p[0].u)*v-(p[1].v-p[0].v)*u)/area;
            if(a>.0001f&&b>.0001f&&a+b<.9999f) {
                int at=y*1024+x;if(pixels[at]>=0&&pixels[at]!=id){ok=0;break;}pixels[at]=id;
            }
        }
    }
    free(pixels);return ok;
}
static GRE_Object4d irregular(void)
{
    /* Open, concave L-shaped patch with varying curvature. */
    GRE_Object4d m=YMGRE_Creat_Object(49,54,"bent L","uv");int f=0;
    for(int y=0;y<7;y++)for(int x=0;x<7;x++)m->pointList[y*7+x].pos=(gre_fvector4d){x,y,.14f*x*x+.11f*y*y,1};
    for(int y=0;y<6;y++)for(int x=0;x<6;x++)if(x<3||y<3) {
        int vertices[6]={y*7+x,y*7+x+1,(y+1)*7+x+1,y*7+x,(y+1)*7+x+1,(y+1)*7+x};
        for(int t=0;t<2;t++) {
            GRE_Polygon4d p=m->polygonList+f++;memset(p,0,sizeof(*p));p->num=3;p->index=GRE_PolyIndex_Malloc(3*sizeof(GRE_Index));
            for(int j=0;j<3;j++)p->index[j]=vertices[t*3+j];
        }
    }
    m->importedUvCount=2;m->importedUvs=calloc(49*4,sizeof(float));
    for(int i=0;i<49;i++){m->importedUvs[i*4+2]=i;m->importedUvs[i*4+3]=-i;}
    return m;
}
/* Cylinder cap perimeter points must remain equidistant from their UV center. */
static float capRadiusRatio(GRE_Object4d mesh,int cap)
{
    float lo=INFINITY,hi=0;
    for(int f=cap;f<mesh->polygonNum;f+=4) {
        GRE_Polygon4d face=mesh->polygonList+f;
        gre_vertex4d center=mesh->pointList[face->index[0]];
        for(int j=1;j<3;j++) {
            gre_vertex4d p=mesh->pointList[face->index[j]];
            float r=hypotf(p.u-center.u,p.v-center.v);lo=fminf(lo,r);hi=fmaxf(hi,r);
        }
    }
    return hi/lo;
}
int main(void)
{
    char error[256];int islands;
    GRErgb24 white={255,255,255};
    GRE_Object4d primitives[8]={
        YMGRE_MeshGener_RectPlane(48,48,3,4,white,"plane","uv"),
        YMGRE_MeshGener_Cube(30,white,"cube","uv"),
        YMGRE_MeshGener_Box(42,24,30,white,"box","uv"),NULL,
        YMGRE_MeshGener_Cylinder(15,32,12,white,"cylinder","uv"),
        YMGRE_MeshGener_Cone(17,34,12,white,"cone","uv"),
        YMGRE_MeshGener_Torus(20,6,12,8,white,"torus","uv"),
        YMGRE_MeshGener_Capsule(12,26,4,12,white,"capsule","uv")};
    gre_fvector4d center={0,0,0,1},axes[3]={{1,0,0,0},{0,1,0,0},{0,0,1,0}};
    for(unsigned shape=0;shape<8;shape++)if(shape!=3) {
        GRE_Object4d m=primitives[shape];CHECK(m);unsigned corners=m->polygonNum*3;
        gre_vertex4d* before=malloc(corners*sizeof(*before));CHECK(before);
        m->importedUvCount=2;m->importedUvs=malloc((size_t)m->pointNum*4*sizeof(float));CHECK(m->importedUvs);
        for(int i=0;i<m->pointNum;i++){m->importedUvs[i*4]=m->pointList[i].u;m->importedUvs[i*4+1]=m->pointList[i].v;m->importedUvs[i*4+2]=m->pointList[i].pos.x*.01f;m->importedUvs[i*4+3]=m->pointList[i].pos.y*.01f;}
        for(unsigned i=0;i<corners;i++)before[i]=m->pointList[m->polygonList[i/3].index[i%3]];
        CHECK(SceneUv_Primitive(m,shape,&center,axes,1,&islands,error,sizeof(error)));
        if(!atlasValid(m)){fprintf(stderr,"Primitive atlas failed: %u\n",shape);return 1;}
        CHECK(islands==((shape==1||shape==2)?6:shape==4?3:shape==5?2:1));
        for(unsigned i=0;i<corners;i++) {
            unsigned k=m->polygonList[i/3].index[i%3];CHECK(!memcmp(&m->pointList[k].pos,&before[i].pos,sizeof(gre_fvector4d)));
            CHECK(m->importedUvCount==2&&fabsf(m->importedUvs[k*4+2]-before[i].pos.x*.01f)<1e-6f&&fabsf(m->importedUvs[k*4+3]-before[i].pos.y*.01f)<1e-6f);
        }
        SceneUvData* first=SceneUvData_Capture(m);CHECK(first);int vertices=m->pointNum;
        CHECK(SceneUv_Primitive(m,shape,&center,axes,1,NULL,error,sizeof(error))&&m->pointNum==vertices);
        SceneUvData* second=SceneUvData_Capture(m);CHECK(second&&first->triangles==second->triangles);
        for(unsigned i=0;i<corners*2;i++)CHECK(fabsf(first->corners[i]-second->corners[i])<1e-6f);
        if(shape==4)CHECK(capRadiusRatio(m,0)<1.001f&&capRadiusRatio(m,1)<1.001f);
        if(shape==7)for(int f=0;f<m->polygonNum;f++) {
            gre_fvector4d a=m->pointList[m->polygonList[f].index[0]].pos,b=m->pointList[m->polygonList[f].index[1]].pos,c=m->pointList[m->polygonList[f].index[2]].pos;
            float x=(b.y-a.y)*(c.z-a.z)-(b.z-a.z)*(c.y-a.y),y=(b.z-a.z)*(c.x-a.x)-(b.x-a.x)*(c.z-a.z),z=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);CHECK(x*x+y*y+z*z>1e-8f);
        }
        free(first);free(second);free(before);YMGRE_Free_Object(m);
    }
    GRE_Object4d cube=YMGRE_MeshGener_Cube(2,(GRErgb24){255,255,255},"cube","uv");
    CHECK(cube&&SceneUv_Generate(cube,&islands,error,sizeof(error)));
    CHECK(islands==6&&cube->pointNum==24&&atlasValid(cube));
    SceneUvEdit* edit=SceneUvEdit_Create(cube);CHECK(edit);
    SceneUvEdit_Select(edit,0,0,0);unsigned selected=0;
    for(unsigned i=0;i<edit->count;i++)selected+=edit->points[i].selected;CHECK(selected==1);
    SceneUvEdit_Select(edit,0,1,0);selected=0;
    float minU=INFINITY,minV=INFINITY,maxU=-INFINITY,maxV=-INFINITY;
    for(unsigned i=0;i<edit->count;i++)if(edit->points[i].selected){selected++;minU=fminf(minU,edit->points[i].u);maxU=fmaxf(maxU,edit->points[i].u);minV=fminf(minV,edit->points[i].v);maxV=fmaxf(maxV,edit->points[i].v);}
    CHECK(selected==4);SceneUvEdit_Start(edit);
    gre_vertex4d* beforeEdit=malloc(cube->pointNum*sizeof(*beforeEdit));CHECK(beforeEdit);memcpy(beforeEdit,cube->pointList,cube->pointNum*sizeof(*beforeEdit));
    CHECK(SceneUvEdit_Transform(edit,.2f,-.1f,90,2,.5f));
    CHECK(!memcmp(beforeEdit,cube->pointList,cube->pointNum*sizeof(*beforeEdit))); /* preview is separate */
    CHECK(!SceneUvEdit_Transform(edit,NAN,0,0,1,1));
    CHECK(SceneUvEdit_Commit(edit)==1&&SceneUvEdit_Commit(edit)==0);
    for(unsigned i=0;i<edit->count;i++) {
        CHECK(!memcmp(&beforeEdit[i].pos,&cube->pointList[i].pos,sizeof(gre_fvector4d)));
        float u=beforeEdit[i].u,v=beforeEdit[i].v;
        if(edit->points[i].selected){float pu=(minU+maxU)*.5f,pv=(minV+maxV)*.5f;u=pu+.2f-(beforeEdit[i].v-pv)*.5f;v=pv-.1f+(beforeEdit[i].u-pu)*2;}
        CHECK(fabsf(cube->pointList[i].u-u)<1e-6f&&fabsf(cube->pointList[i].v-v)<1e-6f);
    }
    free(beforeEdit);SceneUvEdit_Free(edit);
    SceneUvData* snapshot=SceneUvData_Capture(cube);CHECK(snapshot);char* serialized=SceneUvData_Text(snapshot);CHECK(serialized);
    GRE_Vertex4d originalBuffer=cube->pointList;CHECK(SceneUvData_Apply(cube,snapshot)&&cube->pointList==originalBuffer);
    SceneUvData* parsed=SceneUvData_Parse(serialized);CHECK(parsed&&parsed->triangles==snapshot->triangles&&!memcmp(parsed->corners,snapshot->corners,snapshot->triangles*6*sizeof(float)));
    CHECK(!SceneUvData_Parse("1 1 1 nan 0 0 0 0 0")&&!SceneUvData_Parse("1 1 1 0 0 0 0 0 0 garbage")&&!SceneUvData_Parse("1 1 20001 "));
    GRE_Object4d rebuilt=YMGRE_MeshGener_Cube(2,(GRErgb24){255,255,255},"rebuilt","uv");CHECK(rebuilt&&SceneUvData_Apply(rebuilt,parsed));
    SceneUvData* result=SceneUvData_Capture(rebuilt);CHECK(result&&!memcmp(result->corners,snapshot->corners,snapshot->triangles*6*sizeof(float)));
    GRE_Vertex4d buffer=rebuilt->pointList;parsed->counts[0]++;CHECK(!SceneUvData_Apply(rebuilt,parsed)&&rebuilt->pointList==buffer);
    free(snapshot);free(serialized);free(parsed);free(result);YMGRE_Free_Object(rebuilt);
    GRE_Object4d bent=irregular();CHECK(YMGRE_Object_GenerateVertexAttributes(bent));
    gre_fvector4d normals[49];for(int i=0;i<49;i++)normals[i]=bent->pointList_wN[i].normal;
    CHECK(SceneUv_Generate(bent,&islands,error,sizeof(error)));CHECK(islands<54&&atlasValid(bent));
    for(int i=0;i<bent->pointNum;i++) {
        int original=bent->importedUvs[i*4+2];CHECK(original>=0&&original<49&&bent->importedUvs[i*4+3]==-original);
        CHECK(fabsf(bent->pointList_wN[i].normal.x-normals[original].x)<1e-6f);
        CHECK(bent->importedUvs[i*4]==bent->pointList[i].u&&bent->importedUvs[i*4+1]==bent->pointList[i].v);
    }
    printf("Irregular curved L: %d islands / %d triangles\n",islands,bent->polygonNum);
    cube->nextObject=bent;CHECK(SceneUv_Generate(cube,&islands,error,sizeof(error))&&atlasValid(cube));
    /* A failed later submesh must leave all live vertex/index buffers unchanged. */
    GRE_Vertex4d before=cube->pointList;GRE_Index old=bent->polygonList[0].index[1];
    bent->polygonList[0].index[1]=bent->polygonList[0].index[0];
    CHECK(!SceneUv_Generate(cube,NULL,error,sizeof(error))&&cube->pointList==before);
    bent->polygonList[0].index[1]=old;CHECK(atlasValid(cube));
    CHECK(SceneUv_GenerateAtlas(cube,NULL,NULL,&islands,error,sizeof(error))&&atlasValid(cube));
    before=cube->pointList;old=bent->polygonList[0].index[1];bent->polygonList[0].index[1]=bent->pointNum;
    CHECK(!SceneUv_GenerateAtlas(cube,NULL,NULL,&islands,error,sizeof(error))&&cube->pointList==before);
    bent->polygonList[0].index[1]=old;
    for(int i=0;i<bent->pointNum;i++) {
        int original=bent->importedUvs[i*4+2];CHECK(original>=0&&original<49);
        CHECK(fabsf(bent->pointList_wN[i].normal.x-normals[original].x)<1e-6f);
    }
    YMGRE_Free_Object(cube);
    GRE_Object4d sphere=YMGRE_MeshGener_Sphere(2,10,16,(GRErgb24){255,255,255},"sphere","uv");
    CHECK(sphere&&SceneUv_Generate(sphere,&islands,error,sizeof(error))&&atlasValid(sphere));
    CHECK(islands<sphere->polygonNum);YMGRE_Free_Object(sphere);
    GRE_Object4d torus=YMGRE_MeshGener_Torus(3,1,16,10,(GRErgb24){255,255,255},"torus","uv");
    CHECK(torus&&SceneUv_GenerateAtlas(torus,NULL,NULL,&islands,error,sizeof(error))&&atlasValid(torus));
    printf("Torus xatlas: %d islands\n",islands);CHECK(islands<=16);YMGRE_Free_Object(torus);
    GRE_Object4d cylinder=YMGRE_MeshGener_Cylinder(2,6,32,(GRErgb24){255,255,255},"cylinder","uv");
    CHECK(cylinder&&SceneUv_GenerateAtlas(cylinder,NULL,NULL,&islands,error,sizeof(error))&&atlasValid(cylinder));
    printf("Cylinder cap UV radius ratios: %.6f / %.6f\n",capRadiusRatio(cylinder,0),capRadiusRatio(cylinder,1));
    CHECK(capRadiusRatio(cylinder,0)<1.001f&&capRadiusRatio(cylinder,1)<1.001f);YMGRE_Free_Object(cylinder);
    sphere=YMGRE_MeshGener_Sphere(2,20,20,(GRErgb24){255,255,255},"sphere","uv");
    CHECK(sphere&&SceneUv_Sphere(sphere,20,20,error,sizeof(error))&&atlasValid(sphere));
    CHECK(sphere->pointNum==441&&sphere->polygonNum==800);
    for(int y=0;y<=20;y++) {
        gre_vertex4d a=sphere->pointList[y*21],b=sphere->pointList[y*21+20];
        CHECK(a.u==0&&b.u==1&&a.v==(float)y/20&&b.v==a.v);
        CHECK(!memcmp(&a.pos,&b.pos,sizeof(a.pos)));
    }
    CHECK(SceneUv_Sphere(sphere,20,20,error,sizeof(error))&&atlasValid(sphere));
    YMGRE_Free_Object(sphere);
    puts("Automatic UV: cube seams, irregular charts, nonoverlap, multiple submeshes, normals, extra UV channels, transactional failure PASS");
    return 0;
}
