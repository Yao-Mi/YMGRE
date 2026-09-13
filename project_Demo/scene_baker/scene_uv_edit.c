#include "scene_uv_edit.h"
#include "YMGRE_Creat.h"
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
typedef struct {unsigned point,part;float x,y,z,u,v;} Weld;
static unsigned root(unsigned* p,unsigned i){while(p[i]!=i){p[i]=p[p[i]];i=p[i];}return i;}
static void join(unsigned* p,unsigned a,unsigned b){a=root(p,a);b=root(p,b);if(a!=b)p[b]=a;}
static int compare(const void* ap,const void* bp)
{
    const Weld *a=ap,*b=bp;if(a->part!=b->part)return a->part<b->part?-1:1;
#define CMP(k) if(a->k!=b->k)return a->k<b->k?-1:1
    CMP(x);CMP(y);CMP(z);CMP(u);CMP(v);
#undef CMP
    return 0;
}
void SceneUvEdit_Free(SceneUvEdit* edit){if(edit){free(edit->points);free(edit);}}
SceneUvEdit* SceneUvEdit_Create(GRE_Object4d mesh)
{
    unsigned count=0;
    for(GRE_Object4d m=mesh;m;m=m->nextObject) {
        if(m->pointNum<1||m->pointNum>200000-count||!m->pointList||!m->polygonList||m->polygonNum<1||m->importedUvCount>8||
           (m->importedUvs&&!m->importedUvCount)||!YMGRE_Object_GenerateVertexAttributes(m))return NULL;count+=m->pointNum;
    }
    if(!count)return NULL;
    SceneUvEdit* e=calloc(1,sizeof(*e));Weld* w=malloc(count*sizeof(*w));
    unsigned* vertices=malloc(count*sizeof(unsigned)),*islands=malloc(count*sizeof(unsigned));
    if(!e||!w||!vertices||!islands)goto bad;
    e->count=count;e->points=calloc(count,sizeof(*e->points));if(!e->points)goto bad;
    unsigned n=0,part=0;
    for(GRE_Object4d m=mesh;m;m=m->nextObject,part++)for(int i=0;i<m->pointNum;i++,n++) {
        gre_vertex4d p=m->pointList[i];
        if(!isfinite(p.u)||!isfinite(p.v)||!isfinite(p.pos.x)||!isfinite(p.pos.y)||!isfinite(p.pos.z))goto bad;
        e->points[n]=(SceneUvEditPoint){m,i,0,0,p.u,p.v,p.u,p.v,0};
        w[n]=(Weld){n,part,p.pos.x,p.pos.y,p.pos.z,p.u,p.v};vertices[n]=islands[n]=n;
    }
    qsort(w,count,sizeof(*w),compare);
    for(unsigned i=1;i<count;i++)if(!compare(w+i-1,w+i))join(vertices,w[i-1].point,w[i].point);
    for(unsigned i=0;i<count;i++){e->points[i].group=root(vertices,i);islands[i]=e->points[i].group;}
    n=0;
    for(GRE_Object4d m=mesh;m;m=m->nextObject) {
        for(int f=0;f<m->polygonNum;f++) {
            GRE_Polygon4d p=m->polygonList+f;if(p->num!=3||!p->index)goto bad;
            for(int j=0;j<3;j++)if(p->index[j]>=m->pointNum)goto bad;
            join(islands,n+p->index[0],n+p->index[1]);join(islands,n+p->index[0],n+p->index[2]);
        }
        n+=m->pointNum;
    }
    for(unsigned i=0;i<count;i++)e->points[i].island=root(islands,i);
    free(w);free(vertices);free(islands);return e;
bad:free(w);free(vertices);free(islands);SceneUvEdit_Free(e);return NULL;
}
void SceneUvEdit_Select(SceneUvEdit* e,unsigned point,int island,int add)
{
    if(!e||point>=e->count)return;
    unsigned group=island?e->points[point].island:e->points[point].group;
    int selected=add?!e->points[point].selected:1;
    for(unsigned i=0;i<e->count;i++) {
        if(!add)e->points[i].selected=0;
        if((island?e->points[i].island:e->points[i].group)==group)e->points[i].selected=selected;
    }
}
void SceneUvEdit_Start(SceneUvEdit* e)
{if(e)for(unsigned i=0;i<e->count;i++){e->points[i].startU=e->points[i].u;e->points[i].startV=e->points[i].v;}}
int SceneUvEdit_Transform(SceneUvEdit* e,float du,float dv,float degrees,float su,float sv)
{
    if(!e||!isfinite(du)||!isfinite(dv)||!isfinite(degrees)||!isfinite(su)||!isfinite(sv)||fabsf(su)<.0001f||fabsf(sv)<.0001f||fabsf(su)>1000||fabsf(sv)>1000)return 0;
    float loU=INFINITY,loV=INFINITY,hiU=-INFINITY,hiV=-INFINITY;int found=0;
    for(unsigned i=0;i<e->count;i++)if(e->points[i].selected) {
        SceneUvEditPoint p=e->points[i];loU=fminf(loU,p.startU);loV=fminf(loV,p.startV);hiU=fmaxf(hiU,p.startU);hiV=fmaxf(hiV,p.startV);found=1;
    }
    if(!found)return 0;
    float pu=(loU+hiU)*.5f,pv=(loV+hiV)*.5f,c=cosf(degrees*.01745329251994f),s=sinf(degrees*.01745329251994f);
    for(int pass=0;pass<2;pass++)for(unsigned i=0;i<e->count;i++)if(e->points[i].selected) {
        SceneUvEditPoint* p=e->points+i;float u=(p->startU-pu)*su,v=(p->startV-pv)*sv;
        float outU=pu+du+u*c-v*s,outV=pv+dv+u*s+v*c;
        if(!isfinite(outU)||!isfinite(outV)||fabsf(outU)>1024||fabsf(outV)>1024)return 0;
        if(pass){p->u=outU;p->v=outV;}
    }
    return 1;
}
int SceneUvEdit_Commit(SceneUvEdit* e)
{
    if(!e)return 0;int changed=0;
    for(unsigned i=0;i<e->count;i++) {
        SceneUvEditPoint p=e->points[i];if(!isfinite(p.u)||!isfinite(p.v)||fabsf(p.u)>1024||fabsf(p.v)>1024)return -1;
        gre_vertex4d v=p.mesh->pointList[p.index];if(v.u!=p.u||v.v!=p.v)changed=1;
    }
    if(!changed)return 0;
    GRE_Object4d previous=NULL;
    for(unsigned i=0;i<e->count;i++) {
        SceneUvEditPoint p=e->points[i];p.mesh->pointList[p.index].u=p.u;p.mesh->pointList[p.index].v=p.v;
        if(p.mesh->importedUvs){size_t k=(size_t)p.index*p.mesh->importedUvCount*2;p.mesh->importedUvs[k]=p.u;p.mesh->importedUvs[k+1]=p.v;}
        if(previous&&previous!=p.mesh)YMGRE_Object_GenerateVertexAttributes(previous);previous=p.mesh;
    }
    if(previous)YMGRE_Object_GenerateVertexAttributes(previous);return 1;
}
void SceneUvEdit_Cancel(SceneUvEdit* e)
{if(e)for(unsigned i=0;i<e->count;i++){SceneUvEditPoint* p=e->points+i;p->u=p->mesh->pointList[p->index].u;p->v=p->mesh->pointList[p->index].v;}}
