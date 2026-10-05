#define _POSIX_C_SOURCE 200809L
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_Camera.h"
#include "YMGRE_RenderContext.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_Material.h"
#include "YMCS_File_IO.h"
#include "raster_pool.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <sys/resource.h>

static const int optionKeys[]={SDLK_1,SDLK_2,SDLK_3,SDLK_6,SDLK_7,SDLK_8,SDLK_9};
#include <SDL2/SDL_ttf.h>
static int width=1200,height=800;
static const char *captureDir=".";
#ifndef YMGRE_GIRL_ASSET_DIR
#define YMGRE_GIRL_ASSET_DIR ""
#endif
static int has_assets(const char* root)
{
 char path[8192];snprintf(path,sizeof(path),"%s/BODY.mesh",root);
 return root[0]&&access(path,R_OK)==0;
}
static int asset_root(char* root,size_t capacity,const char* requested)
{
 if(requested){
  if(strlen(requested)>=capacity)return 0;
  strcpy(root,requested);return has_assets(root);
 }
 const char* defaults[]={YMGRE_GIRL_ASSET_DIR,"Resource/girl"};
 for(unsigned i=0;i<sizeof(defaults)/sizeof(defaults[0]);i++)
  if(strlen(defaults[i])<capacity&&has_assets(defaults[i])){strcpy(root,defaults[i]);return 1;}
 char executable[4096];ssize_t length=readlink("/proc/self/exe",executable,sizeof(executable)-1);
 if(length<=0)return 0;executable[length]=0;
 char* slash;
 while((slash=strrchr(executable,'/'))!=NULL){
  *slash=0;
  if(strlen(executable)<capacity&&has_assets(executable)){strcpy(root,executable);return 1;}
  int n=snprintf(root,capacity,"%s/Resource/girl",executable);
  if(n>=0&&(size_t)n<capacity&&has_assets(root))return 1;
 }
 return 0;
}
static const char *names[]={"BODY","boot","bra","eyes","Mouth","skirt","Corset","top","eyesbrow","hair","hat"};
static double now(clockid_t clock){struct timespec t;clock_gettime(clock,&t);return t.tv_sec+t.tv_nsec*1e-9;}
typedef struct {
 gre_scence scene;gre_list objects,lights;
 GRE_Camera4d camera;GRE_RenderWorkspace workspace;
 GRE_Object4d object[11];GRE_Material material[11];GRErgb24 *normal[11];
 uint8 *opacity[11];
 SDL_Window *window;SDL_Renderer *renderer;SDL_Texture *texture;
 unsigned char *pixels;char root[4096];
 TTF_Font *font;
 RasterPool* pool;int threads;
 int materialSided[11];
 int sided,alpha,mip,materialParams,normalParams,linearColor,pixelLighting,animate,hat;float exposure;float orbit,elevation,distance,target_x,target_y;
 double pipeline_ms,present_ms,frame_ms,cpu_ms;long major_faults;
} Viewer;
static void reset(Viewer *v){v->orbit=-.28f;v->elevation=.015f;v->distance=2.45f;v->target_x=0;v->target_y=1.78f;}
/* Data maps may stay PNG when their lossless file is smaller than JPEG. */
static int load_map(const char* root,const char* name,const char* kind,
                    GRErgb24** pixels,uint16* width,uint16* height)
{
 char path[8192];snprintf(path,sizeof(path),"%s/%s_%s.jpg",root,name,kind);
 if(access(path,R_OK)!=0)snprintf(path,sizeof(path),"%s/%s_%s.png",root,name,kind);
 return YMGRE_Image_LoadRGB(path,pixels,width,height);
}
static int load(Viewer *v)
{
 for(int i=0;i<11;i++){
  char path[8192];snprintf(path,sizeof(path),"%s/%s.mesh",v->root,names[i]);
  GRE_Object4d object=YMGRE_LoadOgreMeshAndMaterial(&v->scene,path);
  if(!object||!YMGRE_Object_EnableVertexAttributes(object))return 0;
  GRE_Material material=YMGRE_Material_Find(&v->scene.MaterialList,(char*)names[i]);
  if(!material||!material->pixel)return 0;
  v->materialSided[i]=material->doubleSided;
  if(!material->advanced)material->advanced=calloc(1,sizeof(*material->advanced));
  if(!material->advanced)return 0;
  GRE_MaterialAdvanced a=material->advanced;
  snprintf(path,sizeof(path),"%s/%s.pbr",v->root,names[i]);FILE *f=fopen(path,"r");
  float ior,strength;int sided,hasNormal;
  if(!f)return 0;
  int parsed=fscanf(f,"%f %f %d %d",&ior,&strength,&sided,&hasNormal);fclose(f);if(parsed!=4)return 0;
  a->pbrEnabled=1;a->pbrIOR=ior;a->pbrNormalStrength=strength;
  if(i==0)a->pbrNormalStrength*=.35f;
  if(i==8)a->pbrNormalStrength*=.5f;
  if(!load_map(v->root,names[i],"mrs",&a->pbrParameters,&a->pbrWidth,&a->pbrHeight))return 0;
  if(i==9)for(size_t j=0;j<(size_t)a->pbrWidth*a->pbrHeight;j++)a->pbrParameters[j].B=(uint8)(a->pbrParameters[j].B*.45f+.5f);
  if(hasNormal&&!load_map(v->root,names[i],"normal",&a->normalPixel,&a->normalWidth,&a->normalHeight))return 0;
  v->object[i]=object;v->material[i]=material;v->normal[i]=a->normalPixel;
  for(int j=0;j<object->pointNum;j++){object->pointList_wN[j].normal=object->importedNormals[j];object->pointList_wN[j].color=(GRErgb24){255,255,255};}
  object->boundType=GRE_Bounding_Sphere_R;object->renderMode=GRE_RenderMode_Vertex;
  if(!strcmp(names[i],"hair"))object->mirrorKs=.35f;
  YMGRE_List_Append(&v->objects,sizeof(*object),object);
 }
 char path[8192];snprintf(path,sizeof(path),"%s/hair_alpha_edited.material",v->root);
 YMGRE_ParseMaterialScript(&v->scene,path);
 GRE_Material alternate=YMGRE_Material_Find(&v->scene.MaterialList,"hair_edit_alpha");
 if(!alternate||!alternate->advanced)return 0;
 GRE_MaterialAdvanced source=alternate->advanced;
 if(!YMGRE_Material_SetOpacity(v->material[9],source->opacityPixel,source->opacityWidth,source->opacityHeight,1))return 0;
 for(int i=0;i<11;i++)v->opacity[i]=v->material[i]->advanced->opacityPixel;
 const GRErgb24 colors[]={{255,255,255},{255,242,228},{242,239,235},{255,236,215}};
 const float strengths[]={.12f,.72f,.18f,.035f};
 const gre_fvector4d positions[]={{0,0,0,1},{-3,5,8,1},{4,3,6,1},{-5,5,-4,1}};
 for(int i=0;i<4;i++){
  GRE_Light4d light=YMGRE_Creat_Light(i,i?GRE_PointLight:GRE_GlobalLight,colors[i],strengths[i]);
  if(!light)return 0;
  light->pos=positions[i];light->proper.kc0=1;light->proper.kc1=light->proper.kc2=0;
  YMGRE_List_Append(&v->lights,sizeof(*light),light);
 }
 v->camera=YMGRE_Creat_Camera(0,width,height,28,28,21,21);v->workspace=YMGRE_Creat_RenderWorkspace();
 if(!v->camera||!v->workspace)return 0;
 YMGRE_Camera_Frustum_Init(v->camera,.025f,80.f);return 1;
}

