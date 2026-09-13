#include "scene_uv_data.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
static int valid(float f){return isfinite(f)&&fabsf(f)<=1024;}
SceneUvData* SceneUvData_Capture(GRE_Object4d mesh)
{
    unsigned parts=0,triangles=0,counts[64];
    for(GRE_Object4d p=mesh;p;p=p->nextObject) {
        if(parts==64||p->polygonNum<1||p->polygonNum>20000-triangles)return NULL;
        counts[parts++]=p->polygonNum;triangles+=p->polygonNum;
    }
    if(!parts)return NULL;
    SceneUvData* data=malloc(sizeof(*data)+(size_t)triangles*6*sizeof(float));if(!data)return NULL;
    data->parts=parts;data->triangles=triangles;memcpy(data->counts,counts,parts*sizeof(unsigned));unsigned k=0;
    for(GRE_Object4d p=mesh;p;p=p->nextObject)for(int f=0;f<p->polygonNum;f++) {
        GRE_Polygon4d face=p->polygonList+f;if(face->num!=3||!face->index)goto bad;
        for(int j=0;j<3;j++) {
            if(face->index[j]>=p->pointNum)goto bad;gre_vertex4d v=p->pointList[face->index[j]];
            if(!valid(v.u)||!valid(v.v))goto bad;data->corners[k++]=v.u;data->corners[k++]=v.v;
        }
    }
    return data;
bad:free(data);return NULL;
}
static int integer(const char** p,unsigned* value,unsigned max)
{
    while(isspace((unsigned char)**p))(*p)++;
    if(!isdigit((unsigned char)**p))return 0;
    char* end;unsigned long v=strtoul(*p,&end,10);if(end==*p||v>max||!isspace((unsigned char)*end))return 0;
    *p=end;*value=v;return 1;
}
SceneUvData* SceneUvData_Parse(const char* text)
{
    if(!text)return NULL;unsigned version,parts,counts[64],triangles=0;
    if(!integer(&text,&version,1)||version!=1||!integer(&text,&parts,64)||!parts)return NULL;
    for(unsigned i=0;i<parts;i++) {if(!integer(&text,counts+i,20000-triangles)||!counts[i])return NULL;triangles+=counts[i];}
    SceneUvData* data=malloc(sizeof(*data)+(size_t)triangles*6*sizeof(float));if(!data)return NULL;
    data->parts=parts;data->triangles=triangles;memcpy(data->counts,counts,parts*sizeof(unsigned));
    for(unsigned i=0;i<triangles*6;i++) {
        char* end;float value=strtof(text,&end);if(end==text||!valid(value)||(*end&&!isspace((unsigned char)*end))){free(data);return NULL;}
        data->corners[i]=value;text=end;
    }
    while(isspace((unsigned char)*text))text++;if(*text){free(data);return NULL;}return data;
}
char* SceneUvData_Text(const SceneUvData* data)
{
    if(!data||!data->parts||data->parts>64||data->triangles>20000)return NULL;
    size_t capacity=32+data->parts*12+(size_t)data->triangles*6*24;
    char* text=malloc(capacity);if(!text)return NULL;
    size_t used=snprintf(text,capacity,"1 %u ",data->parts);
    for(unsigned i=0;i<data->parts;i++)used+=snprintf(text+used,capacity-used,"%u ",data->counts[i]);
    for(unsigned i=0;i<data->triangles*6;i++) {
        if(!valid(data->corners[i])){free(text);return NULL;}
        used+=snprintf(text+used,capacity-used,"%.9g ",data->corners[i]);
    }
    return text;
}
