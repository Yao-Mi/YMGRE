#include "scene_lod_image.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMCS_File_IO.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <float.h>

static int writeU32(FILE *file,uint32_t value)
{
    unsigned char bytes[4]={(unsigned char)value,(unsigned char)(value>>8),
        (unsigned char)(value>>16),(unsigned char)(value>>24)};
    return fwrite(bytes,1,4,file)==4;
}
static int readU32(FILE *file,uint32_t *value)
{
    unsigned char bytes[4];if(fread(bytes,1,4,file)!=4)return 0;
    *value=(uint32_t)bytes[0]|(uint32_t)bytes[1]<<8|
        (uint32_t)bytes[2]<<16|(uint32_t)bytes[3]<<24;return 1;
}
static int writeF32(FILE *file,float value)
{uint32_t bits;memcpy(&bits,&value,4);return writeU32(file,bits);}
static int readF32(FILE *file,float *value)
{uint32_t bits;if(!readU32(file,&bits))return 0;memcpy(value,&bits,4);return 1;}

int SceneLodImage_Bake(grass_impostor *image,GRE_Object4d source,
    GRE_List materials,GRE_List lights)
{
    if(!image||!source)return 0;
    GRE_Object4d copy=YMGRE_Object_Clone(source);
    if(!copy)return 0;
    gre_fvector4d lo={FLT_MAX,FLT_MAX,FLT_MAX,1},hi={-FLT_MAX,-FLT_MAX,-FLT_MAX,1};
    for(GRE_Object4d part=copy;part;part=part->nextObject)
        for(int i=0;i<part->pointNum;i++){
            gre_fvector4d p=part->pointList[i].pos;
            lo.x=fminf(lo.x,p.x);lo.y=fminf(lo.y,p.y);lo.z=fminf(lo.z,p.z);
            hi.x=fmaxf(hi.x,p.x);hi.y=fmaxf(hi.y,p.y);hi.z=fmaxf(hi.z,p.z);
        }
    if(lo.x==FLT_MAX){YMGRE_Free_Object(copy);return 0;}
    gre_fvector4d center={(lo.x+hi.x)*.5f,(lo.y+hi.y)*.5f,(lo.z+hi.z)*.5f,1};
    for(GRE_Object4d part=copy;part;part=part->nextObject){
        part->WorldCoordinate=center;part->boundType=GRE_Bounding_Sphere_R;
        part->BoundingSphereR=0;
        for(int i=0;i<part->pointNum;i++){
            gre_fvector4d p=part->pointList[i].pos;
            float distance=sqrtf((p.x-center.x)*(p.x-center.x)+
                (p.y-center.y)*(p.y-center.y)+(p.z-center.z)*(p.z-center.z));
            if(distance>part->BoundingSphereR)part->BoundingSphereR=distance;
        }
    }
    int ok=grass_impostor_bake(image,copy,materials,lights);
    YMGRE_Free_Object(copy);
    return ok;
}

