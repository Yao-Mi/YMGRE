#include "YMGRE_LOD.h"
#include "YMGRE_LOD_Simplify.h"
#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_Material.h"
#include "YMGRE_ScenceManager.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMCS_File_IO.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WIDTH 1200
#define HEIGHT 750
#define SCENE_GRASS 1600
#define SCENE_TREES 24
#define SCENE_OBJECTS (1+SCENE_GRASS+SCENE_TREES+1)

static uint32 triangles(GRE_Object4d object)
{
    uint32 count=0;
    for(GRE_Object4d part=object;part;part=part->nextObject)count+=part->polygonNum;
    return count;
}
static GRE_Object4d loadVegetation(gre_scence *scene,const char *category,const char *name)
{
    char path[1024];
    snprintf(path,sizeof(path),"%s/vegetation_lowpoly/%s/%s/%s.mesh",
             RESOURCE_ROOT,category,name,name);
    return YMGRE_LoadOgreMeshAndMaterial(scene,path);
}
static void bounds(GRE_Object4d source,gre_fvector4d *lo,gre_fvector4d *hi)
{
    *lo=(gre_fvector4d){FLT_MAX,FLT_MAX,FLT_MAX,1};
    *hi=(gre_fvector4d){-FLT_MAX,-FLT_MAX,-FLT_MAX,1};
    for(GRE_Object4d part=source;part;part=part->nextObject)
        for(int i=0;i<part->pointNum;i++){
            gre_fvector4d p=part->pointList[i].pos;
            if(p.x<lo->x)lo->x=p.x;
            if(p.y<lo->y)lo->y=p.y;
            if(p.z<lo->z)lo->z=p.z;
            if(p.x>hi->x)hi->x=p.x;
            if(p.y>hi->y)hi->y=p.y;
            if(p.z>hi->z)hi->z=p.z;
        }
}
static GRE_Object4d placeYaw(GRE_Object4d source,float x,float y,float z,
                             float desiredHeight,float yaw)
{
    gre_fvector4d lo,hi;bounds(source,&lo,&hi);
    float height=fmaxf(hi.y-lo.y,.001f),scale=desiredHeight/height;
    float cx=(lo.x+hi.x)*.5f,cz=(lo.z+hi.z)*.5f;
    float ca=cosf(yaw),sa=sinf(yaw);
    GRE_Object4d object=YMGRE_Object_Clone(source);
    if(!object)return NULL;
    for(GRE_Object4d part=object;part;part=part->nextObject){
        part->WorldCoordinate=(gre_fvector4d){x,y,z,1};
        part->boundType=GRE_Bounding_Sphere_R;part->BoundingSphereR=0;
        for(int i=0;i<part->pointNum;i++){
            gre_fvector4d p=part->pointList[i].pos;
            float localX=(p.x-cx)*scale,localZ=(p.z-cz)*scale;
            p.x=x+localX*ca-localZ*sa;p.y=y+(p.y-lo.y)*scale;
            p.z=z+localX*sa+localZ*ca;
            part->pointList[i].pos=p;
            if(part->importedNormals){
                gre_fvector4d n=part->importedNormals[i];
                part->importedNormals[i].x=n.x*ca-n.z*sa;
                part->importedNormals[i].z=n.x*sa+n.z*ca;
            }
            float dx=p.x-x,dy=p.y-y,dz=p.z-z;
            float r=sqrtf(dx*dx+dy*dy+dz*dz);
            if(r>part->BoundingSphereR)part->BoundingSphereR=r;
        }
        if(!YMGRE_Object_GenerateVertexAttributes(part)){
            YMGRE_Free_Object(object);return NULL;
        }
    }
    return object;
}
static GRE_Object4d place(GRE_Object4d source,float x,float y,float z,float desiredHeight)
{
    return placeYaw(source,x,y,z,desiredHeight,0);
}
static int addRow(gre_scence *scene,GRE_Object4d original,
                  gre_listnode *nodes,GRE_Object4d *placed,int row,float y)
{
    GRE_Object4d medium=NULL,far=NULL;
    YMGRE_LOD_SimplifyResult mediumStats={0},farStats={0};
    float midRatio=row==0?.80f:row==1?.85f:.65f;
    float farRatio=row==0?.60f:row==1?.70f:.35f;
    medium=YMGRE_LOD_SimplifyMesh(original,
        (YMGRE_LOD_SimplifyOptions){.target_ratio=midRatio,.max_relative_error=.08f,.prune_components=1},&mediumStats);
    far=YMGRE_LOD_SimplifyMesh(original,
        (YMGRE_LOD_SimplifyOptions){.target_ratio=farRatio,.max_relative_error=.25f,.prune_components=1},&farStats);
    if(!medium||!far){YMGRE_Free_Object(medium);YMGRE_Free_Object(far);return 0;}
    if(!(triangles(original)>triangles(medium)&&triangles(medium)>triangles(far))){
        YMGRE_Free_Object(medium);YMGRE_Free_Object(far);return 0;
    }
    YMGRE_LOD_Object lod;YMGRE_LOD_Init(&lod,.10f);
    if(!YMGRE_LOD_Add(&lod,YMGRE_LOD_MESH,90,NULL,original)||
       !YMGRE_LOD_Add(&lod,YMGRE_LOD_MESH,40,NULL,medium)||
       !YMGRE_LOD_Add(&lod,YMGRE_LOD_MESH,0,NULL,far)){
        YMGRE_Free_Object(medium);YMGRE_Free_Object(far);return 0;
    }
    const float samplePixels[3]={120,60,15};
    const float positions[3]={-6.5f,0,6.5f};
    for(int col=0;col<3;col++){
        YMGRE_LOD_Instance state={0};
        const YMGRE_LOD_Level *level=YMGRE_LOD_Select(&lod,&state,samplePixels[col]);
        if(!level||level->resource!=(col==0?(void*)original:col==1?(void*)medium:(void*)far)){
            YMGRE_Free_Object(medium);YMGRE_Free_Object(far);return 0;
        }
        GRE_Object4d p=place((GRE_Object4d)level->resource,positions[col],y,9,3);
        if(!p){YMGRE_Free_Object(medium);YMGRE_Free_Object(far);return 0;}
        if(row==2)for(GRE_Object4d part=p;part;part=part->nextObject)part->wireFrame=1;
        placed[row*3+col]=p;
        nodes[row*3+col]=(gre_listnode){sizeof(gre_object4d),p,
            row*3+col<8?&nodes[row*3+col+1]:NULL};
    }
    printf("row %d: source %u, medium %u, far %u triangles; QEM %u/%u, pieces %u/%u\n",
           row,triangles(original),mediumStats.output_triangles,farStats.output_triangles,
           mediumStats.collapsed_triangles,farStats.collapsed_triangles,
           mediumStats.pruned_triangles,farStats.pruned_triangles);
    YMGRE_Free_Object(medium);YMGRE_Free_Object(far);
    (void)scene;
    return 1;
}
static float random01(void)
{
    static unsigned int state=0x1a2b3c4du;
    state=state*1664525u+1013904223u;
    return (float)(state>>8)*(1.0f/16777216.0f);
}
static int writeFrame(GRE_Camera4d camera,const char *output)
{
    GRE_RenderTarget frame=YMGRE_Camera_GetRenderTarget(camera);
    GRErgb24 *pixels=malloc((size_t)WIDTH*HEIGHT*sizeof(*pixels));
    if(!pixels)return 0;
    size_t drawn=0;
    for(size_t i=1;i<(size_t)WIDTH*HEIGHT;i++)
        drawn+=memcmp(&frame->data[i],&frame->data[0],sizeof(GRE_FramePixel))!=0;
    if(drawn<1000){free(pixels);return 0;}
    for(size_t i=0;i<(size_t)WIDTH*HEIGHT;i++)pixels[i]=GRE_FramePixel_To_RGB24(frame->data[i]);
    YMGRE_Image_LoadTo_Bmp_File(output,pixels,WIDTH,HEIGHT);
    free(pixels);
    FILE *check=fopen(output,"rb");
    if(!check)return 0;
    unsigned char magic[2];size_t read=fread(magic,1,2,check);fclose(check);
    return read==2&&magic[0]=='B'&&magic[1]=='M';
}
static int appendPlaced(gre_listnode *nodes,GRE_Object4d *placed,int *count,
                        GRE_Object4d source,float x,float y,float z,float height)
{
    if(*count>=SCENE_OBJECTS)return 0;
    GRE_Object4d object=place(source,x,y,z,height);
    if(!object)return 0;
    int i=(*count)++;
    placed[i]=object;
    nodes[i]=(gre_listnode){sizeof(gre_object4d),object,NULL};
    if(i)nodes[i-1].next=&nodes[i];
    return 1;
}
static int appendPlacedYaw(gre_listnode *nodes,GRE_Object4d *placed,int *count,
                           GRE_Object4d source,float x,float y,float z,float height,float yaw)
{
    if(*count>=SCENE_OBJECTS)return 0;
    GRE_Object4d object=placeYaw(source,x,y,z,height,yaw);
    if(!object)return 0;
    int i=(*count)++;
    placed[i]=object;
    nodes[i]=(gre_listnode){sizeof(gre_object4d),object,NULL};
    if(i)nodes[i-1].next=&nodes[i];
    return 1;
}
static int initThreeLevels(YMGRE_LOD_Object *lod,GRE_Object4d levels[3],
                           float nearPixels,float midPixels,float midRatio,float farRatio)
{
    YMGRE_LOD_Init(lod,.10f);
    levels[1]=YMGRE_LOD_SimplifyMesh(levels[0],
        (YMGRE_LOD_SimplifyOptions){.target_ratio=midRatio,.max_relative_error=.08f,.prune_components=1},NULL);
    levels[2]=YMGRE_LOD_SimplifyMesh(levels[0],
        (YMGRE_LOD_SimplifyOptions){.target_ratio=farRatio,.max_relative_error=.25f,.prune_components=1},NULL);
    if(!levels[1]||!levels[2]||
       !(triangles(levels[0])>triangles(levels[1])&&triangles(levels[1])>triangles(levels[2])))return 0;
    return YMGRE_LOD_Add(lod,YMGRE_LOD_MESH,nearPixels,NULL,levels[0])&&
           YMGRE_LOD_Add(lod,YMGRE_LOD_MESH,midPixels,NULL,levels[1])&&
           YMGRE_LOD_Add(lod,YMGRE_LOD_MESH,0,NULL,levels[2]);
}
typedef struct {
    const YMGRE_LOD_Object *resource;
    YMGRE_LOD_Instance lod;
    GRE_Object4d mesh;
    float x,y,z,height,yaw,radius;
    int visible;
} basic_instance;
static int updateBasicInstance(basic_instance *instance,GRE_Camera4d camera,int *changed)
{
    *changed=0;
    if(!instance->visible)return 1;
    gre_fvector4d center={instance->x,instance->y+instance->height*.5f,instance->z,1};
    YMGRE_LOD_Instance previous=instance->lod;
    const YMGRE_LOD_Level *level=YMGRE_LOD_SelectCamera(instance->resource,
        &instance->lod,camera,center,instance->radius);
    if(!level||level->kind!=YMGRE_LOD_MESH||!level->resource){instance->lod=previous;return 0;}
    if(instance->mesh&&previous.initialized&&previous.level==instance->lod.level)return 1;
    GRE_Object4d replacement=placeYaw((GRE_Object4d)level->resource,
        instance->x,instance->y,instance->z,instance->height,instance->yaw);
    if(!replacement){instance->lod=previous;return 0;}
    YMGRE_Free_Object(instance->mesh);
    instance->mesh=replacement;
    *changed=1;
    return 1;
}
static int buildSceneColorMips(gre_scence *scene)
{
    int count=0;
    for(GRE_ListNode node=scene->MaterialList.listhead;node;node=node->next){
        GRE_Material material=node->data;
        if(material&&material->pixel&&material->width&&material->height){
            if(!YMGRE_Material_BuildColorMips(material))return -1;
            count++;
        }
    }
    return count;
}
static int basicView(const char *prefix,int colorMips)
{
    gre_scence scene={0};GRE_Object4d levels[2][3]={{0}};
    YMGRE_LOD_Object lod[2];basic_instance instances[4]={{0}};
    gre_listnode nodes[4]={{0}};gre_list objects={nodes,4};
    GRE_Camera4d camera=NULL;GRE_RenderWorkspace workspace=NULL;
    const float cameraZ[]={26,20,5,-15,-45,-15,5,20,26};
    int ok=0,switches[4]={0},mipCount=0;
    levels[0][0]=loadVegetation(&scene,"Grasses","grass_green_low");
    levels[1][0]=loadVegetation(&scene,"trees","oak_01");
    if(!levels[0][0]||!levels[1][0])goto cleanup;
    if(!initThreeLevels(&lod[0],levels[0],150,60,.80f,.60f)||
       !initThreeLevels(&lod[1],levels[1],290,155,.85f,.70f))goto cleanup;
    if(colorMips){
        mipCount=buildSceneColorMips(&scene);
        if(mipCount<=0)goto cleanup;
    }
    camera=YMGRE_Creat_Camera(0,WIDTH,HEIGHT,45,45,22,22);
    workspace=YMGRE_Creat_RenderWorkspace();
    if(!camera||!workspace)goto cleanup;
    YMGRE_Camera_Frustum_Init(camera,.1f,140);
    instances[0]=(basic_instance){.resource=&lod[0],.x=-2.7f,.z=35,.height=1.6f,
        .yaw=1.5707963f,.radius=1.6f*.68f,.visible=1};
    instances[1]=(basic_instance){.resource=&lod[0],.x=-4.5f,.z=43,.height=1.3f,
        .yaw=.8f,.radius=1.3f*.68f,.visible=1};
    instances[2]=(basic_instance){.resource=&lod[1],.x=3.5f,.z=35,.height=5.6f,
        .radius=5.6f*.63f,.visible=1};
    instances[3]=(basic_instance){.resource=&lod[1],.x=5.5f,.z=43,.height=4.2f,
        .radius=4.2f*.63f,.visible=1};
    for(int i=0;i<4;i++){
        nodes[i]=(gre_listnode){sizeof(gre_object4d),NULL,i==3?NULL:&nodes[i+1]};
    }
    YMGRE_List_Append(&scene.LightList,sizeof(gre_light4d),
        YMGRE_Creat_Light(0,GRE_GlobalLight,(GRErgb24){255,255,255},.85f));
    for(size_t frame=0;frame<sizeof(cameraZ)/sizeof(cameraZ[0]);frame++){
        gre_fvector4d eye={0,2.4f,cameraZ[frame],1};
        gre_fvector4d target={0,2.0f,35,1};
        YMGRE_UVNCamera_PositionInit(camera,&eye,&target,NULL,0);
        int changed[4]={0};
        clock_t start=clock();
        for(int i=0;i<4;i++){
            if(!updateBasicInstance(&instances[i],camera,&changed[i]))goto cleanup;
            nodes[i].data=instances[i].mesh;
            if(frame&&changed[i])switches[i]++;
        }
        clock_t prepared=clock();
        YMGRE_Camera_TanglePipline_wN(camera,&scene.LightList,&objects,&scene.MaterialList,workspace);
        clock_t rendered=clock();
        char path[1024];
        if(snprintf(path,sizeof(path),"%s_%02zu.bmp",prefix,frame)>=(int)sizeof(path)||
           !writeFrame(camera,path))goto cleanup;
        printf("frame %zu: camera z=%g, grass LOD %u/%u, tree LOD %u/%u; "
               "rebuilt %d/%d/%d/%d; switch CPU %.3f ms, render CPU %.3f ms; %s\n",
               frame,cameraZ[frame],instances[0].lod.level,instances[1].lod.level,
               instances[2].lod.level,instances[3].lod.level,
               changed[0],changed[1],changed[2],changed[3],
               1000.0*(prepared-start)/CLOCKS_PER_SEC,
               1000.0*(rendered-prepared)/CLOCKS_PER_SEC,path);
    }
    for(int i=0;i<4;i++)if(!switches[i])goto cleanup;
    printf("basic demo: color Mips %s on %d materials; grass switches %d/%d, tree switches %d/%d\n",
           colorMips?"enabled":"disabled",mipCount,
           switches[0],switches[1],switches[2],switches[3]);
    ok=1;
cleanup:
    YMGRE_Free_RenderWorkspace(workspace);YMGRE_Free_Camera(camera);
    for(int i=0;i<4;i++)YMGRE_Free_Object(instances[i].mesh);
    for(int i=0;i<2;i++){
        for(int tier=0;tier<3;tier++)YMGRE_Free_Object(levels[i][tier]);
    }
    YMGRE_List_Clear(&scene.LightList,YMGRE_Free_Light);
    YMGRE_List_Clear(&scene.MaterialList,YMGRE_Free_Material);
    return ok?0:1;
}
static int sceneView(const char *output,int forceOriginal)
{
    gre_scence scene={0};GRE_Object4d levels[3][3]={{0}},placed[SCENE_OBJECTS]={0};
    gre_listnode nodes[SCENE_OBJECTS]={0};GRE_Camera4d camera=NULL;
    GRE_RenderWorkspace workspace=NULL;int count=0,ok=0,selected[3][3]={{0}};
    unsigned long long chosenTriangles=0,originalTriangles=0;
    YMGRE_LOD_Object lod[3];
    levels[0][0]=loadVegetation(&scene,"Grasses","grass_green_low");
    levels[1][0]=loadVegetation(&scene,"trees","oak_01");
    levels[2][0]=YMGRE_MeshGener_Sphere(1,30,40,(GRErgb24){65,133,201},"earth","lod-earth");
    if(!levels[0][0]||!levels[1][0]||!levels[2][0])goto cleanup;
    GRE_Material groundMat=YMGRE_Creat_Material("lod-ground");
    GRE_Material earthMat=YMGRE_Creat_Material("lod-earth");
    if(!groundMat||!earthMat)goto cleanup;
    groundMat->diffuse=(GRErgb24){78,112,49};
    groundMat->ambient=groundMat->diffuse;
    groundMat->doubleSided=1;
    earthMat->diffuse=(GRErgb24){65,133,201};
    earthMat->ambient=earthMat->diffuse;
    YMGRE_List_Append(&scene.MaterialList,sizeof(gre_material),groundMat);
    YMGRE_List_Append(&scene.MaterialList,sizeof(gre_material),earthMat);
    if(!initThreeLevels(&lod[0],levels[0],110,43,.80f,.60f)||
       !initThreeLevels(&lod[1],levels[1],290,155,.85f,.70f)||
       !initThreeLevels(&lod[2],levels[2],300,145,.65f,.35f))goto cleanup;
    camera=YMGRE_Creat_Camera(0,WIDTH,HEIGHT,45,45,22,22);
    workspace=YMGRE_Creat_RenderWorkspace();
    if(!camera||!workspace)goto cleanup;
    YMGRE_Camera_Frustum_Init(camera,.1f,110);
    gre_fvector4d eye={0,2.25f,-8,1},target={0,1.2f,34,1};
    YMGRE_UVNCamera_PositionInit(camera,&eye,&target,NULL,0);
    GRE_Object4d ground=YMGRE_MeshGener_RectPlane(65,90,1,1,(GRErgb24){78,112,49},"ground","lod-ground");
    if(!ground)goto cleanup;
    for(int i=0;i<ground->pointNum;i++)ground->pointList[i].pos.z+=30;
    ground->WorldCoordinate=(gre_fvector4d){0,0,30,1};
    if(!YMGRE_Object_GenerateVertexAttributes(ground)){YMGRE_Free_Object(ground);goto cleanup;}
    placed[count]=ground;
    nodes[count++]=(gre_listnode){sizeof(gre_object4d),ground,NULL};
    for(int i=0;i<SCENE_GRASS;i++){
        float z=-3+58*random01(),x=(random01()-.5f)*25;
        float height=.85f+random01()*.4f;
        gre_fvector4d center={x,height*.5f,z,1};
        YMGRE_LOD_Instance state={0};
        const YMGRE_LOD_Level *choice=YMGRE_LOD_SelectCamera(&lod[0],&state,camera,center,height*.68f);
        if(!choice||!appendPlaced(nodes,placed,&count,
            forceOriginal?levels[0][0]:(GRE_Object4d)choice->resource,x,0,z,height))goto cleanup;
        selected[0][state.level]++;
        chosenTriangles+=triangles((GRE_Object4d)choice->resource);
        originalTriangles+=triangles(levels[0][0]);
    }
    for(int i=0;i<SCENE_TREES;i++){
        float z=8+(float)(i/2)*4.3f+random01()*.9f;
        float x=(i&1?1:-1)*(5.4f+random01()*3.5f),height=5.2f+random01()*1.2f;
        gre_fvector4d center={x,height*.5f,z,1};
        YMGRE_LOD_Instance state={0};
        const YMGRE_LOD_Level *choice=YMGRE_LOD_SelectCamera(&lod[1],&state,camera,center,height*.63f);
        if(!choice||!appendPlaced(nodes,placed,&count,
            forceOriginal?levels[1][0]:(GRE_Object4d)choice->resource,x,0,z,height))goto cleanup;
        selected[1][state.level]++;
        chosenTriangles+=triangles((GRE_Object4d)choice->resource);
        originalTriangles+=triangles(levels[1][0]);
    }
    {
        float x=0,z=25,height=3.1f;
        gre_fvector4d center={x,3.7f,z,1};
        YMGRE_LOD_Instance state={0};
        const YMGRE_LOD_Level *choice=YMGRE_LOD_SelectCamera(&lod[2],&state,camera,center,height*.55f);
        if(!choice||!appendPlaced(nodes,placed,&count,
            forceOriginal?levels[2][0]:(GRE_Object4d)choice->resource,x,2.1f,z,height))goto cleanup;
        selected[2][state.level]++;
        chosenTriangles+=triangles((GRE_Object4d)choice->resource);
        originalTriangles+=triangles(levels[2][0]);
    }
    for(int kind=0;kind<2;kind++)for(int tier=0;tier<3;tier++)if(!selected[kind][tier])goto cleanup;
    printf("scene: grass near/mid/far %d/%d/%d; trees %d/%d/%d; globe %d/%d/%d\n",
           selected[0][0],selected[0][1],selected[0][2],
           selected[1][0],selected[1][1],selected[1][2],
           selected[2][0],selected[2][1],selected[2][2]);
    printf("scene mesh triangles: selected %llu, all-original %llu (%.1f%%); rendering %s\n",
           chosenTriangles,originalTriangles,100.0*(double)chosenTriangles/(double)originalTriangles,
           forceOriginal?"all original":"LOD");
    YMGRE_List_Append(&scene.LightList,sizeof(gre_light4d),
        YMGRE_Creat_Light(0,GRE_GlobalLight,(GRErgb24){255,255,255},.85f));
    gre_list objects={nodes,(uint32)count};
    YMGRE_Camera_TanglePipline_wN(camera,&scene.LightList,&objects,&scene.MaterialList,workspace);
    ok=writeFrame(camera,output);
    if(ok)printf("wrote %s\n",output);
cleanup:
    YMGRE_Free_RenderWorkspace(workspace);YMGRE_Free_Camera(camera);
    for(int i=0;i<count;i++)YMGRE_Free_Object(placed[i]);
    for(int kind=0;kind<3;kind++)for(int tier=0;tier<3;tier++)YMGRE_Free_Object(levels[kind][tier]);
    YMGRE_List_Clear(&scene.LightList,YMGRE_Free_Light);
    YMGRE_List_Clear(&scene.MaterialList,YMGRE_Free_Material);
    return ok?0:1;
}
static int distanceView(const char *output,int tree,int single,float singleZ)
{
    gre_scence scene={0};GRE_Object4d levels[3]={0},placed[4]={0};
    gre_listnode nodes[4]={0};GRE_Camera4d camera=NULL;
    GRE_RenderWorkspace workspace=NULL;YMGRE_LOD_Object lod;
    int ok=0,count=0;
    levels[0]=loadVegetation(&scene,tree?"trees":"Grasses",tree?"oak_01":"grass_green_low");
    if(!levels[0])goto cleanup;
    if(!initThreeLevels(&lod,levels,tree?290:150,tree?155:60,
                        tree?.85f:.80f,tree?.70f:.60f))goto cleanup;
    GRE_Material groundMat=YMGRE_Creat_Material("lod-ground");
    if(!groundMat)goto cleanup;
    groundMat->ambient=groundMat->diffuse=(GRErgb24){83,118,52};
    groundMat->doubleSided=1;
    YMGRE_List_Append(&scene.MaterialList,sizeof(gre_material),groundMat);
    camera=YMGRE_Creat_Camera(0,WIDTH,HEIGHT,45,45,22,22);
    workspace=YMGRE_Creat_RenderWorkspace();
    if(!camera||!workspace)goto cleanup;
    YMGRE_Camera_Frustum_Init(camera,.1f,120);
    gre_fvector4d eye={0,tree?3.0f:1.5f,-8,1};
    gre_fvector4d target={0,tree?2.0f:.75f,36,1};
    YMGRE_UVNCamera_PositionInit(camera,&eye,&target,NULL,0);
    GRE_Object4d ground=YMGRE_MeshGener_RectPlane(90,100,1,1,
        (GRErgb24){83,118,52},"ground","lod-ground");
    if(!ground)goto cleanup;
    for(int i=0;i<ground->pointNum;i++)ground->pointList[i].pos.z+=32;
    ground->WorldCoordinate=(gre_fvector4d){0,0,32,1};
    if(!YMGRE_Object_GenerateVertexAttributes(ground)){YMGRE_Free_Object(ground);goto cleanup;}
    placed[count]=ground;nodes[count++]=(gre_listnode){sizeof(gre_object4d),ground,NULL};
    const float grassX[3]={-2.1f,.9f,6.5f},grassZ[3]={-2,17,47};
    const float treeX[3]={-5.7f,2.1f,14.5f},treeZ[3]={4,25,54};
    const float *xs=tree?treeX:grassX,*zs=tree?treeZ:grassZ;
    float height=tree?5.6f:1.6f,radius=height*(tree?.63f:.68f);
    for(int i=0;i<(single?1:3);i++){
        float x=single?0:xs[i],z=single?singleZ:zs[i];
        gre_fvector4d center={x,height*.5f,z,1};
        YMGRE_LOD_Instance state={0};
        float pixels=YMGRE_LOD_ProjectedDiameter(camera,center,radius);
        const YMGRE_LOD_Level *choice=YMGRE_LOD_SelectCamera(&lod,&state,camera,center,radius);
        if(!choice||(!single&&state.level!=i)||
           !appendPlacedYaw(nodes,placed,&count,(GRE_Object4d)choice->resource,
                            x,0,z,height,tree?0:1.5707963f))goto cleanup;
        printf("%s %s: depth z=%.1f, projected %.1f px, tier %d, %u triangles\n",
               tree?"tree":"grass",state.level==0?"near":state.level==1?"mid":"far",
               z,pixels,state.level,triangles((GRE_Object4d)choice->resource));
    }
    YMGRE_List_Append(&scene.LightList,sizeof(gre_light4d),
        YMGRE_Creat_Light(0,GRE_GlobalLight,(GRErgb24){255,255,255},.85f));
    gre_list objects={nodes,(uint32)count};
    YMGRE_Camera_TanglePipline_wN(camera,&scene.LightList,&objects,&scene.MaterialList,workspace);
    ok=writeFrame(camera,output);
    if(ok)printf("wrote %s\n",output);
cleanup:
    YMGRE_Free_RenderWorkspace(workspace);YMGRE_Free_Camera(camera);
    for(int i=0;i<count;i++)YMGRE_Free_Object(placed[i]);
    for(int i=0;i<3;i++)YMGRE_Free_Object(levels[i]);
    YMGRE_List_Clear(&scene.LightList,YMGRE_Free_Light);
    YMGRE_List_Clear(&scene.MaterialList,YMGRE_Free_Material);
    return ok?0:1;
}
int main(int argc,char **argv)
{
    if(argc>=2&&!strcmp(argv[1],"--basic"))
        return basicView(argc>2?argv[2]:"lod_basic",1);
    if(argc>=2&&!strcmp(argv[1],"--basic-no-mip"))
        return basicView(argc>2?argv[2]:"lod_basic_no_mip",0);
    if(argc>=2&&!strcmp(argv[1],"--distance-grass"))
        return distanceView(argc>2?argv[2]:"lod_distance_grass.bmp",0,0,0);
    if(argc>=2&&!strcmp(argv[1],"--distance-tree"))
        return distanceView(argc>2?argv[2]:"lod_distance_tree.bmp",1,0,0);
    if(argc==4&&(!strcmp(argv[1],"--frame-grass")||!strcmp(argv[1],"--frame-tree")))
        return distanceView(argv[2],!strcmp(argv[1],"--frame-tree"),1,(float)atof(argv[3]));
    if(argc>=2&&!strcmp(argv[1],"--scene"))return sceneView(argc>2?argv[2]:"lod_mesh_scene.bmp",0);
    if(argc>=2&&!strcmp(argv[1],"--scene-original"))return sceneView(argc>2?argv[2]:"lod_mesh_scene_original.bmp",1);
    const char *output=argc>1?argv[1]:"lod_mesh_preview.bmp";
    gre_scence scene={0};GRE_Object4d sources[3]={0},placed[9]={0};
    gre_listnode nodes[9]={0};GRE_Camera4d camera=NULL;
    GRE_RenderWorkspace workspace=NULL;GRErgb24 *pixels=NULL;int ok=0;
    sources[0]=loadVegetation(&scene,"Grasses","grass_green_low");
    sources[1]=loadVegetation(&scene,"trees","oak_01");
    sources[2]=YMGRE_MeshGener_Sphere(1,30,40,(GRErgb24){255,255,255},"earth","lod-earth");
    if(!sources[0]||!sources[1]||!sources[2])goto cleanup;
    GRE_Material earth=YMGRE_Creat_Material("lod-earth");
    earth->diffuse=(GRErgb24){70,155,220};
    YMGRE_List_Append(&scene.MaterialList,sizeof(gre_material),earth);
    for(int row=0;row<3;row++)if(!addRow(&scene,sources[row],nodes,placed,row,5-row*4))goto cleanup;
    gre_list objects={nodes,9};
    GRE_Light4d ambient=YMGRE_Creat_Light(0,GRE_GlobalLight,(GRErgb24){255,255,255},.85f);
    YMGRE_List_Append(&scene.LightList,sizeof(gre_light4d),ambient);
    camera=YMGRE_Creat_Camera(0,WIDTH,HEIGHT,45,45,22,22);
    workspace=YMGRE_Creat_RenderWorkspace();
    if(!camera||!workspace)goto cleanup;
    YMGRE_Camera_Frustum_Init(camera,.1f,100);
    gre_fvector4d eye={0,6,-10,1},target={0,2,9,1};
    YMGRE_UVNCamera_PositionInit(camera,&eye,&target,NULL,0);
    YMGRE_Camera_TanglePipline_wN(camera,&scene.LightList,&objects,&scene.MaterialList,workspace);
    pixels=malloc((size_t)WIDTH*HEIGHT*sizeof(*pixels));if(!pixels)goto cleanup;
    GRE_RenderTarget frame=YMGRE_Camera_GetRenderTarget(camera);
    size_t drawn=0;
    for(size_t i=1;i<(size_t)WIDTH*HEIGHT;i++)
        drawn+=memcmp(&frame->data[i],&frame->data[0],sizeof(GRE_FramePixel))!=0;
    if(drawn<1000)goto cleanup;
    for(size_t i=0;i<(size_t)WIDTH*HEIGHT;i++)pixels[i]=GRE_FramePixel_To_RGB24(frame->data[i]);
    YMGRE_Image_LoadTo_Bmp_File(output,pixels,WIDTH,HEIGHT);
    FILE *check=fopen(output,"rb");
    if(!check)goto cleanup;
    unsigned char magic[2];size_t read=fread(magic,1,2,check);fclose(check);
    if(read!=2||magic[0]!='B'||magic[1]!='M')goto cleanup;
    printf("wrote %s\n",output);ok=1;
cleanup:
    free(pixels);YMGRE_Free_RenderWorkspace(workspace);YMGRE_Free_Camera(camera);
    for(int i=0;i<9;i++)YMGRE_Free_Object(placed[i]);
    for(int i=0;i<3;i++)YMGRE_Free_Object(sources[i]);
    YMGRE_List_Clear(&scene.LightList,YMGRE_Free_Light);
    YMGRE_List_Clear(&scene.MaterialList,YMGRE_Free_Material);
    return ok?0:1;
}
