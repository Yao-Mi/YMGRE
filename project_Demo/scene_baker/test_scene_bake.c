#include "scene_bake.h"
#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_Coordinates_Transform.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_TriangleRaster.h"
#include "YMGRE_CullingAndClipping.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#define CHECK(c) do {if(!(c)){fprintf(stderr,"bake test line %d: %s\n",__LINE__,#c);return 1;}}while(0)
int main(void)
{
    char error[256],path[4096],root[]="/tmp/ymgre-bake-test-XXXXXX";
    CHECK(mkdtemp(root));
    GRE_Object4d cube=YMGRE_MeshGener_Cube(2,(GRErgb24){255,255,255},"cube","");
    CHECK(cube);
    for(int i=0;i<cube->pointNum;i++) CHECK(cube->pointList[i].u==0 && cube->pointList[i].v==0);
    YMGRE_Object_GenerateVertexAttributes(cube);
    gre_list lights={0};
    GRE_Light4d ambient=YMGRE_Creat_Light(0,GRE_GlobalLight,(GRErgb24){255,255,255},0.2f);
    GRE_Light4d point=YMGRE_Creat_Light(1,GRE_PointLight,(GRErgb24){255,255,255},0.8f);
    point->pos=(gre_fvector4d){-3,4,-5,1};point->proper.kc0=1;point->proper.kc1=point->proper.kc2=0;
    YMGRE_List_Append(&lights,sizeof(gre_light4d),ambient);YMGRE_List_Append(&lights,sizeof(gre_light4d),point);
    gre_vertex4d* original=malloc(cube->pointNum*sizeof(*original));CHECK(original);
    memcpy(original,cube->pointList,cube->pointNum*sizeof(*original));
    GRE_Lightmap map=SceneBake_Create(cube,&lights,256,error,sizeof(error));CHECK(map);
    CHECK(!memcmp(original,cube->pointList,cube->pointNum*sizeof(*original)));free(original);
    CHECK(map->triangleCount==12);
    int minimum=255,maximum=0;
    for(unsigned i=0;i<map->triangleCount;i++) {
        float* uv=&map->uv1[i*6];CHECK(uv[2]>uv[0] && uv[5]>uv[1]);
        for(unsigned j=0;j<i;j++) {
            float* v=&map->uv1[j*6];
            CHECK(uv[2]<v[0] || v[2]<uv[0] || uv[5]<v[1] || v[5]<uv[1]);
        }
        int x=(uv[0]+uv[2]+uv[4])/3*256,y=(uv[1]+uv[3]+uv[5])/3*256;
        int value=map->pixels[y*256+x].R;if(value<minimum)minimum=value;if(value>maximum)maximum=value;
    }
    CHECK(minimum>=50 && maximum-minimum>60);
    uint64_t hash=SceneBake_Fingerprint(cube,&lights);
    CHECK(SceneBake_Save(map,hash,root,path,sizeof(path),error,sizeof(error)));
    GRE_Lightmap loaded=SceneBake_Load(path,cube,&lights,error,sizeof(error));CHECK(loaded);
    CHECK(!memcmp(map->uv1,loaded->uv1,12*6*sizeof(float)));
    CHECK(!memcmp(map->pixels,loaded->pixels,256*256*sizeof(GRErgb24)));YMGRE_Free_Lightmap(loaded);
    CHECK(strstr(path,".material")!=NULL);
    /* The BMP is authoritative: changing it changes the loaded lightmap. */
    char imagePath[4096],missingPath[4096];strcpy(imagePath,path);
    strcpy(strrchr(imagePath,'/')+1,"lightmap.bmp");
    FILE* bitmap=fopen(imagePath,"r+b");CHECK(bitmap);CHECK(fseek(bitmap,54,SEEK_SET)==0);
    int oldBlue=fgetc(bitmap);CHECK(oldBlue!=EOF);CHECK(fseek(bitmap,54,SEEK_SET)==0);fputc(oldBlue^127,bitmap);fclose(bitmap);
    loaded=SceneBake_Load(path,cube,&lights,error,sizeof(error));CHECK(loaded);
    CHECK(loaded->pixels[255*256].B==(oldBlue^127));YMGRE_Free_Lightmap(loaded);
    bitmap=fopen(imagePath,"r+b");CHECK(bitmap);CHECK(fseek(bitmap,54,SEEK_SET)==0);fputc(oldBlue,bitmap);fclose(bitmap);
    strcpy(missingPath,imagePath);strcpy(strrchr(missingPath,'/')+1,"missing.bmp");
    CHECK(rename(imagePath,missingPath)==0);CHECK(!SceneBake_Load(path,cube,&lights,error,sizeof(error)));
    CHECK(rename(missingPath,imagePath)==0);
    /* Preserve the source material color and a padded, non-square albedo BMP. */
    GRErgb24 basePixels[6]={{7,31,63},{101,103,107},{109,113,127},{131,137,139},{149,151,157},{163,167,173}};
    gre_material base={.ambient={65,99,130},.diffuse={205,165,105},.width=2,.height=3,.pixel=basePixels};
    CHECK(SceneBake_SaveMaterial(map,hash,&base,root,path,sizeof(path),error,sizeof(error)));
    GRE_Material restored=NULL;
    loaded=SceneBake_LoadMaterial(path,cube,&lights,&restored,error,sizeof(error));CHECK(loaded && restored);
    CHECK(restored->width==2 && restored->height==3 && !memcmp(restored->pixel,basePixels,sizeof(basePixels)));
    CHECK(!memcmp(&restored->diffuse,&base.diffuse,sizeof(GRErgb24)) && !memcmp(&restored->ambient,&base.ambient,sizeof(GRErgb24)));
    YMGRE_Free_Lightmap(loaded);YMGRE_Free_Material(restored);
    /* Old scene references still load their original YLM resource. */
    char legacyPath[4096];snprintf(legacyPath,sizeof(legacyPath),"%s/legacy.ylm",root);
    FILE* legacy=fopen(legacyPath,"wb");CHECK(legacy);
    fprintf(legacy,"YMGRE_LIGHTMAP 1\n256 256 12 %" PRIu64 "\n",hash);
    for(int i=0;i<72;i++)fprintf(legacy,"%.9g%c",map->uv1[i],i%6==5?'\n':' ');
    fputs("RGB\n",legacy);for(int i=0;i<256*256;i++){fputc(map->pixels[i].R,legacy);fputc(map->pixels[i].G,legacy);fputc(map->pixels[i].B,legacy);}fclose(legacy);
    loaded=SceneBake_Load(legacyPath,cube,&lights,error,sizeof(error));CHECK(loaded);
    CHECK(!memcmp(loaded->pixels,map->pixels,256*256*sizeof(GRErgb24)));YMGRE_Free_Lightmap(loaded);
    point->proper.strength+=0.1f;
    CHECK(!SceneBake_Load(path,cube,&lights,error,sizeof(error)));point->proper.strength-=0.1f;
    cube->pointList[0].pos.x+=1;CHECK(!SceneBake_Load(path,cube,&lights,error,sizeof(error)));cube->pointList[0].pos.x-=1;
    char blocked[4096];snprintf(blocked,sizeof(blocked),"%s/blocked",root);FILE* f=fopen(blocked,"w");CHECK(f);fclose(f);
    CHECK(!SceneBake_Save(map,hash,blocked,path,sizeof(path),error,sizeof(error)));
    /* Separate UV0 and UV1, perspective correct interpolation, no repeated light. */
    GRE_Camera4d camera=YMGRE_Creat_Camera(0,32,32,45,45,45,45);CHECK(camera);
    YMGRE_Camera_Frustum_Init(camera,0.1f,100);
    gre_vertex4d_wN vertices[3]={0};
    vertices[0].base=(gre_vertex4d){{2,2,1,1},0.1f,0.1f};
    vertices[1].base=(gre_vertex4d){{28,2,2,1},0.1f,0.1f};
    vertices[2].base=(gre_vertex4d){{2,28,4,1},0.1f,0.1f};
    for(int i=0;i<3;i++){vertices[i].color=(GRErgb24){255,255,255};vertices[i].normal.z=-1;vertices[i].lightmapV=0.1f;}
    vertices[0].lightmapU=0.1f;vertices[1].lightmapU=0.9f;vertices[2].lightmapU=0.1f;
    GRErgb24 texels[2]={{128,255,255},{255,0,0}},irradiance[2]={{64,64,64},{192,192,192}};
    gre_material material={0};material.diffuse=(GRErgb24){128,128,128};material.width=2;material.height=1;material.pixel=texels;
    gre_lightmap tiny={.width=2,.height=1,.pixels=irradiance,.enabled=1};GRE_Index indices[3]={0,1,2};
    gre_polygon4d triangle={.num=3,.index=indices};
    YMGRE_CameraImage_Init(camera,(GRErgb24){0,0,0});
    YMGRE_TriangleRaster_FillLightmap_wN(vertices,&triangle,&material,&tiny,camera);
    GRErgb24 sample=GRE_FramePixel_To_RGB24(camera->img.data[4*32+18]);
    CHECK(abs(sample.R-16)<=3 && abs(sample.G-32)<=3 && abs(sample.B-32)<=3);
    /* UV1 must survive near-plane clipping independently of UV0. */
    camera->perspectPlane.kl=-1;camera->perspectPlane.kr=1;camera->perspectPlane.kd=-1;camera->perspectPlane.ku=1;
    camera->frustum.Znear=1;
    vertices[0].base.pos=(gre_fvector4d){0,0,0.5f,1};vertices[1].base.pos=(gre_fvector4d){1,0,3,1};vertices[2].base.pos=(gre_fvector4d){0,1,3,1};
    for(int i=0;i<3;i++){vertices[i].lightmapU=vertices[i].base.pos.z/4;vertices[i].lightmapV=vertices[i].base.pos.x/2;}
    gre_vertex4d_wN clipped[YMGRE_FRUSTUM_CLIP_VERTEX_MAX];
    unsigned count=YMGRE_Polygon_FrustumClip_wN(vertices,3,clipped,YMGRE_FRUSTUM_CLIP_VERTEX_MAX,camera);CHECK(count==4);
    for(unsigned i=0;i<count;i++) CHECK(fabsf(clipped[i].lightmapU-clipped[i].base.pos.z/4)<1e-6f && fabsf(clipped[i].lightmapV-clipped[i].base.pos.x/2)<1e-6f);
    /* Black output with no light is valid; zero-area faces are rejected. */
    gre_list empty={0};GRE_Lightmap black=SceneBake_Create(cube,&empty,32,error,sizeof(error));CHECK(black);
    for(unsigned i=0;i<32*32;i++)CHECK(black->pixels[i].R==0 && black->pixels[i].G==0 && black->pixels[i].B==0);
    YMGRE_Free_Lightmap(black);
    GRE_Index old=cube->polygonList[0].index[1];cube->polygonList[0].index[1]=cube->polygonList[0].index[0];
    CHECK(!SceneBake_Create(cube,&lights,256,error,sizeof(error)));cube->polygonList[0].index[1]=old;
    /* A truncated resource is rejected without attaching partial state. */
    CHECK(SceneBake_Save(map,hash,root,path,sizeof(path),error,sizeof(error)));
    CHECK(truncate(path,48)==0);CHECK(!SceneBake_Load(path,cube,&lights,error,sizeof(error)));
    /* Capture view-dependent specular independently of albedo, including persistence. */
    gre_fvector4d view={3,-4,-5,1},reverse={-3,4,5,1};
    GRErgb24 specColor={255,240,220};
    GRE_Lightmap glossy=SceneBake_CreateView(cube,&lights,256,1,&view,.8f,16,specColor,error,sizeof(error));CHECK(glossy && glossy->specularPixels);
    GRE_Lightmap other=SceneBake_CreateView(cube,&lights,256,1,&reverse,.8f,16,specColor,error,sizeof(error));CHECK(other);
    unsigned highlights=0,viewChanges=0;
    for(unsigned i=0;i<256*256;i++) {
        if(glossy->specularPixels[i].R>20)highlights++;
        if(memcmp(&glossy->specularPixels[i],&other->specularPixels[i],sizeof(GRErgb24)))viewChanges++;
    }
    CHECK(highlights>100 && viewChanges>100);YMGRE_Free_Lightmap(other);
    other=SceneBake_CreateView(cube,&lights,256,1,&view,0,16,specColor,error,sizeof(error));CHECK(other);
    for(unsigned i=0;i<256*256;i++)CHECK(!other->specularPixels[i].R && !other->specularPixels[i].G && !other->specularPixels[i].B);
    YMGRE_Free_Lightmap(other);
    CHECK(SceneBake_SaveMaterial(glossy,hash,&base,root,path,sizeof(path),error,sizeof(error)));
    loaded=SceneBake_LoadMaterial(path,cube,&lights,&restored,error,sizeof(error));CHECK(loaded && restored);
    CHECK(loaded->specularPixels && !memcmp(loaded->specularPixels,glossy->specularPixels,256*256*sizeof(GRErgb24)));
    CHECK(loaded->referenceView.x==view.x && loaded->referenceView.y==view.y && loaded->referenceView.z==view.z);
    CHECK(loaded->specularKs==.8f && loaded->specularPower==16 && restored->advanced->specularPower==16);
    YMGRE_Free_Lightmap(loaded);YMGRE_Free_Material(restored);
    strcpy(imagePath,path);strcpy(strrchr(imagePath,'/')+1,"specular.bmp");
    CHECK(unlink(imagePath)==0);CHECK(!SceneBake_Load(path,cube,&lights,error,sizeof(error)));
    YMGRE_Free_Lightmap(glossy);
    /* Strong light must clamp after material color, matching realtime rendering. */
    float32 ambientStrength=ambient->proper.strength,pointStrength=point->proper.strength;
    ambient->proper.strength=2;point->proper.strength=0;cube->mirrorKs=0;
    gre_material blue={.ambient={92,155,220},.diffuse={12,24,48},.specular={255,255,255}};
    GRE_Lightmap colored=SceneBake_CreateSurface(cube,&lights,256,1,&view,&blue,error,sizeof(error));CHECK(colored && colored->colorsBaked);
    float* firstUv=colored->uv1;int cx=(firstUv[0]+firstUv[2]+firstUv[4])/3*256,cy=(firstUv[1]+firstUv[3]+firstUv[5])/3*256;
    GRErgb24 litColor=colored->pixels[cy*256+cx];CHECK(litColor.R==184 && litColor.G==255 && litColor.B==255);
    uint64_t coloredHash=SceneBake_ModeFingerprint(cube,&lights,0);
    CHECK(SceneBake_SaveMaterial(colored,coloredHash,&blue,root,path,sizeof(path),error,sizeof(error)));
    loaded=SceneBake_LoadMaterial(path,cube,&lights,&restored,error,sizeof(error));CHECK(loaded && restored && loaded->colorsBaked);
    CHECK(!memcmp(&loaded->capturedAmbient,&blue.ambient,sizeof(GRErgb24)) && !memcmp(&loaded->capturedDiffuse,&blue.diffuse,sizeof(GRErgb24)));
    YMGRE_Free_Lightmap(loaded);YMGRE_Free_Material(restored);YMGRE_Free_Lightmap(colored);
    ambient->proper.strength=ambientStrength;point->proper.strength=pointStrength;
    /* Specular is additive and must not inherit the blue texture tint. */
    GRErgb24 fixedSpec[4]={{20,30,40},{20,30,40},{20,30,40},{20,30,40}};tiny.specularPixels=fixedSpec;
    vertices[0].base=(gre_vertex4d){{2,2,1,1},.1f,.1f};
    vertices[1].base=(gre_vertex4d){{28,2,2,1},.1f,.1f};
    vertices[2].base=(gre_vertex4d){{2,28,2,1},.1f,.1f};
    YMGRE_CameraImage_Init(camera,(GRErgb24){0,0,0});
    YMGRE_TriangleRaster_FillLightmap_wN(vertices,&triangle,&material,&tiny,camera);
    GRErgb24 withSpec=GRE_FramePixel_To_RGB24(camera->img.data[4*32+18]);
    tiny.specularPixels=NULL;YMGRE_CameraImage_Init(camera,(GRErgb24){0,0,0});
    YMGRE_TriangleRaster_FillLightmap_wN(vertices,&triangle,&material,&tiny,camera);
    GRErgb24 noSpec=GRE_FramePixel_To_RGB24(camera->img.data[4*32+18]);
    CHECK(withSpec.R-noSpec.R==20 && withSpec.G-noSpec.G==30 && withSpec.B-noSpec.B==40);
    YMGRE_Free_Camera(camera);cube->lightmap=map;YMGRE_Free_Object(cube);YMGRE_List_Clear(&lights,YMGRE_Free_Light);
    printf("scene bake tests: PASS (UV1, diffuse lighting, persistence, invalidation, failure, raster, clipping)\n");return 0;
}
