static SceneEditorObject* opticsObject(SceneEditorPlaceKind kind,const char* name,float x,float y,float z,float scale,GYcolor color)
{
    SceneEditorPlaceResult p={.kind=kind,.x=x,.y=y,.z=z,.scale=scale,.color=color,.detailA=10,.detailB=16};
    snprintf(p.name,sizeof(p.name),"%s",name);snprintf(p.type,sizeof(p.type),"%s",kind==SCENE_PLACE_SPHERE?"球体":kind==SCENE_PLACE_CYLINDER?"圆柱":"网格");
    p.mesh=SceneEditorPlace_CreateMesh(&p);if(!p.mesh)return NULL;
    SceneEditorPlace_TransformMesh(p.mesh,x,y,z,0,scale,0);objectPlaced(&p,NULL);return g_selected;
}
static SceneEditorObject* opticsFind(const char* name)
{for(int i=0;i<32;i++)if(g_scene[i].active&&!strcmp(g_scene[i].name,name))return g_scene+i;return NULL;}
static void opticsClick(int x,int y){YMGUI_Inject_Pointer(x,y,1);YMGUI_Inject_Pointer(x,y,0);}
static void opticsUiMaterial(int mode,int analytic,int apply)
{
    opticsClick(900,756); /* sphere/cylinder material button beneath topology */
    opticsClick(530,240);opticsClick(530,256+mode*22+11);
    opticsClick(530,416);opticsClick(530,432+analytic*22+11);
    opticsClick(apply?650:550,588);
}
static int runOpticsSelfTest(void)
{
#define OPTICS_CHECK(c) do{if(!(c)){fprintf(stderr,"Optics line %d: %s\n",__LINE__,#c);return 1;}}while(0)
    char temporary[]="/tmp/ymgre-optics-XXXXXX",source[4096],bakedSource[4096],meshPath[4096],error[256];
    const char* root=getenv("YMGRE_OPTICS_OUTPUT_DIR");if(!root)root=mkdtemp(temporary);OPTICS_CHECK(root);
    char* oldRoot=getenv("YMGRE_BAKE_DIR")?strdup(getenv("YMGRE_BAKE_DIR")):NULL;setenv("YMGRE_BAKE_DIR",root,1);
    createNewScene();rendererClickOption(1);g_globalLight->strength=.3f;SceneEditorObject_SyncHandle(g_globalLight);
    SceneEditorObject* floor=opticsObject(SCENE_PLACE_PLANE,"Ground",0,0,20,4,0xFFAAAAAA);OPTICS_CHECK(floor);
    SceneEditorObject* mirror=opticsObject(SCENE_PLACE_SPHERE,"Mirror Sphere",-25,18,0,1,0xFFFFFFFF);OPTICS_CHECK(mirror);
    uint32 history=g_historyCount;opticsUiMaterial(1,1,0);OPTICS_CHECK(!mirror->rayType&&!mirror->analytic&&g_historyCount==history);
    opticsUiMaterial(1,1,1);OPTICS_CHECK(mirror->rayType==1&&mirror->analytic==1&&g_historyCount==history+1);
    SceneEditorObject* glass=opticsObject(SCENE_PLACE_SPHERE,"Glass Sphere",24,18,0,1,0xFFFFFFFF);OPTICS_CHECK(glass);
    opticsUiMaterial(2,1,1);OPTICS_CHECK(glass->rayType==2&&glass->analytic==1&&glass->ior==1.5f);
    SceneEditorObject* column=opticsObject(SCENE_PLACE_CYLINDER,"Orange Cylinder",-6,16,48,1,0xFFE7913C);OPTICS_CHECK(column);
    opticsUiMaterial(0,1,1);OPTICS_CHECK(column->analytic);
    SceneEditorObject_SetRotationAxis(column,2,15);
    SceneEditorObject* cube=opticsObject(SCENE_PLACE_CUBE,"Blue Cube",40,15,48,1,0xFF287ADA);OPTICS_CHECK(cube);
    SceneEditorPlaceResult lamp={.kind=SCENE_PLACE_POINT_LIGHT,.x=-45,.y=90,.z=-55,.strength=1.6f,.shadowsEnabled=1,.color=0xFFFFFFFF};
    strcpy(lamp.name,"Key Light");objectPlaced(&lamp,NULL);
    g_activeCamera->x=75;g_activeCamera->y=62;g_activeCamera->z=-125;g_activeCamera->targetY=15;g_activeCamera->targetZ=15;
    selectObject(glass,"镜面与玻璃：解析球体、解析圆柱、三角网格共用场景");
    rendererClickOption(1);OPTICS_CHECK(rendererFinish());rendererSaveScreen(root,"optics");
    opticsClick(900,756);rendererSaveScreen(root,"material-panel");opticsClick(550,588);
    snprintf(source,sizeof(source),"%s/optics.scene",root);OPTICS_CHECK(writeCurrentScene(source,error,sizeof(error)));
    size_t bytes=(size_t)g_target.width*g_target.height*sizeof(GRE_FramePixel);GRE_FramePixel* before=malloc(bytes);OPTICS_CHECK(before);memcpy(before,g_target.data,bytes);
    loadSceneFromPath(source,NULL);OPTICS_CHECK(!strcmp(source,g_currentScenePath));
    mirror=opticsFind("Mirror Sphere");glass=opticsFind("Glass Sphere");column=opticsFind("Orange Cylinder");
    OPTICS_CHECK(mirror&&glass&&column&&mirror->rayType==1&&glass->rayType==2&&glass->analytic&&column->analytic);
    selectObject(glass,"重新打开场景");OPTICS_CHECK(rendererFinish());OPTICS_CHECK(!memcmp(before,g_target.data,bytes));free(before);
    /* Duplicating keeps independent material/geometry parameters; undo restores the scene metadata. */
    duplicateObject(glass,NULL);OPTICS_CHECK(g_selected!=glass&&g_selected->rayType==2&&g_selected->analytic&&g_selected->ior==glass->ior);
    deleteObject(g_selected,NULL);glass=opticsFind("Glass Sphere");selectObject(glass,"玻璃网格导出");renderScene();
    showExportDirectory(1);OPTICS_CHECK(YMGUI_FileDialog_Navigate(g_bakeExportDialog,root));OPTICS_CHECK(YMGUI_FileDialog_Confirm(g_bakeExportDialog));
    snprintf(meshPath,sizeof(meshPath),"%s",YMGUI_TextInput_GetText(g_bakeExportPath));OPTICS_CHECK(access(meshPath,R_OK)==0);
    gre_scence imported={0};GRE_Object4d mesh=YMGRE_LoadOgreMeshAndMaterial(&imported,meshPath);OPTICS_CHECK(mesh);
    GRE_Material material=YMGRE_Material_Find(&imported.MaterialList,mesh->materiaName);
    OPTICS_CHECK(material&&material->advanced&&material->advanced->rayType==2&&fabsf(material->advanced->ior-1.5f)<.001f);
    OPTICS_CHECK(mesh->polygonNum==glass->mesh->polygonNum);YMGRE_Free_Object(mesh);YMGRE_List_Clear(&imported.MaterialList,YMGRE_Free_Material);
    OPTICS_CHECK(importMesh(meshPath,0,NULL));SceneEditorObject* importedA=g_selected;
    OPTICS_CHECK(importMesh(meshPath,0,NULL));SceneEditorObject* importedB=g_selected;renderScene();
    GRE_Material a=YMGRE_Material_Find(&g_importContext.MaterialList,importedA->mesh->materiaName);
    GRE_Material b=YMGRE_Material_Find(&g_importContext.MaterialList,importedB->mesh->materiaName);
    OPTICS_CHECK(a&&b&&a!=b&&a->advanced->rayType==2&&b->advanced->rayType==2);
    importedA->rayType=1;importedA->reflectivity=.4f;renderScene();
    OPTICS_CHECK(a->advanced->rayType==1&&b->advanced->rayType==2);
    OPTICS_CHECK(fabsf(importedA->mirrorKs-glass->mirrorKs)<.001f);
    deleteObject(importedA,NULL);deleteObject(importedB,NULL);selectObject(glass,"玻璃颜色烘焙");
    YMGUI_Checkbox_SetChecked(g_bakeLighting,0);bakeObject(glass,NULL);
    OPTICS_CHECK(glass->mesh->lightmap&&glass->mesh->lightmap->materialOnly);
    gre_scence colorContext={0};mesh=YMGRE_LoadOgreMeshAndMaterial(&colorContext,g_bakedModelPath);OPTICS_CHECK(mesh);
    material=YMGRE_Material_Find(&colorContext.MaterialList,mesh->materiaName);
    OPTICS_CHECK(material&&!material->unlit&&material->advanced&&material->advanced->rayType==2);
    YMGRE_Free_Object(mesh);YMGRE_List_Clear(&colorContext.MaterialList,YMGRE_Free_Material);
    /* Baking captures the tessellated surface and exports the same Ogre format with lighting off. */
    int polygons=glass->mesh->polygonNum;YMGUI_Checkbox_SetChecked(g_bakeLighting,1);bakeObject(glass,NULL);
    OPTICS_CHECK(glass->mesh->lightmap&&glass->mesh->lightmap->colorsBaked&&glass->rayBakeSignature&&glass->analytic);
    snprintf(meshPath,sizeof(meshPath),"%s",g_bakedModelPath);OPTICS_CHECK(access(meshPath,R_OK)==0);
    OPTICS_CHECK(rendererFinish());rendererSaveScreen(root,"baked-glass");
    snprintf(bakedSource,sizeof(bakedSource),"%s/baked-source.scene",root);OPTICS_CHECK(writeCurrentScene(bakedSource,error,sizeof(error)));
    uint64_t signature=glass->rayBakeSignature;loadSceneFromPath(bakedSource,NULL);glass=opticsFind("Glass Sphere");
    OPTICS_CHECK(glass&&glass->analytic&&glass->rayType==2&&glass->rayBakeSignature==signature&&glass->mesh->lightmap);
    renderScene();OPTICS_CHECK(glass->mesh->lightmap);g_activeCamera->x+=5;renderScene();OPTICS_CHECK(glass->mesh->lightmap);
    cube=opticsFind("Blue Cube");SceneEditorObject_Translate(cube,2,0,0);renderScene();OPTICS_CHECK(!glass->mesh->lightmap);
    createNewScene();OPTICS_CHECK(importMesh(meshPath,0,NULL));GRE_Object4d baked=g_selected->mesh;
    material=YMGRE_Material_Find(&g_importContext.MaterialList,baked->materiaName);
    OPTICS_CHECK(material&&material->unlit&&(!material->advanced||!material->advanced->rayType)&&baked->polygonNum==polygons);
    for(int i=0;i<32;i++)if(g_scene[i].active&&g_scene[i].kind==SCENE_OBJECT_LIGHT){g_scene[i].strength=0;SceneEditorObject_SyncHandle(g_scene+i);}
    OPTICS_CHECK(rendererFinish());
    if(oldRoot){setenv("YMGRE_BAKE_DIR",oldRoot,1);free(oldRoot);}else unsetenv("YMGRE_BAKE_DIR");
    printf("Optics PASS: UI apply/cancel, mirror/glass/analytic scene roundtrip, duplicate, independent imported materials, color-only export, fixed-view bake, source reload, invalidation, unlit mesh reimport. Artifacts: %s\n",root);
#undef OPTICS_CHECK
    return 0;
}
