#include "YMGRE_Light.h"
static GYpx* smoothScreen;
static void (*smoothOriginalFlush)(GYdisp*,const GYrect*,const GYpx*);
static void smoothFlush(GYdisp* d,const GYrect* area,const GYpx* pixels)
{
    for(int y=0;y<area->h;y++)
        memcpy(smoothScreen+(size_t)(area->y+y)*d->hor_res+area->x,
               pixels+(size_t)y*area->w,area->w*sizeof(*pixels));
    smoothOriginalFlush(d,area,pixels);
}
static int smoothFinish(void)
{
    for(int i=0;i<1000;i++) {
        if(!SDL_LCD_PumpEvents())return 0;
        renderScene();YMGUI_Obj_Invalidate(g_image);YMGUI_Refresh(g_ui->host.context);
        if(g_rayProgress==100)return 1;
    }
    return 0;
}
static GYOBJ smoothFindRayButton(GYOBJ parent)
{
    if(parent->area.x==12 && parent->area.y==166 && parent->area.w==178 && parent->area.h==30)return parent;
    for(GYOBJ c=parent->child_head;c;c=c->sibling){GYOBJ found=smoothFindRayButton(c);if(found)return found;}
    return NULL;
}
static int runSmoothSphereSelfTest(void)
{
#define SMOOTH_CHECK(c) do{if(!(c)){fprintf(stderr,"Smooth sphere line %d: %s\n",__LINE__,#c);return 1;}}while(0)
    char temporary[]="/tmp/ymgre-smooth-XXXXXX";const char* root=getenv("YMGRE_SMOOTH_OUTPUT_DIR");if(!root)root=mkdtemp(temporary);SMOOTH_CHECK(root);
    createNewScene();
    GYdisp* display=&g_ui->host.display;
    smoothScreen=calloc((size_t)display->hor_res*display->ver_res,sizeof(*smoothScreen));SMOOTH_CHECK(smoothScreen);
    smoothOriginalFlush=display->flush_cb;display->flush_cb=smoothFlush;
    SceneEditorObject* sphere=opticsObject(SCENE_PLACE_SPHERE,"Plain Sphere",0,18,0,1,0xFF5CB07E);SMOOTH_CHECK(sphere);
    SceneEditorPlaceResult lamp={.kind=SCENE_PLACE_POINT_LIGHT,.x=-35,.y=50,.z=-55,.strength=1.2f,.color=0xFFFFFFFF};
    strcpy(lamp.name,"Point Light");objectPlaced(&lamp,NULL);
    g_globalLight->strength=.12f;SceneEditorObject_SyncHandle(g_globalLight);
    g_activeCamera->x=0;g_activeCamera->y=18;g_activeCamera->z=-60;g_activeCamera->targetY=18;
    selectObject(sphere,"普通球体＋点光源：三角网格对照");
    GYOBJ rayButton=smoothFindRayButton(g_rightPanel);SMOOTH_CHECK(rayButton && (rayButton->state&GY_STATE_Hidden));
    rendererClickOption(1);SMOOTH_CHECK(!(rayButton->state&GY_STATE_Hidden));
    g_referenceVisible=0;g_transformMode=TRANSFORM_NONE;g_selected=NULL;
    SMOOTH_CHECK(smoothFinish());rendererSaveScreen(root,"mesh");
    size_t bytes=(size_t)g_target.width*g_target.height*sizeof(GRE_FramePixel);GRE_FramePixel* faceted=malloc(bytes);SMOOTH_CHECK(faceted);memcpy(faceted,g_target.data,bytes);
    selectObject(sphere,"普通球体＋点光源：解析曲面对照");opticsUiMaterial(0,1,1);
    SMOOTH_CHECK(sphere->analytic && sphere->rayType==0 && !sphere->mesh->lightmap);
    SMOOTH_CHECK(g_rayProgress==0); /* Applying while already tracing must invalidate completion immediately. */
    g_selected=NULL;SMOOTH_CHECK(smoothFinish());
    /* Compare actual viewport samples with an independent sphere intersection + point-light evaluation.
       This catches flat triangle normals and an 8x8 coarse frame that never completes. */
    GRE_Camera4d camera=g_activeCamera->camera;gre_camera4d rayCamera=*camera;rayCamera.img=g_target;
    GRE_Material material=YMGRE_Material_Find(&g_importContext.MaterialList,sphere->mesh->materiaName);SMOOTH_CHECK(material);
    gre_fvector4d center={0,18,0,1};unsigned samples=0,wrong=0,changed=0,stale=0;
    GYrect imageArea;YMGUI_Obj_GetAbsArea(g_image,&imageArea);
    for(int y=100;y<548;y+=3)for(int x=60;x<g_target.width-60;x+=3) {
        gre_ray ray;gre_ray_hit hit;YMGRE_Ray_FromCameraPixel(&rayCamera,x,y,&ray);
        if(!YMGRE_Ray_IntersectSphere(&ray,&center,18,.1f,1000,&hit))continue;
        GRErgb24 base={0},spec={0};gre_polygon4d surface={0};
        gre_fvector4d position={hit.position.x-camera->pos.x,hit.position.y-camera->pos.y,hit.position.z-camera->pos.z,1};
        for(GRE_ListNode n=g_lightsList.listhead;n;n=n->next) {
            gre_light4d light=*(GRE_Light4d)n->data;light.proper.shadowK=0;
            light.proper.pos_=(gre_fvector4d){light.pos.x-camera->pos.x,light.pos.y-camera->pos.y,light.pos.z-camera->pos.z,1};
            surface.planeColor=light.type==GRE_GlobalLight?material->ambient:material->diffuse;
            YMGRE_PolygonLighting_ComponentsAdvanced(&surface,&position,&hit.normal,&light,&base,&spec,sphere->mirrorKs,(uint8)sphere->specularPower,material->specular);
        }
        GRErgb24 expected={GREMin(255,base.R+spec.R),GREMin(255,base.G+spec.G),GREMin(255,base.B+spec.B)};
        size_t i=(size_t)y*g_target.width+x;GRErgb24 actual=GRE_FramePixel_To_RGB24(g_target.data[i]);
        if(abs(expected.R-actual.R)>1||abs(expected.G-actual.G)>1||abs(expected.B-actual.B)>1)wrong++;
        if(memcmp(g_target.data+i,faceted+i,sizeof(GRE_FramePixel)))changed++;
        GYcolor shown=GY_PxToColor(smoothScreen[(size_t)(imageArea.y+y)*display->hor_res+imageArea.x+x]);
        if(((shown>>16)&255)!=actual.R || ((shown>>8)&255)!=actual.G || (shown&255)!=actual.B)stale++;
        samples++;
    }
    printf("Smooth sphere: samples=%u wrong=%u stale-display=%u changed-from-mesh=%u progress=%d artifacts=%s\n",samples,wrong,stale,changed,g_rayProgress,root);
    SMOOTH_CHECK(samples>1000 && wrong==0 && stale==0 && changed>samples/2);
    rendererSaveScreen(root,"analytic");
    for(int i=0;i<12;i++){renderScene();SMOOTH_CHECK(g_rayProgress==100);}
    uint32 history=g_historyCount;rendererClickOption(0);
    SMOOTH_CHECK((rayButton->state&GY_STATE_Hidden) && sphere->analytic && history==g_historyCount);
    rendererClickOption(1);SMOOTH_CHECK(!(rayButton->state&GY_STATE_Hidden) && smoothFinish());
    display->flush_cb=smoothOriginalFlush;free(smoothScreen);smoothScreen=NULL;
    free(faceted);
#undef SMOOTH_CHECK
    return 0;
}
