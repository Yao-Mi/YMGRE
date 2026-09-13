/* Exercise real dropdown events, retain scene state, and save inspectable viewport output. */
static int rendererFinish(void)
{
    for(int i=0;i<1000;i++) {
        renderScene();
        if(!g_rendererMode)return 0;
        if(g_rayProgress==100)return 1;
    }
    return 0;
}
static void rendererClickOption(int option)
{
    GYrect area;YMGUI_Obj_GetAbsArea(g_rendererSelector,&area);
    YMGUI_Inject_Pointer(area.x+50,area.y+10,1);YMGUI_Inject_Pointer(area.x+50,area.y+10,0);
    YMGUI_Inject_Pointer(area.x+50,area.y+area.h+option*22+11,1);
    YMGUI_Inject_Pointer(area.x+50,area.y+area.h+option*22+11,0);
}
static void rendererSaveScreen(const char* root,const char* name)
{
    YMGUI_Obj_Invalidate(g_ui->host.context->root);YMGUI_Refresh(g_ui->host.context);
    unsigned size=(unsigned)g_ui->host.display.hor_res*g_ui->host.display.ver_res;
    GRErgb24* screen=malloc(size*sizeof(*screen));if(!screen)return;
    for(unsigned i=0;i<size;i++){GYcolor c=GY_PxToColor(g_ui->host.display.buf1[i]);screen[i]=(GRErgb24){c>>16,c>>8,c};}
    char path[4096];snprintf(path,sizeof(path),"%s/%s.bmp",root,name);
    YMGRE_Image_LoadTo_Bmp_File(path,screen,g_ui->host.display.hor_res,g_ui->host.display.ver_res);free(screen);
}
static int runRendererSelfTest(void)
{
#define RENDER_CHECK(c) do{if(!(c)){fprintf(stderr,"Renderer UI line %d: %s\n",__LINE__,#c);return 1;}}while(0)
    char temporary[]="/tmp/ymgre-renderer-XXXXXX",path[4096],error[256];
    const char* root=getenv("YMGRE_RENDERER_OUTPUT_DIR");if(!root)root=mkdtemp(temporary);RENDER_CHECK(root);
    createNewScene();RENDER_CHECK(g_rendererMode==0&&YMGUI_Dropdown_GetOptionCount(g_rendererSelector)==2);
    g_globalLight->strength=.18f;SceneEditorObject_SyncHandle(g_globalLight);
    SceneEditorPlaceResult result={.kind=SCENE_PLACE_CUBE,.y=15,.scale=1,.color=GY_ARGB(0xFF,92,155,220)};
    strcpy(result.name,"Blue Cube");strcpy(result.type,"立方体");result.mesh=SceneEditorPlace_CreateMesh(&result);RENDER_CHECK(result.mesh);
    SceneEditorPlace_TransformMesh(result.mesh,0,15,0,0,1,0);objectPlaced(&result,NULL);SceneEditorObject* cube=g_selected;
    cube->mirrorKs=cube->mesh->mirrorKs=.6f;cube->specularPower=24;
    result=(SceneEditorPlaceResult){.kind=SCENE_PLACE_PLANE,.scale=3,.detailA=1,.detailB=1,.color=GY_ARGB(0xFF,165,173,184)};
    strcpy(result.name,"Ground");strcpy(result.type,"平面");result.mesh=SceneEditorPlace_CreateMesh(&result);RENDER_CHECK(result.mesh);
    SceneEditorPlace_TransformMesh(result.mesh,0,0,0,0,3,0);objectPlaced(&result,NULL);
    result=(SceneEditorPlaceResult){.kind=SCENE_PLACE_POINT_LIGHT,.x=-40,.y=65,.z=-40,.strength=1.3f,.shadowsEnabled=1,.color=GY_ARGB(0xFF,255,244,228)};
    strcpy(result.name,"Key Light");objectPlaced(&result,NULL);SceneEditorObject* lamp=g_selected;
    g_activeCamera->x=65;g_activeCamera->y=60;g_activeCamera->z=-95;g_activeCamera->targetY=12;
    selectObject(cube,"渲染器切换验收");renderScene();
    size_t bytes=(size_t)g_target.width*g_target.height*sizeof(GRE_FramePixel);
    GRE_FramePixel* original=malloc(bytes);RENDER_CHECK(original);memcpy(original,g_target.data,bytes);
    uint32 history=g_historyCount;uint8 dirty=g_sceneDirty;GRE_Object4d mesh=cube->mesh;
    rendererSaveScreen(root,"raster");rendererClickOption(1);
    RENDER_CHECK(g_rendererMode==1&&YMGUI_Dropdown_GetSelected(g_rendererSelector)==1&&!YMGUI_Dropdown_IsOpen(g_rendererSelector));
    RENDER_CHECK(rendererFinish());RENDER_CHECK(mesh==cube->mesh&&history==g_historyCount&&dirty==g_sceneDirty);
    RENDER_CHECK(memcmp(original,g_target.data,bytes));rendererSaveScreen(root,"ray");
    snprintf(path,sizeof(path),"%s/comparison.scene",root);RENDER_CHECK(writeCurrentScene(path,error,sizeof(error)));
    /* A renderer switch is a viewport preference, not a scene/material edit. */
    rendererClickOption(0);renderScene();RENDER_CHECK(g_rendererMode==0&&!memcmp(original,g_target.data,bytes));free(original);
    rendererClickOption(1);RENDER_CHECK(rendererFinish());
    g_rightPanel->state|=GY_STATE_Hidden;layoutPanels();RENDER_CHECK(rendererFinish()&&g_target.width==780);
    g_rightPanel->state&=~GY_STATE_Hidden;layoutPanels();RENDER_CHECK(rendererFinish());
    SceneEditorObject_Translate(cube,10,0,0);RENDER_CHECK(rendererFinish());
    lamp->strength=.4f;SceneEditorObject_SyncHandle(lamp);RENDER_CHECK(rendererFinish());
    /* Existing Tank assets use the same imported material and all submesh traversal. */
    createNewScene();RENDER_CHECK(g_rendererMode==1&&importMesh(YMGRE_TANK_FIXTURE,0,NULL));
    g_activeCamera->x=8;g_activeCamera->y=6;g_activeCamera->z=-12;g_activeCamera->targetY=1;g_activeCamera->targetZ=-1;
    RENDER_CHECK(rendererFinish());rendererSaveScreen(root,"tank-ray");
    printf("Renderer switch PASS: real dropdown, unchanged scene/history, raster restoration, resize, edits, imported Tank. Artifacts: %s\n",root);
#undef RENDER_CHECK
    return 0;
}