int SceneLodImage_Save(const grass_impostor *image,const char *directory,
    const gre_fvector4d *origin,char *error,size_t errorCapacity)
{
    if(!image||!directory){snprintf(error,errorCapacity,"八方向图资源无效");return 0;}
    char path[4096];
    if(snprintf(path,sizeof(path),"%s/views8.bin",directory)>=(int)sizeof(path)){
        snprintf(error,errorCapacity,"八方向图路径过长");return 0;
    }
    FILE *file=fopen(path,"wb");
    if(!file){snprintf(error,errorCapacity,"无法写入八方向图");return 0;}
    float header[4]={image->center.x-(origin?origin->x:0),
        image->center.y-(origin?origin->y:0),image->center.z-(origin?origin->z:0),image->span};
    int ok=fwrite("YMGRE8V1",1,8,file)==8;
    for(int i=0;i<4&&ok;i++)ok=writeF32(file,header[i]);
    for(int view=0;view<GRASS_VIEWS&&ok;view++){
        const grass_view *tile=&image->views[view];
        int bounds[4]={tile->left,tile->top,tile->right,tile->bottom};
        for(int i=0;i<4&&ok;i++)ok=writeU32(file,(uint32_t)bounds[i]);
        for(int i=0;i<GRASS_TILE*GRASS_TILE&&ok;i++){
            unsigned char rgb[3]={tile->color[i].R,tile->color[i].G,tile->color[i].B};
            ok=fwrite(rgb,1,3,file)==3;
        }
        if(ok)ok=fwrite(tile->mask,1,GRASS_TILE*GRASS_TILE,file)==GRASS_TILE*GRASS_TILE;
    }
    if(fclose(file))ok=0;
    if(!ok){snprintf(error,errorCapacity,"八方向图数据写入失败");return 0;}
    GRErgb24 mask[GRASS_TILE*GRASS_TILE];
    for(int view=0;view<GRASS_VIEWS;view++){
        if(snprintf(path,sizeof(path),"%s/view_%d.bmp",directory,view)>=(int)sizeof(path))return 0;
        YMGRE_Image_LoadTo_Bmp_File(path,(GRErgb24*)image->views[view].color,GRASS_TILE,GRASS_TILE);
        if(access(path,R_OK)){snprintf(error,errorCapacity,"八方向颜色图写入失败");return 0;}
        for(int i=0;i<GRASS_TILE*GRASS_TILE;i++){
            uint8_t a=image->views[view].mask[i]?255:0;mask[i]=(GRErgb24){a,a,a};
        }
        if(snprintf(path,sizeof(path),"%s/mask_%d.bmp",directory,view)>=(int)sizeof(path))return 0;
        YMGRE_Image_LoadTo_Bmp_File(path,mask,GRASS_TILE,GRASS_TILE);
        if(access(path,R_OK)){snprintf(error,errorCapacity,"八方向透明遮罩写入失败");return 0;}
    }
    return 1;
}

int SceneLodImage_Load(grass_impostor *image,const char *path)
{
    if(!image||!path)return 0;
    FILE *file=fopen(path,"rb");if(!file)return 0;
    char magic[8];float header[4];
    int ok=fread(magic,1,8,file)==8&&!memcmp(magic,"YMGRE8V1",8);
    for(int i=0;i<4&&ok;i++)ok=readF32(file,&header[i]);
    if(ok)ok=isfinite(header[0])&&isfinite(header[1])&&isfinite(header[2])&&
        isfinite(header[3])&&header[3]>0;
    if(ok){image->center=(gre_fvector4d){header[0],header[1],header[2],1};image->span=header[3];}
    for(int view=0;view<GRASS_VIEWS&&ok;view++){
        grass_view *tile=&image->views[view];uint32_t bounds[4];
        for(int i=0;i<4&&ok;i++)ok=readU32(file,&bounds[i]);
        for(int i=0;i<GRASS_TILE*GRASS_TILE&&ok;i++){
            unsigned char rgb[3];ok=fread(rgb,1,3,file)==3;
            if(ok)tile->color[i]=(GRErgb24){rgb[0],rgb[1],rgb[2]};
        }
        if(ok)ok=fread(tile->mask,1,GRASS_TILE*GRASS_TILE,file)==GRASS_TILE*GRASS_TILE;
        if(ok){tile->left=(int32_t)bounds[0];tile->top=(int32_t)bounds[1];
            tile->right=(int32_t)bounds[2];tile->bottom=(int32_t)bounds[3];
            ok=tile->left>=0&&tile->left<=GRASS_TILE&&tile->top>=0&&tile->top<=GRASS_TILE&&
                tile->right>=-1&&tile->right<GRASS_TILE&&tile->bottom>=-1&&tile->bottom<GRASS_TILE;
        }
    }
    fclose(file);return ok;
}
