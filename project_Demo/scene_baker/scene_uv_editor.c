#include "scene_uv_editor.h"
#include "scene_uv_edit.h"
#include "scene_uv_preview.h"
#include "scene_texture_image.h"
#include "scene_texture_paint.h"
#include "scene_editor_file_dialog.h"
#include <unistd.h>
#include <strings.h>
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGUI_DrawImg.h"
#include "scene_editor_place.h"
#include "YMGRE_Free.h"
#include "YMGUI_Button.h"
#include "YMGUI_Dropdown.h"
#include "YMGUI_Checkbox.h"
#include "YMGUI_TextInput.h"
#include "YMGUI_Label.h"
#include "YMGUI_Event.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_DrawFill.h"
#include "YMGUI_DrawLine.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef GY_KEY_REDO
#define GY_KEY_REDO 0x110E
#endif
static struct {
    GYOBJ parent,dialog,canvas,mode,tool,multi,info,stats,values[5];
    SceneEditorObject* object;SceneUvEdit* edit;GRE_List materials;
    GYOBJ sourceAction,sourceEdit,fileDialog;
    SceneTextureImage paintImage,importImage;SceneEditorObject* draftObject;GRE_Object4d draftMesh;
    int sourceMode,returnMode;char importDirectory[4096];
    GYOBJ preview,previewToggle,previewMode;int previewOn,previewDirty,orbit,orbitX,orbitY;
    float yaw,pitch,previewZoom;GYpx* previewPixels;GYpx* texturePixels;
    SceneUvEditorChanged changed;SceneUvEditorHistory history;SceneUvEditorApply apply;void* user;
    float centerU,centerV,zoom,aspect,startU,startV,pivotU,pivotV,panU,panV;
    int downX,downY,lastX,lastY,drag,box,pan;
} s;
static GYOBJ label(GYOBJ p,int x,int y,int w,int h,const char* text)
{
    GYOBJ l=YMGUI_Creat_Label_Creat(p,x,y,w,h);YMGUI_Label_SetText(l,text);YMGUI_Label_SetTextColor(l,0xFFE0E9F2);return l;
}
static GYOBJ button(GYOBJ p,int x,int y,int w,const char* text,GYbtn_clicked_cb cb)
{GYOBJ b=YMGUI_Creat_Button_Creat(p,x,y,w,32);YMGUI_Button_SetText(b,text);YMGUI_Button_SetClicked(b,cb);return b;}
static void uvAt(int x,int y,float* u,float* v)
{
    GYrect a;YMGUI_Obj_GetAbsArea(s.canvas,&a);
    *u=s.centerU+(x-a.x-a.w*.5f)/(s.zoom*s.aspect);*v=s.centerV+(y-a.y-a.h*.5f)/s.zoom;
}
static void screen(float u,float v,float* x,float* y)
{
    GYrect a;YMGUI_Obj_GetAbsArea(s.canvas,&a);
    *x=a.x+a.w*.5f+(u-s.centerU)*s.zoom*s.aspect;*y=a.y+a.h*.5f+(v-s.centerV)*s.zoom;
}
static void sourceControls(void);
static SceneTextureImage* selectedImage(void)
{int mode=YMGUI_Dropdown_GetSelected(s.previewMode);return mode==2?&s.paintImage:mode==3?&s.importImage:NULL;}
static void update(int previewChanged)
{
    sourceControls();unsigned count=0;
    if(s.edit)for(unsigned i=0;i<s.edit->count;i++)if(s.edit->points[i].selected&&s.edit->points[i].group==i)count++;
    char text[96];snprintf(text,sizeof(text),"选中 %u 个 UV 顶点",count);
    YMGUI_Label_SetText(s.stats,text);YMGUI_Obj_Invalidate(s.canvas);
    if(previewChanged){s.previewDirty=1;if(s.previewOn)YMGUI_Obj_Invalidate(s.preview);}
}
static void selectedBounds(float* u,float* v)
{
    float a=INFINITY,b=INFINITY,c=-INFINITY,d=-INFINITY;
    for(unsigned i=0;s.edit&&i<s.edit->count;i++)if(s.edit->points[i].selected) {
        SceneUvEditPoint p=s.edit->points[i];a=fminf(a,p.u);b=fminf(b,p.v);c=fmaxf(c,p.u);d=fmaxf(d,p.v);
    }
    *u=isfinite(a)?(a+c)*.5f:s.centerU;*v=isfinite(b)?(b+d)*.5f:s.centerV;
}
static void fit(int selected);
static GYpx nativePixel(GRErgb24 c){return GY_ColorToPx(0xFF000000u|((unsigned)c.R<<16)|((unsigned)c.G<<8)|c.B);}
static void textureDraw(GYSURFACE surface,const GYrect* area)
{
    if(!s.texturePixels)s.texturePixels=malloc(480*480*sizeof(GYpx));if(!s.texturePixels)return;
    int checker=YMGUI_Dropdown_GetSelected(s.previewMode)==0;
    GRE_Object4d mesh=s.object->mesh;
    for(unsigned i=0;i<s.edit->count;i++)if(s.edit->points[i].selected){mesh=s.edit->points[i].mesh;break;}
    GRE_Material material=s.materials?YMGRE_Material_Find(s.materials,mesh->materiaName):NULL;
    const GRErgb24* pixels=checker?SceneUvPreview_Checker(s.aspect==2):material?material->pixel:NULL;
    SceneTextureImage* custom=selectedImage();if(custom&&custom->pixels)pixels=custom->pixels;
    int w=checker?(s.aspect==2?400:200):material?material->width:0,h=checker?200:material?material->height:0;
    GRErgb24 color=checker?(GRErgb24){255,255,255}:material?material->diffuse:(GRErgb24){s.object->color>>16,s.object->color>>8,s.object->color};
    if(custom&&custom->pixels){w=custom->width;h=custom->height;color=(GRErgb24){255,255,255};}
    float stepU=1/(s.zoom*s.aspect),stepV=1/s.zoom,baseU=s.centerU-area->w*.5f*stepU,baseV=s.centerV-area->h*.5f*stepV;
    for(int y=0;y<area->h;y++)for(int x=0;x<area->w;x++) {
        float u=baseU+x*stepU,v=baseV+y*stepV;GRErgb24 c={23,32,45};
        if(u>=0&&u<1&&v>=0&&v<1) {
            c=(pixels&&w>0&&h>0)?pixels[(int)(v*h)*w+(int)(u*w)]:(GRErgb24){255,255,255};
            c=(GRErgb24){c.R*color.R/255,c.G*color.G/255,c.B*color.B/255};
        }
        s.texturePixels[y*area->w+x]=nativePixel(c);
    }
    GYimg img={s.texturePixels,area->w,area->h,0,0};YMGUI_Draw_Img(surface,&img,area->x,area->y);
}
static void previewDraw(GYOBJ widget,GYSURFACE surface,const GYrect* area)
{
    (void)widget;YMGUI_Draw_Fill(surface,area,0xFF17202D,255);if(!s.edit)return;
    if(!s.previewPixels){s.previewPixels=malloc(400*400*sizeof(GYpx));s.previewDirty=1;}if(!s.previewPixels)return;
    if(s.previewDirty) {
        GRErgb24* rgb=malloc(400*400*sizeof(*rgb));
        GRErgb24 color={s.object->color>>16,s.object->color>>8,s.object->color};
        SceneTextureImage* custom=selectedImage();
        int ok=rgb&&(custom&&custom->pixels?SceneUvPreview_RenderImage(s.edit,custom->pixels,custom->width,custom->height,s.yaw,s.pitch,s.previewZoom,400,rgb):SceneUvPreview_Render(s.edit,s.materials,(YMGUI_Dropdown_GetSelected(s.previewMode)==0?(s.aspect==2?2:1):0),color,s.yaw,s.pitch,s.previewZoom,400,rgb));
        for(int i=0;i<400*400;i++)s.previewPixels[i]=nativePixel(ok?rgb[i]:(GRErgb24){23,32,45});
        if(!ok)YMGUI_Label_SetText(s.info,"贴图预览生成失败");free(rgb);s.previewDirty=0;
    }
    GYimg img={s.previewPixels,400,400,0,0};YMGUI_Draw_Img(surface,&img,area->x+(area->w-400)/2,area->y+(area->h-400)/2);
}
static void previewEvent(GYOBJ widget,GYEvent e)
{
    if(!s.edit)return;int x=widget->ctx->point_x,y=widget->ctx->point_y;
    if(e==GY_EVENT_Pressed){s.orbit=1;s.orbitX=x;s.orbitY=y;}
    else if(e==GY_EVENT_Pressing&&s.orbit){s.yaw+=(x-s.orbitX)*.6f;s.pitch=fmaxf(-85,fminf(85,s.pitch+(y-s.orbitY)*.6f));s.orbitX=x;s.orbitY=y;s.previewDirty=1;YMGUI_Obj_Invalidate(widget);}
    else if(e==GY_EVENT_Released||e==GY_EVENT_ReleasedOff)s.orbit=0;
    else if(e==GY_EVENT_Wheel){s.previewZoom=fmaxf(.3f,fminf(2.0f,s.previewZoom*powf(1.12f,widget->ctx->wheel_y)));s.previewDirty=1;YMGUI_Obj_Invalidate(widget);}
}
static void sourceControls(void)
{
    if(!s.sourceAction||!s.previewMode)return;int mode=YMGUI_Dropdown_GetSelected(s.previewMode);
    YMGUI_Obj_SetHidden(s.sourceAction,mode==0);YMGUI_Obj_SetHidden(s.sourceEdit,mode!=3);
    YMGUI_Button_SetText(s.sourceAction,mode==3?"重新导入":mode==2?"编辑画板":"编辑图案");
}
static void paintDone(int accepted,const GRErgb24* pixels,unsigned width,unsigned height,void* user)
{
    (void)user;if(!s.object)return;
    if(accepted) {
        GRErgb24* copy=malloc((size_t)width*height*sizeof(*copy));
        if(copy){memcpy(copy,pixels,(size_t)width*height*sizeof(*copy));SceneTextureImage_Free(&s.paintImage);s.paintImage=(SceneTextureImage){copy,width,height};s.sourceMode=2;YMGUI_Label_SetText(s.info,"图案已就绪，点击应用贴图用于场景。");}
        else {s.sourceMode=s.returnMode;YMGUI_Label_SetText(s.info,"图案复制失败：内存不足");}
    }else s.sourceMode=s.returnMode;
    YMGUI_Dropdown_SetSelected(s.previewMode,s.sourceMode);update(1);
}
static void openPaint(int returnMode)
{
    int mode=YMGUI_Dropdown_GetSelected(s.previewMode);SceneTextureImage* custom=selectedImage();
    const GRErgb24* seed=custom?custom->pixels:NULL;unsigned width=seed?custom->width:512,height=seed?custom->height:(s.aspect==2?256:512);
    GRErgb24* tinted=NULL;
    if(mode==1) {
        GRE_Material m=s.materials?YMGRE_Material_Find(s.materials,s.object->mesh->materiaName):NULL;
        unsigned w=m&&m->pixel?m->width:0,h=m&&m->pixel?m->height:0;
        if(w>1&&h>1){width=w;height=h;}
        unsigned extent=width>height?width:height;if(extent>512){width=width*512/extent;height=height*512/extent;if(!width)width=1;if(!height)height=1;}
        tinted=malloc((size_t)width*height*sizeof(*tinted));if(!tinted){YMGUI_Label_SetText(s.info,"画板创建失败：内存不足");return;}
        GRErgb24 tint=m?m->diffuse:(GRErgb24){s.object->color>>16,s.object->color>>8,s.object->color};
        for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++) {GRErgb24 p=w&&h?m->pixel[(size_t)(y*h/height)*w+x*w/width]:(GRErgb24){255,255,255};tinted[y*width+x]=(GRErgb24){p.R*tint.R/255,p.G*tint.G/255,p.B*tint.B/255};}
        seed=tinted;
    }
    s.returnMode=returnMode;SceneTexturePaint_Open(s.canvas->ctx,seed,width,height,paintDone,NULL);free(tinted);
}
static void editTexture(GYOBJ button){(void)button;openPaint(s.sourceMode);}
static uint8 imageFilter(GYOBJ dialog,GYfiledialog_mode mode,const char* parent,const GYfiledialog_entry* entry,void* user)
{
    (void)dialog;(void)mode;(void)parent;(void)user;if(entry->is_dir)return 1;const char* ext=strrchr(entry->name,'.');
    return ext&&(!strcasecmp(ext,".png")||!strcasecmp(ext,".jpg")||!strcasecmp(ext,".jpeg")||!strcasecmp(ext,".bmp"));
}
static void imageChosen(GYOBJ dialog,uint8 accepted,const char* path,void* user)
{
    (void)dialog;(void)user;if(!s.object)return;
    char error[192];
    if(accepted&&SceneTextureImage_Read(path,&s.importImage,error,sizeof(error))) {
        s.sourceMode=3;snprintf(s.importDirectory,sizeof(s.importDirectory),"%s",path);char* slash=strrchr(s.importDirectory,'/');if(slash){if(slash==s.importDirectory)slash[1]=0;else *slash=0;}
        char text[128];snprintf(text,sizeof(text),"已导入 %u × %u，点击应用贴图。",s.importImage.width,s.importImage.height);YMGUI_Label_SetText(s.info,text);
    }else {s.sourceMode=s.returnMode;if(accepted)YMGUI_Label_SetText(s.info,error);}
    YMGUI_Dropdown_SetSelected(s.previewMode,s.sourceMode);update(1);
}
static void importTexture(int returnMode)
{
    s.returnMode=returnMode;
    if(!s.fileDialog){s.fileDialog=YMGUI_Creat_FileDialog_Creat(s.canvas->ctx,880,640,4095,255,512);YMGUI_FileDialog_SetFS(s.fileDialog,SceneEditorFileDialog_PosixFS(),NULL);YMGUI_FileDialog_SetResultCb(s.fileDialog,imageChosen,NULL);YMGUI_FileDialog_SetFilterCb(s.fileDialog,imageFilter,NULL);}
    if(!s.importDirectory[0]&&!getcwd(s.importDirectory,sizeof(s.importDirectory)))strcpy(s.importDirectory,".");
    if(!YMGUI_FileDialog_Show(s.fileDialog,GY_FILE_DIALOG_OPEN_FILE,s.importDirectory,NULL)){YMGUI_Dropdown_SetSelected(s.previewMode,returnMode);YMGUI_Label_SetText(s.info,"无法打开图像目录");}
}
static void sourceAction(GYOBJ button){if(YMGUI_Dropdown_GetSelected(s.previewMode)==3)importTexture(s.sourceMode);else editTexture(button);}
static void previewModeChanged(GYOBJ widget,uint16 selected)
{
    (void)widget;
    if(selected==2&&!s.paintImage.pixels){openPaint(s.sourceMode);return;}
    if(selected==3&&!s.importImage.pixels){importTexture(s.sourceMode);return;}
    s.sourceMode=selected;update(1);
}

