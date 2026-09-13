#include "scene_editor_texture.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_ScenceManager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
static void put32(unsigned char* p,unsigned v){for(int i=0;i<4;i++)p[i]=v>>(i*8);}
static unsigned get32(const unsigned char* p){return (unsigned)p[0]|((unsigned)p[1]<<8)|((unsigned)p[2]<<16)|((unsigned)p[3]<<24);}
static int directories(const char* path)
{
    char p[4096];if(!path||strlen(path)>=sizeof(p))return 0;strcpy(p,path);
    for(char* c=p+1;*c;c++)if(*c=='/'){*c=0;int ok=mkdir(p,0755)==0||errno==EEXIST;*c='/';if(!ok)return 0;}
    return mkdir(p,0755)==0||errno==EEXIST;
}
int SceneEditorTexture_Save(const GRErgb24* pixels,unsigned width,unsigned height,const char* root,char* path,size_t capacity,char* error,size_t errorCapacity)
{
    char dir[4096],file[4096];FILE* f=NULL;int ok=0;
    if(!pixels||!width||!height||width>4096||height>4096||!directories(root)||
       snprintf(dir,sizeof(dir),"%s/texture-XXXXXX",root)>=(int)sizeof(dir)||!mkdtemp(dir))goto bad;
    if(snprintf(file,sizeof(file),"%s/texture.bmp",dir)>=(int)sizeof(file))goto bad;
    unsigned stride=(width*3+3)&~3u;unsigned char header[54]={0},padding[3]={0};header[0]='B';header[1]='M';
    put32(header+2,54+stride*height);put32(header+10,54);put32(header+14,40);put32(header+18,width);put32(header+22,height);header[26]=1;header[28]=24;put32(header+34,stride*height);
    f=fopen(file,"wb");if(!f)goto bad;ok=fwrite(header,1,54,f)==54;
    for(unsigned y=height;y>0&&ok;y--) {
        for(unsigned x=0;x<width&&ok;x++){GRErgb24 c=pixels[(y-1)*width+x];unsigned char bgr[3]={c.B,c.G,c.R};ok=fwrite(bgr,1,3,f)==3;}
        if(stride>width*3)ok=ok&&fwrite(padding,1,stride-width*3,f)==stride-width*3;
    }
    if(fclose(f))ok=0;f=NULL;if(!ok){unlink(file);goto bad;}
    char absolute[4096];if(!realpath(file,absolute)||strlen(absolute)>=capacity){unlink(file);goto bad;}
    strcpy(path,absolute);return 1;
bad:
    if(f)fclose(f);snprintf(error,errorCapacity,"无法保存应用的贴图，请检查输出目录");return 0;
}
int SceneEditorTexture_Load(SceneEditorObject* object,GRE_List materials,const char* path,char* error,size_t capacity)
{
    static unsigned serial;GRErgb24* pixels=NULL;GRE_Material material=NULL;char* names[64]={0};GRE_Object4d parts[64];unsigned count=0;
    if(!object||!object->mesh||!materials||!path||strlen(path)>=sizeof(object->texturePath))goto bad;
    unsigned char header[54];FILE* f=fopen(path,"rb");if(!f)goto bad;
    int valid=fread(header,1,54,f)==54;
    if(!valid||header[0]!='B'||header[1]!='M'||get32(header+14)!=40||get32(header+10)!=54||
       header[26]!=1||header[27]||header[28]!=24||header[29]||get32(header+30)||
       !get32(header+18)||get32(header+18)>4096||!get32(header+22)||get32(header+22)>4096){fclose(f);goto bad;}
    unsigned width=get32(header+18),height=get32(header+22),stride=(width*3+3)&~3u;
    pixels=GRE_ImageBuff_Malloc(width*height*sizeof(*pixels));if(!pixels){fclose(f);goto bad;}
    unsigned char row[4096*3];
    for(unsigned y=height;y>0;y--) {
        if(fread(row,1,stride,f)!=stride){fclose(f);goto bad;}
        for(unsigned x=0;x<width;x++)pixels[(y-1)*width+x]=(GRErgb24){row[x*3+2],row[x*3+1],row[x*3]};
    }
    fclose(f);
    char name[64];do{snprintf(name,sizeof(name),"scene_uv_texture_%u",++serial);}while(YMGRE_Material_Find(materials,name));
    material=YMGRE_Creat_Material(name);if(!material)goto bad;
    material->pixel=pixels;pixels=NULL;material->width=width;material->height=height;
    material->ambient=material->diffuse=(GRErgb24){object->color>>16,object->color>>8,object->color};material->specular=(GRErgb24){255,255,255};
    for(GRE_Object4d p=object->mesh;p;p=p->nextObject) {
        if(count==64)goto bad;parts[count]=p;names[count]=GRE_malloc1(strlen(name)+1);if(!names[count])goto bad;
        strcpy(names[count],name);count++;
    }
    /* Allocate the list node explicitly so a failed allocation cannot partially apply. */
    GRE_ListNode node=GRE_malloc0(sizeof(*node));if(!node)goto bad;*node=(gre_listnode){sizeof(*material),material,NULL};
    GRE_ListNode* tail=&materials->listhead;while(*tail)tail=&(*tail)->next;*tail=node;materials->len++;material=NULL;
    for(unsigned i=0;i<count;i++){GRE_free1(parts[i]->materiaName);parts[i]->materiaName=names[i];names[i]=NULL;}
    memmove(object->texturePath,path,strlen(path)+1);return 1;
bad:
    for(unsigned i=0;i<64;i++)GRE_free1(names[i]);GRE_ImageBuff_Free(pixels);YMGRE_Free_Material(material);
    snprintf(error,capacity,"无法读取应用贴图：%.150s",path?path:"");return 0;
}