#define PANEL 300
static void configure(Viewer *v)
{
 for(int i=0;i<11;i++){
  v->material[i]->doubleSided=v->sided==2||(v->sided==1&&v->materialSided[i]);
  v->material[i]->advanced->opacityPixel=v->alpha?v->opacity[i]:NULL;
  v->material[i]->advanced->normalPixel=v->normal[i];
  v->object[i]->renderMode=v->pixelLighting?GRE_RenderMode_Pixel:GRE_RenderMode_Vertex;
 }
 v->camera->pbrEnabled=v->pixelLighting&&v->materialParams;
 v->camera->pbrNormalEnabled=v->normalParams;
 v->camera->linearColorEnabled=v->linearColor&&v->pixelLighting;
 v->camera->exposure=v->exposure;
 for(int i=0;i<11;i++){v->material[i]->advanced->opacityUseMip=v->mip;v->material[i]->advanced->normalPixel=v->normalParams?v->normal[i]:NULL;}
 v->object[10]->isVisible=v->hat;
}
static void text_line(Viewer *v,int x,int y,const char *text,SDL_Color color)
{
 SDL_Surface *surface=TTF_RenderUTF8_Blended(v->font,text,color);if(!surface)return;
 SDL_Texture *texture=SDL_CreateTextureFromSurface(v->renderer,surface);
 if(texture){SDL_Rect dst={x,y,surface->w,surface->h};SDL_RenderCopy(v->renderer,texture,NULL,&dst);SDL_DestroyTexture(texture);}
 SDL_FreeSurface(surface);
}
static void panel(Viewer *v)
{
 SDL_Color white={230,234,240,255},muted={162,174,190,255};
 SDL_Rect bg={0,0,PANEL,height};SDL_SetRenderDrawColor(v->renderer,25,29,36,255);SDL_RenderFillRect(v->renderer,&bg);
 text_line(v,16,12,v->pixelLighting?"逐像素光照 · 功能对比":"顶点光照 · 功能对比",white);
 text_line(v,16,42,"默认逐像素，按 9 切换对比",muted);
 const char *labels[]={"1  双面策略","2  透明发丝 / 衣物","3  Mip 缩小过滤","6  金属 / 粗糙 / 高光","7  法线贴图","8  线性色彩 / 曝光","9  逐像素光照"};
 int values[]={v->sided,v->alpha,v->mip,v->materialParams,v->normalParams,v->linearColor,v->pixelLighting};
 for(int i=0;i<7;i++){
  SDL_Rect row={12,78+i*42,PANEL-24,36};
  SDL_SetRenderDrawColor(v->renderer,values[i]?42:40,values[i]?87:45,values[i]?70:55,255);SDL_RenderFillRect(v->renderer,&row);
  char label[128];snprintf(label,sizeof(label),"[%s]  %s",i==0?(v->sided==0?"关":(v->sided==1?"按材质":"全部")):(values[i]?"开":"关"),labels[i]);text_line(v,22,row.y+7,label,white);
 }
 text_line(v,16,381,v->sided==1?"身体、眼球、靴子单面":"按 1 循环：关闭 / 按材质 / 全部",muted);
 text_line(v,16,405,v->pixelLighting?(v->linearColor?"线性色彩；8 切换 RGB 直接输出":"RGB 直接输出；保留 GGX 材质"):"顶点模式：6/7/8 仅逐像素使用",muted);
 const char *buttons[]={"0  一键基础","5  常用效果"};
 for(int i=0;i<2;i++){
  SDL_Rect row={12+i*144,442,132,36};SDL_SetRenderDrawColor(v->renderer,49,60,78,255);SDL_RenderFillRect(v->renderer,&row);text_line(v,row.x+9,row.y+7,buttons[i],white);
 }
 char timing[128];snprintf(timing,sizeof(timing),"当前帧 %.1f ms / 约 %.2f 帧/秒",v->pipeline_ms,1000/fmax(.001,v->pipeline_ms));text_line(v,16,494,timing,white);
 snprintf(timing,sizeof(timing),"%d×%d · %d 线程 · 视距 %.2f",width,height,v->workspace->rasterDispatch&&v->camera->pbrEnabled?v->threads:1,v->distance);text_line(v,16,524,timing,muted);
 if(v->linearColor&&v->materialParams&&v->pixelLighting)snprintf(timing,sizeof(timing),"曝光 %.2f  ·  +/- 调整",v->exposure);else snprintf(timing,sizeof(timing),"曝光不生效 · RGB 直接输出");text_line(v,16,552,timing,muted);
 text_line(v,16,590,"A 自动转动   R 复位视角",muted);
 text_line(v,16,620,"F 全身/半身   H 显示帽子",muted);
 text_line(v,16,650,"左拖旋转 · 右拖平移 · 滚轮缩放",muted);
 text_line(v,16,678,"T 单核/多核 · 先不透明，后透明",muted);
}
static int render(Viewer *v)
{
 configure(v);double start=now(CLOCK_MONOTONIC),cpu=now(CLOCK_THREAD_CPUTIME_ID);
 float radius=v->distance*cosf(v->elevation);
 gre_fvector4d eye={v->target_x+radius*sinf(v->orbit),v->target_y+v->distance*sinf(v->elevation),radius*cosf(v->orbit),1};
 gre_fvector4d target={v->target_x,v->target_y,0,1};
 YMGRE_UVNCamera_PositionInit(v->camera,&eye,&target,NULL,0);
 YMGRE_Camera_TanglePipline_wN(v->camera,&v->lights,&v->objects,&v->scene.MaterialList,v->workspace);
 if(v->workspace->materialStatus){fprintf(stderr,"Material workspace allocation failed\n");return 0;}
 v->pipeline_ms=(now(CLOCK_MONOTONIC)-start)*1000;
 v->cpu_ms=(now(CLOCK_THREAD_CPUTIME_ID)-cpu)*1000;
 GRE_RenderTarget rt=YMGRE_Camera_GetRenderTarget(v->camera);if(!rt||!rt->data)return 0;
 for(int i=0;i<width*height;i++){GRErgb24 c=GRE_FramePixel_To_RGB24(rt->data[i]);v->pixels[3*i]=c.R;v->pixels[3*i+1]=c.G;v->pixels[3*i+2]=c.B;}
 if(SDL_UpdateTexture(v->texture,NULL,v->pixels,width*3)<0)return 0;
 SDL_SetRenderDrawColor(v->renderer,25,29,36,255);SDL_RenderClear(v->renderer);
 SDL_Rect dst={PANEL,0,width,height};SDL_RenderCopy(v->renderer,v->texture,NULL,&dst);panel(v);SDL_RenderPresent(v->renderer);
 v->frame_ms=(now(CLOCK_MONOTONIC)-start)*1000;
 char title[256];snprintf(title,sizeof(title),"%s lighting | double side %s | alpha %s | mip %s | material %d | normal %d | draw %.1f ms | frame %.1f ms",
 v->pixelLighting?"Pixel":"Vertex",
 v->sided==2?"ALL":(v->sided==1?"MATERIAL":"OFF"),v->alpha?"ON":"OFF",v->mip?"ON":"OFF",v->materialParams,v->normalParams,v->pipeline_ms,v->frame_ms);
 SDL_SetWindowTitle(v->window,title);return 1;
}
static int option(Viewer *v,int key)
{
 if(key==SDLK_0){v->sided=v->alpha=v->mip=0;v->materialParams=v->normalParams=v->pixelLighting=0;v->linearColor=1;v->exposure=1;}
 else if(key==SDLK_5){v->sided=v->alpha=v->mip=1;}
 else if(key==SDLK_1)v->sided=(v->sided+1)%3;
 else if(key==SDLK_2)v->alpha=!v->alpha;
 else if(key==SDLK_3||key==SDLK_4)v->mip=!v->mip;
 else if(key==SDLK_6){v->materialParams=!v->materialParams;if(!v->materialParams)v->pixelLighting=0;}
 else if(key==SDLK_7)v->normalParams=!v->normalParams;
 else if(key==SDLK_8)v->linearColor=!v->linearColor;
 else if(key==SDLK_9){v->pixelLighting=!v->pixelLighting;if(v->pixelLighting)v->materialParams=1;}
 else if(key==SDLK_PLUS||key==SDLK_EQUALS||key==SDLK_KP_PLUS)v->exposure=fminf(4,v->exposure*1.1f);
 else if(key==SDLK_MINUS||key==SDLK_KP_MINUS)v->exposure=fmaxf(.25f,v->exposure/1.1f);
 else return 0;
 return 1;
}
static int save_frame(Viewer *v,const char *name)
{
 char path[8192];snprintf(path,sizeof(path),"%s/library_%s.ppm",captureDir,name);
 FILE *f=fopen(path,"wb");if(!f)return 0;
 fprintf(f,"P6\n%d %d\n255\n",width,height);int ok=fwrite(v->pixels,3,(size_t)width*height,f)==(size_t)width*height;fclose(f);
 fprintf(stderr,"CAPTURE %s: %.3f ms\n",name,v->pipeline_ms);return ok;
}
static int smoke(Viewer *v)
{
 size_t bytes=(size_t)width*height*3;unsigned char *ref=malloc(bytes);if(!ref)return 0;
 if(!render(v)||v->workspace->materialStatus||!save_frame(v,"pixel"))return 0;
 memcpy(ref,v->pixels,bytes);
 YMGRE_RenderWorkspace_SetRasterDispatcher(v->workspace,NULL,NULL);
 if(!render(v)||memcmp(ref,v->pixels,bytes))return 0;
 if(v->pool)YMGRE_RenderWorkspace_SetRasterDispatcher(v->workspace,raster_pool_dispatch,v->pool);
 option(v,SDLK_8);if(!render(v)||!save_frame(v,"rgb"))return 0;
 option(v,SDLK_8);if(!render(v)||memcmp(ref,v->pixels,bytes))return 0;
 GRE_ListNode nodes[11];int count=0;for(GRE_ListNode n=v->objects.listhead;n;n=n->next)nodes[count++]=n;
 for(int i=0;i<count;i++)nodes[i]->next=i?nodes[i-1]:NULL;
 v->objects.listhead=nodes[count-1];if(!render(v)||memcmp(ref,v->pixels,bytes))return 0;
 for(int i=0;i<count;i++)nodes[i]->next=i+1<count?nodes[i+1]:NULL;
 v->objects.listhead=nodes[0];
 v->distance=1.5f;v->target_y=2.1f;v->hat=0;if(!render(v)||!save_frame(v,"close"))return 0;
 reset(v);v->hat=1;v->distance=9.2f;v->target_y=0;if(!render(v)||!save_frame(v,"body"))return 0;
 reset(v);v->distance=.2f;if(!render(v))return 0;
 for(size_t i=0;i<(size_t)width*height*3;i++)if(!isfinite(v->workspace->linearColor[i])||v->workspace->linearColor[i]<0)return 0;
 reset(v);if(!render(v)||memcmp(ref,v->pixels,bytes))return 0;
 option(v,SDLK_0);if(!render(v)||!save_frame(v,"base"))return 0;
 free(ref);fprintf(stderr,"PASS: serial/parallel identical, linear/RGB restore, reversed submission, close/body, near clip, baseline\n");return 1;
}
static int benchmark(Viewer *v)
{
 printf("round,frame,draw_ms\n");
 for(int round=0;round<2;round++){
  reset(v);if(!render(v)||!render(v))return 0;
  for(int i=0;i<8;i++){
   v->orbit=-.28f+.35f*sinf(6.283185307f*i/8);if(!render(v))return 0;
   printf("%d,%d,%.6f\n",round,i,v->pipeline_ms);fflush(stdout);
  }
 }
 return 1;
}
static void destroy(Viewer *v)
{
 raster_pool_destroy(v->pool);
 for(int i=0;i<11;i++)if(v->material[i]&&v->material[i]->advanced){v->material[i]->advanced->opacityPixel=v->opacity[i];v->material[i]->advanced->normalPixel=v->normal[i];}
 YMGRE_List_Clear(&v->objects,YMGRE_Free_Object);
 YMGRE_List_Clear(&v->lights,YMGRE_Free_Light);
 YMGRE_List_Clear(&v->scene.MaterialList,YMGRE_Free_Material);
 YMGRE_Free_RenderWorkspace(v->workspace);YMGRE_Free_Camera(v->camera);
 free(v->pixels);SDL_DestroyTexture(v->texture);SDL_DestroyRenderer(v->renderer);SDL_DestroyWindow(v->window);
 TTF_CloseFont(v->font);TTF_Quit();SDL_Quit();
}
int main(int argc,char **argv)
{
 int test=0,bench=0,threads=SDL_GetCPUCount();if(threads>16)threads=16;if(threads<1)threads=1;const char *assets=NULL;
 for(int i=1;i<argc;i++){
  if(!strcmp(argv[i],"--smoke-test"))test=1;
  else if(!strcmp(argv[i],"--benchmark"))bench=1;
  else if(!strcmp(argv[i],"--threads")&&i+1<argc){char* end;long n=strtol(argv[++i],&end,10);if(*end||n<1||n>32)return 2;threads=(int)n;}
  else if(!strcmp(argv[i],"--assets")&&i+1<argc)assets=argv[++i];
  else if(!strcmp(argv[i],"--capture-dir")&&i+1<argc)captureDir=argv[++i];
  else if(!strcmp(argv[i],"--size")&&i+1<argc){if(sscanf(argv[++i],"%dx%d",&width,&height)!=2)return 2;}
  else return 2;
 }
 if(width<600||height<720||width>2400||height>1600)return 2;
 Viewer v={0};v.hat=1;v.linearColor=1;v.exposure=1;reset(&v);
 option(&v,SDLK_5);option(&v,SDLK_7);option(&v,SDLK_9);
 if(!asset_root(v.root,sizeof(v.root),assets)){fprintf(stderr,"Girl assets not found. Usage: %s --assets /path/to/Resource/girl [--threads 1..32] [--smoke-test|--benchmark] [--capture-dir DIR]\n",argv[0]);return 2;}
 fprintf(stderr,"Loading girl viewer...\n");if(!load(&v))return 1;
 if(SDL_Init(SDL_INIT_VIDEO)!=0||TTF_Init()!=0)return 1;
 v.threads=threads;
 if(threads>1){v.pool=raster_pool_create(threads);if(!v.pool)return 1;YMGRE_RenderWorkspace_SetRasterDispatcher(v.workspace,raster_pool_dispatch,v.pool);}
 v.font=TTF_OpenFont("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",16);
 if(!v.font){fprintf(stderr,"Chinese font unavailable: %s\n",TTF_GetError());return 1;}
 v.window=SDL_CreateWindow("YMGRE girl viewer",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,width+PANEL,height,SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);if(!v.window)return 1;
 v.renderer=SDL_CreateRenderer(v.window,-1,SDL_RENDERER_ACCELERATED);if(!v.renderer)v.renderer=SDL_CreateRenderer(v.window,-1,SDL_RENDERER_SOFTWARE);if(!v.renderer)return 1;
 SDL_RenderSetLogicalSize(v.renderer,width+PANEL,height);v.texture=SDL_CreateTexture(v.renderer,SDL_PIXELFORMAT_RGB24,SDL_TEXTUREACCESS_STREAMING,width,height);v.pixels=malloc((size_t)width*height*3);
 if(!v.texture||!v.pixels)return 1;
 if(test||bench){int ok=test?smoke(&v):benchmark(&v);destroy(&v);return ok?0:1;}
 if(!render(&v))return 1;
 int running=1,drag=0;
 while(running){
  SDL_Event e;int got=SDL_WaitEventTimeout(&e,v.animate?0:100),dirty=0;
  if(got)do{
   if(e.type==SDL_QUIT)running=0;
   else if(e.type==SDL_KEYDOWN&&!e.key.repeat){
    int key=e.key.keysym.sym;
    if(option(&v,key))dirty=1;
    else if(key==SDLK_ESCAPE)running=0;
    else if(key==SDLK_a)v.animate=!v.animate;
    else if(key==SDLK_t&&v.pool){YMGRE_RenderWorkspace_SetRasterDispatcher(v.workspace,v.workspace->rasterDispatch?NULL:raster_pool_dispatch,v.pool);dirty=1;}
    else if(key==SDLK_h){v.hat=!v.hat;dirty=1;}
    else if(key==SDLK_r){reset(&v);v.hat=1;dirty=1;}
    else if(key==SDLK_f){if(v.distance>4)reset(&v);else{v.distance=9.2f;v.target_y=0;}dirty=1;}
   }else if(e.type==SDL_MOUSEBUTTONDOWN){
    if(e.button.x<PANEL){
     if(e.button.button==SDL_BUTTON_LEFT){
      for(int i=0;i<7;i++)if(e.button.x>=12&&e.button.x<PANEL-12&&e.button.y>=78+i*42&&e.button.y<114+i*42){option(&v,optionKeys[i]);dirty=1;}
      if(e.button.y>=442&&e.button.y<478){if(e.button.x>=12&&e.button.x<144){option(&v,SDLK_0);dirty=1;}else if(e.button.x>=156&&e.button.x<288){option(&v,SDLK_5);dirty=1;}}
     }
    }else if(e.button.button==SDL_BUTTON_LEFT||e.button.button==SDL_BUTTON_RIGHT)drag=e.button.button;
   }else if(e.type==SDL_MOUSEBUTTONUP){if(e.button.button==drag)drag=0;}
   else if(e.type==SDL_MOUSEWHEEL){v.distance=fminf(12,fmaxf(.2f,v.distance*expf(-.16f*e.wheel.y)));dirty=1;}
   else if(e.type==SDL_MOUSEMOTION){
    if(drag==SDL_BUTTON_LEFT&&(e.motion.state&SDL_BUTTON_LMASK)){v.orbit+=e.motion.xrel*.006f;v.elevation=fmaxf(-1.45f,fminf(1.45f,v.elevation+e.motion.yrel*.004f));dirty=1;}
    if(drag==SDL_BUTTON_RIGHT&&(e.motion.state&SDL_BUTTON_RMASK)){v.target_x-=e.motion.xrel*v.distance*.0015f;v.target_y+=e.motion.yrel*v.distance*.0015f;dirty=1;}
   }else if(e.type==SDL_WINDOWEVENT){
    if(e.window.event==SDL_WINDOWEVENT_FOCUS_LOST)drag=0;
    if(e.window.event==SDL_WINDOWEVENT_EXPOSED||e.window.event==SDL_WINDOWEVENT_SIZE_CHANGED)dirty=1;
   }
  }while(SDL_PollEvent(&e));
  if(v.animate){v.orbit+=.025f;dirty=1;}
  if(running&&dirty&&!render(&v))running=0;
 }
 destroy(&v);return 0;
}