static void applyTexture(GYOBJ widget)
{
    (void)widget;if(!s.object||!s.edit||!s.apply)return;char text[192];
    int mode=YMGUI_Dropdown_GetSelected(s.previewMode);SceneTextureImage* custom=selectedImage();
    const GRErgb24* pixels=mode==0?SceneUvPreview_Checker(s.aspect==2):custom?custom->pixels:NULL;
    unsigned width=mode==0?(s.aspect==2?400:200):custom?custom->width:0,height=mode==0?200:custom?custom->height:0;
    if(mode>=2&&!pixels){YMGUI_Label_SetText(s.info,"请先绘制或导入图像");return;}
    if(!s.apply(s.object,pixels,width,height,text,sizeof(text),s.user)){YMGUI_Label_SetText(s.info,text);return;}
    s.sourceMode=1;YMGUI_Dropdown_SetSelected(s.previewMode,1);YMGUI_Label_SetText(s.info,"贴图已应用，保存场景可保留。");
    if(s.changed)s.changed("贴图已应用到选中模型",s.user);update(1);
}
static void previewToggle(GYOBJ widget)
{
    (void)widget;s.previewOn=!s.previewOn;
    YMGUI_Obj_SetHidden(s.preview,!s.previewOn);YMGUI_Obj_SetHidden(s.previewMode,!s.previewOn);
    YMGUI_Button_SetText(s.previewToggle,s.previewOn?"关闭贴图预览":"贴图预览");
    if(!s.previewOn)YMGUI_Dropdown_Close(s.previewMode);fit(0);update(1);YMGUI_Obj_Invalidate(s.dialog);
}

