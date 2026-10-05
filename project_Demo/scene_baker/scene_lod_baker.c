#include "scene_model_export.h"
#include "YMGRE_LOD.h"
#include "YMGRE_LOD_Simplify.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMCS_File_IO.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

enum { PREVIEW_WIDTH=640, PREVIEW_HEIGHT=480, TIER_COUNT=3 };
static const char *tierName[TIER_COUNT]={"near","middle","far"};

static unsigned triangles(GRE_Object4d mesh)
{
    unsigned total=0;
    for(GRE_Object4d part=mesh;part;part=part->nextObject)total+=(unsigned)part->polygonNum;
    return total;
}
static void meshBounds(GRE_Object4d mesh,gre_fvector4d *lo,gre_fvector4d *hi)
{
    *lo=(gre_fvector4d){FLT_MAX,FLT_MAX,FLT_MAX,1};
    *hi=(gre_fvector4d){-FLT_MAX,-FLT_MAX,-FLT_MAX,1};
    for(GRE_Object4d part=mesh;part;part=part->nextObject)
        for(int i=0;i<part->pointNum;i++){
            gre_fvector4d p=part->pointList[i].pos;
            if(p.x<lo->x)lo->x=p.x;if(p.y<lo->y)lo->y=p.y;if(p.z<lo->z)lo->z=p.z;
            if(p.x>hi->x)hi->x=p.x;if(p.y>hi->y)hi->y=p.y;if(p.z>hi->z)hi->z=p.z;
        }
}
static void freeAssets(gre_scence contexts[TIER_COUNT],GRE_Object4d meshes[TIER_COUNT])
{
    for(int i=0;i<TIER_COUNT;i++){
        YMGRE_Free_Object(meshes[i]);
        YMGRE_List_Clear(&contexts[i].MaterialList,YMGRE_Free_Material);
    }
}
static int pathJoin(char *out,size_t capacity,const char *dir,const char *name)
{return snprintf(out,capacity,"%s/%s",dir,name)<(int)capacity;}
static int exportLOD(const char *root,const char *input[TIER_COUNT],int supplied)
{
    gre_scence contexts[TIER_COUNT]={{0}};
    GRE_Object4d meshes[TIER_COUNT]={0};
    char path[4096],error[256]={0};
    int ok=0;
    if(access(input[0],R_OK)){fprintf(stderr,"Source model is not readable: %s\n",input[0]);goto cleanup;}
    meshes[0]=YMGRE_LoadOgreMeshAndMaterial(&contexts[0],input[0]);
    if(!meshes[0]){fprintf(stderr,"Cannot load source model: %s\n",input[0]);goto cleanup;}
    if(supplied){
        for(int i=1;i<TIER_COUNT;i++){
            if(access(input[i],R_OK)){fprintf(stderr,"Model is not readable: %s\n",input[i]);goto cleanup;}
            meshes[i]=YMGRE_LoadOgreMeshAndMaterial(&contexts[i],input[i]);
            if(!meshes[i]){fprintf(stderr,"Cannot load model: %s\n",input[i]);goto cleanup;}
        }
    }else{
        const float ratios[2]={.80f,.60f},errors[2]={.08f,.25f};
        for(int i=1;i<TIER_COUNT;i++){
            meshes[i]=YMGRE_LOD_SimplifyMesh(meshes[0],
                (YMGRE_LOD_SimplifyOptions){.target_ratio=ratios[i-1],
                .max_relative_error=errors[i-1],.prune_components=1},NULL);
            if(!meshes[i]){
                fprintf(stderr,"Could not simplify source; per-face lightmaps and some mesh types are unsupported\n");
                goto cleanup;
            }
        }
    }
    gre_fvector4d nearLo,nearHi;meshBounds(meshes[0],&nearLo,&nearHi);
    float nearSize=sqrtf((nearHi.x-nearLo.x)*(nearHi.x-nearLo.x)+
                         (nearHi.y-nearLo.y)*(nearHi.y-nearLo.y)+
                         (nearHi.z-nearLo.z)*(nearHi.z-nearLo.z));
    if(!(nearSize>0)){fprintf(stderr,"Source model has no usable bounds\n");goto cleanup;}
    for(int i=1;i<TIER_COUNT;i++){
        gre_fvector4d lo,hi;meshBounds(meshes[i],&lo,&hi);
        float centerShift=sqrtf(powf((lo.x+hi.x-nearLo.x-nearHi.x)*.5f,2)+
                                powf((lo.y+hi.y-nearLo.y-nearHi.y)*.5f,2)+
                                powf((lo.z+hi.z-nearLo.z-nearHi.z)*.5f,2));
        if(centerShift>nearSize*.1f)
            fprintf(stderr,"Warning: %s model center differs from near model by %.2f units; preview may jump\n",
                    tierName[i],centerShift);
    }
    GRE_List materials[TIER_COUNT];
    for(int i=0;i<TIER_COUNT;i++)materials[i]=
        supplied?&contexts[i].MaterialList:&contexts[0].MaterialList;
    if(!SceneModel_ExportLOD(meshes,materials,NULL,NULL,root,120,40,.10f,
                             path,sizeof(path),error,sizeof(error))){
        fprintf(stderr,"LOD export failed: %s\n",error);goto cleanup;
    }
    for(int i=0;i<TIER_COUNT;i++)printf("%s: %u triangles\n",tierName[i],triangles(meshes[i]));
    printf("LOD package: %s\n",path);ok=1;
cleanup:
    freeAssets(contexts,meshes);
    return ok?0:1;
}
static char *readText(const char *path)
{
    FILE *file=fopen(path,"rb");if(!file)return NULL;
    if(fseek(file,0,SEEK_END)){fclose(file);return NULL;}
    long size=ftell(file);if(size<0||size>65536||fseek(file,0,SEEK_SET)){fclose(file);return NULL;}
    char *text=malloc((size_t)size+1);
    if(!text){fclose(file);return NULL;}
    if(fread(text,1,(size_t)size,file)!=(size_t)size){free(text);fclose(file);return NULL;}
    text[size]=0;fclose(file);return text;
}
static int saveFrame(GRE_Camera4d camera,const char *path)
{
    GRE_RenderTarget target=YMGRE_Camera_GetRenderTarget(camera);
    GRErgb24 *pixels=malloc((size_t)PREVIEW_WIDTH*PREVIEW_HEIGHT*sizeof(*pixels));
    if(!target||!pixels){free(pixels);return 0;}
    unsigned drawn=0;
    for(size_t i=0;i<(size_t)PREVIEW_WIDTH*PREVIEW_HEIGHT;i++){
        pixels[i]=GRE_FramePixel_To_RGB24(target->data[i]);
        drawn+=pixels[i].R!=50||pixels[i].G!=50||pixels[i].B!=50;
    }
    if(!drawn)fprintf(stderr,"LOD preview rendered no object pixels\n");
    if(drawn)YMGRE_Image_LoadTo_Bmp_File(path,pixels,PREVIEW_WIDTH,PREVIEW_HEIGHT);
    free(pixels);
    return drawn>0&&access(path,R_OK)==0;
}
static int previewLOD(const char *lodPath,const char *prefix)
{
    char *descriptionText=readText(lodPath);
    YMGRE_LOD_Object lod;
    gre_scence contexts[TIER_COUNT]={{0}};
    GRE_Object4d meshes[TIER_COUNT]={0},active=NULL;
    GRE_Camera4d camera=NULL;GRE_RenderWorkspace workspace=NULL;
    gre_listnode node={sizeof(gre_object4d),NULL,NULL};gre_list objects={&node,1};
    gre_list lights={0};int ok=0,switched=0;
    if(!descriptionText||!YMGRE_LOD_Parse(&lod,descriptionText)||lod.count!=TIER_COUNT){
        fprintf(stderr,"Expected a valid three-level .lod file\n");goto cleanup;
    }
    char directory[4096];
    const char *slash=strrchr(lodPath,'/');size_t length=slash?(size_t)(slash-lodPath):1;
    if(length>=sizeof(directory))goto cleanup;
    if(slash){memcpy(directory,lodPath,length);directory[length]=0;}else strcpy(directory,".");
    for(int i=0;i<TIER_COUNT;i++){
        char path[4096];
        if(lod.levels[i].kind!=YMGRE_LOD_MESH||
           !pathJoin(path,sizeof(path),directory,lod.levels[i].asset))goto cleanup;
        meshes[i]=YMGRE_LoadOgreMeshAndMaterial(&contexts[i],path);
        if(!meshes[i]){fprintf(stderr,"Cannot load LOD resource: %s\n",path);goto cleanup;}
        lod.levels[i].resource=meshes[i];
    }
    gre_fvector4d lo,hi;meshBounds(meshes[0],&lo,&hi);
    gre_fvector4d center={(lo.x+hi.x)*.5f,(lo.y+hi.y)*.5f,(lo.z+hi.z)*.5f,1};
    float dx=hi.x-lo.x,dy=hi.y-lo.y,dz=hi.z-lo.z;
    float radius=.5f*sqrtf(dx*dx+dy*dy+dz*dz);
    if(!(radius>0)){fprintf(stderr,"The near mesh has no usable bounds\n");goto cleanup;}
    camera=YMGRE_Creat_Camera(0,PREVIEW_WIDTH,PREVIEW_HEIGHT,45,45,22,22);
    workspace=YMGRE_Creat_RenderWorkspace();
    if(!camera||!workspace){fprintf(stderr,"Cannot create preview camera/workspace\n");goto cleanup;}
    float planeHeight=camera->perspectPlane.pU-camera->perspectPlane.pD;
    float depthScale=2*radius*camera->perspectPlane.Dis*PREVIEW_HEIGHT/planeHeight;
    YMGRE_Camera_Frustum_Init(camera,.01f,depthScale/8+radius*4);
    YMGRE_List_Append(&lights,sizeof(gre_light4d),
        YMGRE_Creat_Light(0,GRE_GlobalLight,(GRErgb24){255,255,255},.9f));
    float nearPixels=lod.levels[0].min_pixels*1.5f;
    float farPixels=fmaxf(1.0f,lod.levels[TIER_COUNT-2].min_pixels*.25f);
    YMGRE_LOD_Instance state={0};
    for(size_t frame=0;frame<=32;frame++){
        float fraction=(float)(frame<=16?frame:32-frame)/16.0f;
        float desiredPixels=nearPixels*powf(farPixels/nearPixels,fraction);
        float depth=depthScale/desiredPixels;
        gre_fvector4d eye={center.x+depth*.98f,center.y+depth*.14f,
                            center.z-depth*.14f,1};
        YMGRE_UVNCamera_PositionInit(camera,&eye,&center,NULL,0);
        uint8 previous=state.level,initialized=state.initialized;
        const YMGRE_LOD_Level *level=YMGRE_LOD_SelectCamera(&lod,&state,camera,center,radius);
        if(!level){fprintf(stderr,"Cannot select LOD frame %zu\n",frame);goto cleanup;}
        clock_t started=clock();
        int rebuilt=!active||!initialized||previous!=state.level;
        if(rebuilt){
            GRE_Object4d replacement=YMGRE_Object_Clone((GRE_Object4d)level->resource);
            if(!replacement){fprintf(stderr,"Cannot clone LOD frame %zu\n",frame);goto cleanup;}
            int prepared=1;
            for(GRE_Object4d part=replacement;part;part=part->nextObject){
                part->WorldCoordinate=center;
                part->boundType=GRE_Bounding_Sphere_R;
                part->BoundingSphereR=0;
                for(int i=0;i<part->pointNum;i++){
                    gre_fvector4d p=part->pointList[i].pos;
                    float dx=p.x-center.x,dy=p.y-center.y,dz=p.z-center.z;
                    float distance=sqrtf(dx*dx+dy*dy+dz*dz);
                    if(distance>part->BoundingSphereR)part->BoundingSphereR=distance;
                }
                if(!YMGRE_Object_GenerateVertexAttributes(part)){prepared=0;break;}
            }
            if(!prepared){YMGRE_Free_Object(replacement);goto cleanup;}
            YMGRE_Free_Object(active);active=replacement;
            if(initialized)switched++;
        }
        clock_t prepared=clock();
        node.data=active;
        YMGRE_Camera_TanglePipline_wN(camera,&lights,&objects,
            &contexts[state.level].MaterialList,workspace);
        char path[4096];
        if(snprintf(path,sizeof(path),"%s_%02zu.bmp",prefix,frame)>=(int)sizeof(path)||
           !saveFrame(camera,path)){
            fprintf(stderr,"Cannot save LOD preview frame %zu\n",frame);goto cleanup;
        }
        printf("frame %zu: projected %.1f px, tier %u (%u triangles), rebuilt %d, clone CPU %.3f ms, %s\n",
               frame,YMGRE_LOD_ProjectedDiameter(camera,center,radius),state.level,
               triangles(active),rebuilt,1000.0*(prepared-started)/CLOCKS_PER_SEC,path);
    }
    ok=switched>=2;
cleanup:
    free(descriptionText);YMGRE_Free_Object(active);
    YMGRE_Free_RenderWorkspace(workspace);YMGRE_Free_Camera(camera);
    YMGRE_List_Clear(&lights,YMGRE_Free_Light);
    freeAssets(contexts,meshes);
    return ok?0:1;
}
int main(int argc,char **argv)
{
    if((argc==4||argc==6)&&!strcmp(argv[1],"export")){
        const char *input[TIER_COUNT]={argv[3],argc==6?argv[4]:NULL,argc==6?argv[5]:NULL};
        return exportLOD(argv[2],input,argc==6);
    }
    if(argc==4&&!strcmp(argv[1],"preview"))return previewLOD(argv[2],argv[3]);
    fprintf(stderr,"Usage:\n  %s export OUTPUT_ROOT NEAR.mesh [MIDDLE.mesh FAR.mesh]\n"
                   "  %s preview PACKAGE/object.lod OUTPUT_PREFIX\n",argv[0],argv[0]);
    return 2;
}