/* Clip in floating point before converting to YMGUI's short screen coordinates. */
static void line(GYSURFACE surface,const GYrect* a,float x,float y,float X,float Y,GYcolor color)
{
    float dx=X-x,dy=Y-y,t0=0,t1=1,p[4]={-dx,dx,-dy,dy},q[4]={x-a->x,a->x+a->w-1-x,y-a->y,a->y+a->h-1-y};
    for(int i=0;i<4;i++) {
        if(fabsf(p[i])<1e-12f){if(q[i]<0)return;continue;}
        float t=q[i]/p[i];if(p[i]<0)t0=fmaxf(t0,t);else t1=fminf(t1,t);if(t0>t1)return;
    }
    YMGUI_Draw_Line(surface,(int)(x+t0*dx),(int)(y+t0*dy),(int)(x+t1*dx),(int)(y+t1*dy),color);
}
static void draw(GYOBJ widget,GYSURFACE surface,const GYrect* a)
{
    (void)widget;YMGUI_Draw_Fill(surface,a,0xFF17202D,255);if(!s.edit)return;
    if(s.previewOn)textureDraw(surface,a);
    else for(int y=0;y<10;y++)for(int x=0;x<10;x++) {
        float x0,y0,x1,y1;screen(x*.1f,y*.1f,&x0,&y0);screen((x+1)*.1f,(y+1)*.1f,&x1,&y1);
        x0=fmaxf(a->x,x0);y0=fmaxf(a->y,y0);x1=fminf(a->x+a->w,x1);y1=fminf(a->y+a->h,y1);
        if(x1>x0&&y1>y0){GYrect tile={(int)x0,(int)y0,(int)ceilf(x1-x0),(int)ceilf(y1-y0)};YMGUI_Draw_Fill(surface,&tile,(x+y)%2?0xFF293344:0xFF354255,255);}
    }
    unsigned base=0;
    for(GRE_Object4d m=s.object->mesh;m;m=m->nextObject) {
        for(int f=0;f<m->polygonNum;f++)for(int j=0;j<3;j++) {
            SceneUvEditPoint p=s.edit->points[base+m->polygonList[f].index[j]],q=s.edit->points[base+m->polygonList[f].index[(j+1)%3]];
            float x,y,X,Y;screen(p.u,p.v,&x,&y);screen(q.u,q.v,&X,&Y);
            line(surface,a,x,y,X,Y,p.selected&&q.selected?0xFFFFC16D:0xFF7DD2DF);
        }
        base+=m->pointNum;
    }
    if(YMGUI_Dropdown_GetSelected(s.mode)==0)for(unsigned i=0;i<s.edit->count;i++) {
        SceneUvEditPoint p=s.edit->points[i];float x,y;screen(p.u,p.v,&x,&y);
        if(x>=a->x+2&&x<a->x+a->w-2&&y>=a->y+2&&y<a->y+a->h-2) {
            GYrect dot={(int)x-2,(int)y-2,5,5};YMGUI_Draw_Fill(surface,&dot,p.selected?0xFFFFC16D:0xFFB6E4EF,255);
        }
    }
    if(s.box){line(surface,a,s.downX,s.downY,s.lastX,s.downY,0xFFFFC16D);line(surface,a,s.lastX,s.downY,s.lastX,s.lastY,0xFFFFC16D);
        line(surface,a,s.lastX,s.lastY,s.downX,s.lastY,0xFFFFC16D);line(surface,a,s.downX,s.lastY,s.downX,s.downY,0xFFFFC16D);}
}
static int hit(int x,int y)
{
    if(!s.edit)return -1;int nearest=-1;float distance=64;
    for(unsigned i=0;i<s.edit->count;i++) {
        float X,Y;screen(s.edit->points[i].u,s.edit->points[i].v,&X,&Y);float d=(x-X)*(x-X)+(y-Y)*(y-Y);
        if(d<distance){distance=d;nearest=i;}
    }
    if(YMGUI_Dropdown_GetSelected(s.mode)==0||nearest>=0)return nearest;
    float u,v;uvAt(x,y,&u,&v);unsigned base=0;
    for(GRE_Object4d m=s.object->mesh;m;m=m->nextObject) {
        for(int f=0;f<m->polygonNum;f++) {
            SceneUvEditPoint a=s.edit->points[base+m->polygonList[f].index[0]],b=s.edit->points[base+m->polygonList[f].index[1]],c=s.edit->points[base+m->polygonList[f].index[2]];
            float det=(b.u-a.u)*(c.v-a.v)-(b.v-a.v)*(c.u-a.u);if(fabsf(det)<1e-12f)continue;
            float p=((u-a.u)*(c.v-a.v)-(v-a.v)*(c.u-a.u))/det,q=((b.u-a.u)*(v-a.v)-(b.v-a.v)*(u-a.u))/det;
            if(p>=0&&q>=0&&p+q<=1)return base+m->polygonList[f].index[0];
        }
        base+=m->pointNum;
    }
    return -1;
}
static void commit(void)
{
    int changed=SceneUvEdit_Commit(s.edit);
    if(changed>0) {
        s.object->uvEdited=1;s.object->analytic=0;s.object->bakePath[0]=0;s.object->bakeFingerprint=0;s.object->rayBakeSignature=0;
        for(GRE_Object4d m=s.object->mesh;m;m=m->nextObject){YMGRE_Free_Lightmap(m->lightmap);m->lightmap=NULL;}
        YMGUI_Label_SetText(s.info,"UV 已修改，保存场景可保留。");
        if(s.changed)s.changed("手动 UV 已更新",s.user);
    }else if(changed<0){SceneUvEdit_Cancel(s.edit);YMGUI_Label_SetText(s.info,"UV 数值无效，已保留原结果");}
    update(1);
}
void SceneUvEditor_Close(void)
{
    SceneTexturePaint_Close();if(s.fileDialog)YMGUI_FileDialog_Close(s.fileDialog);
    s.drag=s.box=s.pan=s.orbit=0;SceneUvEdit_Free(s.edit);s.edit=NULL;s.object=NULL;
    if(s.dialog){YMGUI_Dropdown_Close(s.previewMode);YMGUI_Dropdown_Close(s.mode);YMGUI_Dropdown_Close(s.tool);YMGUI_Obj_SetHidden(s.dialog,1);}
}
int SceneUvEditor_IsOpen(void){return s.dialog&&!(s.dialog->state&GY_STATE_Hidden);}
void SceneUvEditor_Tick(void){SceneTexturePaint_Tick();}
void SceneUvEditor_Shutdown(void){SceneUvEditor_Close();SceneTexturePaint_Shutdown();SceneTextureImage_Free(&s.paintImage);SceneTextureImage_Free(&s.importImage);free(s.previewPixels);free(s.texturePixels);memset(&s,0,sizeof(s));}
static void closeClicked(GYOBJ b){(void)b;SceneUvEditor_Close();}
static void all(GYOBJ b){(void)b;if(s.edit)for(unsigned i=0;i<s.edit->count;i++)s.edit->points[i].selected=1;update(0);}
static void none(GYOBJ b){(void)b;if(s.edit)for(unsigned i=0;i<s.edit->count;i++)s.edit->points[i].selected=0;update(0);}
static void fit(int selected)
{
    float a=INFINITY,b=INFINITY,c=-INFINITY,d=-INFINITY;
    for(unsigned i=0;s.edit&&i<s.edit->count;i++)if(!selected||s.edit->points[i].selected) {
        SceneUvEditPoint p=s.edit->points[i];a=fminf(a,p.u);b=fminf(b,p.v);c=fmaxf(c,p.u);d=fmaxf(d,p.v);
    }
    if(!isfinite(a))return;s.centerU=(a+c)*.5f;s.centerV=(b+d)*.5f;
    s.zoom=fminf((s.canvas->area.w-64)/(fmaxf(.001f,c-a)*s.aspect),400/fmaxf(.001f,d-b));s.zoom=fminf(s.zoom,100000);update(0);
}
static void fitAll(GYOBJ b){(void)b;fit(0);}static void fitSelected(GYOBJ b){(void)b;fit(1);}
static void history(int redo)
{
    if(!s.object||!s.history)return;
    SceneEditorObject* object=s.object;GYOBJ parent=s.parent;GRE_List materials=s.materials;SceneUvEditorChanged changed=s.changed;SceneUvEditorHistory cb=s.history;SceneUvEditorApply apply=s.apply;void* user=s.user;
    SceneEditorObject* restored=cb(object,redo,user);
    if(restored)SceneUvEditor_Open(parent,restored,materials,changed,cb,apply,user);
}
static void undo(GYOBJ b){(void)b;history(0);}static void redo(GYOBJ b){(void)b;history(1);}
static void generate(GYOBJ b)
{
    (void)b;if(!s.object)return;SceneUvEdit_Free(s.edit);s.edit=NULL;int islands=0;char text[192];
    uint16 mode=s.object->primitiveKind==SCENE_PLACE_SPHERE?2:s.object->primitiveKind<=SCENE_PLACE_CAPSULE?5:4;
    if(SceneEditorObject_GenerateUv(s.object,mode,&islands,text,sizeof(text))) {
        if(mode!=2)s.object->analytic=0;s.object->bakePath[0]=0;s.object->bakeFingerprint=0;s.object->rayBakeSignature=0;
        snprintf(text,sizeof(text),mode==2?"已生成矩形经纬度 UV":"已生成 %d 个 UV 岛",islands);
        if(s.changed)s.changed("自动 UV 已生成",s.user);
    }
    s.edit=SceneUvEdit_Create(s.object->mesh);YMGUI_Label_SetText(s.info,text);fit(0);update(1);
}
static void numeric(GYOBJ b)
{
    (void)b;float value[5];
    for(int i=0;i<5;i++){char* end;const char* text=YMGUI_TextInput_GetText(s.values[i]);value[i]=strtof(text,&end);
        if(end==text||*end||!isfinite(value[i])){YMGUI_Label_SetText(s.info,"请输入有效数字");return;}}
    SceneUvEdit_Start(s.edit);
    if(!SceneUvEdit_Transform(s.edit,value[0],value[1],value[2],value[3],value[4])){YMGUI_Label_SetText(s.info,"先选 UV；缩放范围 0.0001–1000");return;}
    commit();
}
static void modeChanged(GYOBJ b,uint16 v){(void)b;(void)v;s.drag=s.box=0;SceneUvEdit_Cancel(s.edit);none(NULL);update(1);}
static void event(GYOBJ widget,GYEvent e)
{
    if(!s.edit)return;int x=widget->ctx->point_x,y=widget->ctx->point_y;
    if(e==GY_EVENT_Pressed) {
        YMGUI_SetFocus(widget->ctx,widget);s.downX=s.lastX=x;s.downY=s.lastY=y;int p=hit(x,y);
        int add=YMGUI_Checkbox_GetChecked(s.multi);
        if(p<0){if(!add)none(NULL);s.box=1;return;}
        if(!s.edit->points[p].selected||add)SceneUvEdit_Select(s.edit,p,YMGUI_Dropdown_GetSelected(s.mode),add);
        if(s.edit->points[p].selected){s.drag=1;SceneUvEdit_Start(s.edit);uvAt(x,y,&s.startU,&s.startV);selectedBounds(&s.pivotU,&s.pivotV);}update(0);
    }else if(e==GY_EVENT_Pressing) {
        s.lastX=x;s.lastY=y;
        if(s.drag) {
            float u,v;uvAt(x,y,&u,&v);unsigned tool=YMGUI_Dropdown_GetSelected(s.tool);
            if(tool==0)SceneUvEdit_Transform(s.edit,u-s.startU,v-s.startV,0,1,1);
            else if(tool==1){float angle=(atan2f(v-s.pivotV,u-s.pivotU)-atan2f(s.startV-s.pivotV,s.startU-s.pivotU))*57.2957795f;SceneUvEdit_Transform(s.edit,0,0,angle,1,1);}
            else {float old=hypotf(s.startU-s.pivotU,s.startV-s.pivotV);float scale=old>.00001f?hypotf(u-s.pivotU,v-s.pivotV)/old:expf((x-s.downX)*.01f);SceneUvEdit_Transform(s.edit,0,0,0,scale,scale);}
        }update(s.drag);
    }else if(e==GY_EVENT_Released||e==GY_EVENT_ReleasedOff) {
        if(s.box) {
            int island=YMGUI_Dropdown_GetSelected(s.mode);
            for(unsigned i=0;i<s.edit->count;i++) {
                float X,Y;screen(s.edit->points[i].u,s.edit->points[i].v,&X,&Y);
                if(X>=fminf(s.downX,x)&&X<=fmaxf(s.downX,x)&&Y>=fminf(s.downY,y)&&Y<=fmaxf(s.downY,y)&&!s.edit->points[i].selected)
                    SceneUvEdit_Select(s.edit,i,island,1);
            }
        }
        if(s.drag)commit();s.drag=s.box=0;update(0);
    }else if(e==GY_EVENT_Wheel&&!s.drag) {
        float beforeU,beforeV,afterU,afterV;uvAt(x,y,&beforeU,&beforeV);
        s.zoom=fmaxf(.1f,fminf(100000,s.zoom*powf(1.2f,widget->ctx->wheel_y)));uvAt(x,y,&afterU,&afterV);
        s.centerU+=beforeU-afterU;s.centerV+=beforeV-afterV;update(0);
    }else if(e==GY_EVENT_ContextRequested) {s.pan=1;s.downX=x;s.downY=y;s.panU=s.centerU;s.panV=s.centerV;}
    else if(e==GY_EVENT_ContextDragging&&s.pan) {s.centerU=s.panU-(x-s.downX)/(s.zoom*s.aspect);s.centerV=s.panV-(y-s.downY)/s.zoom;update(0);}
    else if(e==GY_EVENT_ContextReleased||e==GY_EVENT_ContextCancelled)s.pan=0;
    else if(e==GY_EVENT_Key) {
        unsigned key=widget->ctx->last_key;
        if(key==27){widget->ctx->key_handled=1;SceneUvEdit_Cancel(s.edit);s.drag=s.box=0;update(1);}
        else if(key==GY_KEY_SEL_ALL)all(NULL);else if(key==GY_KEY_UNDO)history(0);else if(key==GY_KEY_REDO)history(1);
    }
}
void SceneUvEditor_Open(GYOBJ parent,SceneEditorObject* object,GRE_List materials,SceneUvEditorChanged changed,SceneUvEditorHistory historyCb,SceneUvEditorApply apply,void* user)
{
    SceneUvEditor_Close();
    if(s.draftObject!=object||s.draftMesh!=(object?object->mesh:NULL)){SceneTextureImage_Free(&s.paintImage);SceneTextureImage_Free(&s.importImage);if(s.previewMode&&YMGUI_Dropdown_GetSelected(s.previewMode)>=2)YMGUI_Dropdown_SetSelected(s.previewMode,1);}
    s.draftObject=object;s.draftMesh=object?object->mesh:NULL;s.parent=parent;s.object=object;s.materials=materials;s.changed=changed;s.history=historyCb;s.apply=apply;s.user=user;
    if(!object||!object->mesh)return;s.edit=SceneUvEdit_Create(object->mesh);
    if(!s.dialog) {
        s.dialog=YMGUI_Creat_Obj_Creat(YMGUI_Ctx_GetTopLayer(parent->ctx),0,0,1024,816);YMGUI_Obj_SetBgColor(s.dialog,0xE0101620);
        GYOBJ p=YMGUI_Creat_Obj_Creat(s.dialog,16,24,992,768);YMGUI_Obj_SetBgColor(p,0xFF273142);
        label(p,20,16,200,28,"UV 编辑器");
        s.sourceAction=button(p,228,16,104,"编辑画板",sourceAction);
        s.sourceEdit=button(p,340,16,104,"用画板编辑",editTexture);
        button(p,452,16,168,"应用贴图",applyTexture);
        s.previewToggle=button(p,640,16,148,"关闭贴图预览",previewToggle);
        s.previewMode=YMGUI_Creat_Dropdown_Creat(p,800,16,172,32);
        YMGUI_Dropdown_AddOption(s.previewMode,"棋盘格");YMGUI_Dropdown_AddOption(s.previewMode,"当前材质");
        YMGUI_Dropdown_AddOption(s.previewMode,"画板");YMGUI_Dropdown_AddOption(s.previewMode,"导入图像");
        YMGUI_Dropdown_SetSelectedCb(s.previewMode,previewModeChanged);
        label(p,20,48,464,24,"UV 布局");label(p,508,48,464,24,"3D 贴图预览");
        s.preview=YMGUI_Creat_Obj_Creat(p,508,72,464,432);s.preview->draw_cb=previewDraw;s.preview->event_cb=previewEvent;
        s.previewOn=1;s.yaw=32;s.pitch=24;s.previewZoom=1;
        s.canvas=YMGUI_Creat_Obj_Creat(p,20,72,464,432);s.canvas->draw_cb=draw;s.canvas->event_cb=event;s.canvas->state|=GY_STATE_Focusable;
        label(p,20,536,64,26,"选择");s.mode=YMGUI_Creat_Dropdown_Creat(p,88,534,124,30);
        YMGUI_Dropdown_AddOption(s.mode,"顶点");YMGUI_Dropdown_AddOption(s.mode,"UV 岛");YMGUI_Dropdown_SetSelected(s.mode,1);YMGUI_Dropdown_SetSelectedCb(s.mode,modeChanged);
        label(p,224,536,64,26,"操作");s.tool=YMGUI_Creat_Dropdown_Creat(p,292,534,120,30);
        YMGUI_Dropdown_AddOption(s.tool,"移动");YMGUI_Dropdown_AddOption(s.tool,"旋转");YMGUI_Dropdown_AddOption(s.tool,"缩放");
        s.multi=YMGUI_Creat_Checkbox_Creat(p,432,534,196,30);YMGUI_Checkbox_SetText(s.multi,"追加 / 切换选择");
        button(p,640,534,100,"全选",all);button(p,752,534,100,"清除选择",none);button(p,864,534,108,"显示全部",fitAll);
        label(p,20,580,28,26,"ΔU");s.values[0]=YMGUI_Creat_TextInput_Creat(p,52,578,80,28,31);
        label(p,144,580,28,26,"ΔV");s.values[1]=YMGUI_Creat_TextInput_Creat(p,176,578,80,28,31);
        label(p,272,580,48,26,"角度");s.values[2]=YMGUI_Creat_TextInput_Creat(p,324,578,80,28,31);
        label(p,416,580,52,26,"缩放 U");s.values[3]=YMGUI_Creat_TextInput_Creat(p,472,578,76,28,31);
        label(p,560,580,52,26,"缩放 V");s.values[4]=YMGUI_Creat_TextInput_Creat(p,616,578,76,28,31);
        button(p,708,578,156,"应用数值变换",numeric);button(p,876,578,96,"显示选区",fitSelected);
        button(p,20,622,96,"撤销",undo);button(p,128,622,96,"重做",redo);
        s.stats=label(p,240,622,344,32,"");button(p,604,622,188,"重新自动展开",generate);button(p,804,622,168,"关闭",closeClicked);
        label(p,20,676,320,24,"UV：左键编辑，右键平移");label(p,340,676,292,24,"Esc 取消当前 UV 拖动");label(p,636,676,336,24,"模型：拖动旋转，滚轮缩放");
        s.info=label(p,20,708,952,24,"");
    }
    s.sourceMode=YMGUI_Dropdown_GetSelected(s.previewMode);
    for(int i=0;i<5;i++)YMGUI_TextInput_SetText(s.values[i],i<3?"0":"1");
    YMGUI_Checkbox_SetChecked(s.multi,0);s.aspect=object->primitiveKind==SCENE_PLACE_SPHERE?2:1;s.centerU=s.centerV=.5f;s.zoom=400/s.aspect;
    YMGUI_Label_SetText(s.info,s.edit?"自动展开会覆盖手动调整。":"无法编辑该网格的 UV");
    YMGUI_Obj_SetHidden(s.dialog,0);YMGUI_SetFocus(s.canvas->ctx,s.canvas);update(1);
}
