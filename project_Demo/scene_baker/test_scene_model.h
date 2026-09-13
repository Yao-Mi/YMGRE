/* Integration checks run inside the editor so toolbar routing and reimport use the real UI. */
static int runTankRoundtrip(void)
{
#define TANK_CHECK(c) do {if(!(c)){fprintf(stderr,"Tank roundtrip line %d: %s\n",__LINE__,#c);return 1;}}while(0)
    char temporary[]="/tmp/ymgre-tank-XXXXXX",error[256],exported[4096],preview[4096];
    const char* root=getenv("YMGRE_TANK_OUTPUT_DIR");if(!root)root=mkdtemp(temporary);TANK_CHECK(root);
    char absolute[4096];TANK_CHECK(realpath(root,absolute));root=absolute;
    createNewScene();TANK_CHECK(SceneEditorImport_Validate(YMGRE_TANK_FIXTURE,error,sizeof(error)));
    TANK_CHECK(importMesh(YMGRE_TANK_FIXTURE,0,NULL));
    SceneEditorObject* tank=g_selected;
    TANK_CHECK(tank && tank->mesh->pointNum==225 && tank->mesh->polygonNum==75 && tank->mesh->importedUvCount==6);
    TANK_CHECK(SceneEditorImport_ValidateLoadedMaterials(YMGRE_TANK_FIXTURE,tank->mesh,error,sizeof(error)));
    g_activeCamera->x=8;g_activeCamera->y=6;g_activeCamera->z=-12;
    g_activeCamera->targetX=0;g_activeCamera->targetY=1;g_activeCamera->targetZ=-1;
    g_globalLight->strength=0.9f;SceneEditorObject_SyncHandle(g_globalLight);
    renderScene();
    unsigned size=(unsigned)g_target.width*g_target.height;
    GRErgb24* before=malloc(size*sizeof(*before));GRErgb24* after=malloc(size*sizeof(*after));TANK_CHECK(before && after);
    for(unsigned i=0;i<size;i++)before[i]=GRE_FramePixel_To_RGB24(g_target.data[i]);
    snprintf(preview,sizeof(preview),"%s/imported.bmp",root);
    YMGRE_Image_LoadTo_Bmp_File(preview,before,g_target.width,g_target.height);
    /* Unbaked meshes can be exported; cancellation has no effect. */
    YMGUI_Inject_Pointer(860,27,1);YMGUI_Inject_Pointer(860,27,0);
    TANK_CHECK(YMGUI_FileDialog_IsShown(g_bakeExportDialog));
    YMGUI_FileDialog_Close(g_bakeExportDialog);bakeExportResult(g_bakeExportDialog,0,NULL,NULL);
    TANK_CHECK(g_selected==tank && !tank->mesh->lightmap);
    YMGUI_Inject_Pointer(860,27,1);YMGUI_Inject_Pointer(860,27,0);
    TANK_CHECK(YMGUI_FileDialog_IsShown(g_bakeExportDialog));
    TANK_CHECK(YMGUI_FileDialog_Navigate(g_bakeExportDialog,root));
    TANK_CHECK(YMGUI_FileDialog_Confirm(g_bakeExportDialog));
    snprintf(exported,sizeof(exported),"%s",YMGUI_TextInput_GetText(g_bakeExportPath));
    TANK_CHECK(strstr(exported,"/Tank1_Head.mesh") && access(exported,R_OK)==0);
    TANK_CHECK(SceneEditorImport_Validate(exported,error,sizeof(error)));
    /* Independent material contexts prevent name collisions from masking missing resources. */
    gre_scence originalContext={0},outputContext={0};
    GRE_Object4d original=YMGRE_LoadOgreMeshAndMaterial(&originalContext,YMGRE_TANK_FIXTURE);
    GRE_Object4d restored=YMGRE_LoadOgreMeshAndMaterial(&outputContext,exported);
    TANK_CHECK(original && restored && !original->nextObject && !restored->nextObject);
    TANK_CHECK(original->pointNum==restored->pointNum && original->polygonNum==restored->polygonNum);
    TANK_CHECK(original->importedUvCount==restored->importedUvCount);
    for(int i=0;i<original->pointNum;i++) {
        gre_vertex4d a=original->pointList[i],b=restored->pointList[i];
        TANK_CHECK(fabsf(a.pos.x-b.pos.x)<0.000001f && fabsf(a.pos.y-b.pos.y)<0.000001f && fabsf(a.pos.z-b.pos.z)<0.000001f);
        TANK_CHECK(a.u==b.u && a.v==b.v);
    }
    TANK_CHECK(!memcmp(original->importedUvs,restored->importedUvs,original->pointNum*6*2*sizeof(float32)));
    TANK_CHECK(!memcmp(original->importedNormals,restored->importedNormals,original->pointNum*sizeof(gre_fvector4d)));
    for(int i=0;i<original->polygonNum;i++)TANK_CHECK(!memcmp(original->polygonList[i].index,restored->polygonList[i].index,3*sizeof(uint16)));
    TANK_CHECK(!strcmp(original->materiaName,restored->materiaName));
    GRE_Material a=YMGRE_Material_Find(&originalContext.MaterialList,original->materiaName);
    GRE_Material b=YMGRE_Material_Find(&outputContext.MaterialList,restored->materiaName);
    TANK_CHECK(a && b && a->pixel && b->pixel && a->width==b->width && a->height==b->height);
    TANK_CHECK(!memcmp(a->pixel,b->pixel,(size_t)a->width*a->height*sizeof(GRErgb24)));
    TANK_CHECK(!memcmp(&a->ambient,&b->ambient,sizeof(GRErgb24)) && !memcmp(&a->diffuse,&b->diffuse,sizeof(GRErgb24)) && !memcmp(&a->specular,&b->specular,sizeof(GRErgb24)));
    unsigned textureWidth=a->width,textureHeight=a->height;
    YMGRE_Free_Object(original);YMGRE_Free_Object(restored);
    YMGRE_List_Clear(&originalContext.MaterialList,YMGRE_Free_Material);
    YMGRE_List_Clear(&outputContext.MaterialList,YMGRE_Free_Material);
    createNewScene();TANK_CHECK(importMesh(exported,0,NULL));
    g_activeCamera->x=8;g_activeCamera->y=6;g_activeCamera->z=-12;
    g_activeCamera->targetX=0;g_activeCamera->targetY=1;g_activeCamera->targetZ=-1;
    g_globalLight->strength=0.9f;SceneEditorObject_SyncHandle(g_globalLight);renderScene();
    for(unsigned i=0;i<size;i++)after[i]=GRE_FramePixel_To_RGB24(g_target.data[i]);
    snprintf(preview,sizeof(preview),"%s/reimported.bmp",root);
    YMGRE_Image_LoadTo_Bmp_File(preview,after,g_target.width,g_target.height);
    unsigned changed=0;for(unsigned i=0;i<size;i++)if(memcmp(before+i,after+i,sizeof(*before)))changed++;
    TANK_CHECK(changed==0);free(before);free(after);
    /* Failed writes preserve the already exported resource and current geometry. */
    char blocked[4096],failed[4096];snprintf(blocked,sizeof(blocked),"%s/not-a-directory",root);
    FILE* f=fopen(blocked,"w");TANK_CHECK(f);fclose(f);
    TANK_CHECK(!SceneModel_Export(g_selected->mesh,&g_importContext.MaterialList,NULL,NULL,blocked,"Tank1_Head",failed,sizeof(failed),error,sizeof(error)));
    TANK_CHECK(access(exported,R_OK)==0);unlink(blocked);
    snprintf(preview,sizeof(preview),"%s/Tank1_Head.scene",root);TANK_CHECK(writeCurrentScene(preview,error,sizeof(error)));
    YMGUI_TextInput_SetText(g_bakeExportPath,exported);
    setStatus("Tank1_Head 导出并重新导入成功：225 顶点 / 75 三角形 / 6 套 UV");
    YMGUI_Obj_Invalidate(g_ui->host.context->root);YMGUI_Refresh(g_ui->host.context);
    unsigned screenSize=(unsigned)g_ui->host.display.hor_res*g_ui->host.display.ver_res;
    GRErgb24* screen=malloc(screenSize*sizeof(*screen));TANK_CHECK(screen);
    for(unsigned i=0;i<screenSize;i++){GYcolor c=GY_PxToColor(g_ui->host.display.buf1[i]);screen[i]=(GRErgb24){c>>16,c>>8,c};}
    snprintf(preview,sizeof(preview),"%s/editor.bmp",root);
    YMGRE_Image_LoadTo_Bmp_File(preview,screen,g_ui->host.display.hor_res,g_ui->host.display.ver_res);free(screen);
    printf("Tank roundtrip: PASS (225 vertices, 75 triangles, 6 UV channels, authored normals, %ux%u texture identical; rendered pixel differences=%u)\nmesh: %s\nartifacts: %s\n",textureWidth,textureHeight,changed,exported,root);
#undef TANK_CHECK
    return 0;
}

static int runColorBakeSelfTest(void)
{
#define COLOR_CHECK(c) do {if(!(c)){fprintf(stderr,"Color bake line %d: %s\n",__LINE__,#c);return 1;}}while(0)
    char temporary[]="/tmp/ymgre-color-bake-XXXXXX",scene[4096],path[4096],error[256],preview[4096];
    const char* root=getenv("YMGRE_COLOR_BAKE_DIR");if(!root)root=mkdtemp(temporary);COLOR_CHECK(root);
    const char* priorRoot=getenv("YMGRE_BAKE_DIR");char* savedRoot=priorRoot?strdup(priorRoot):NULL;
    setenv("YMGRE_BAKE_DIR",root,1);
    for(int mode=0;mode<2;mode++) {
        createNewScene();
        g_globalLight->strength=.2f;SceneEditorObject_SyncHandle(g_globalLight);
        SceneEditorPlaceResult cube={.kind=SCENE_PLACE_CUBE,.y=15,.scale=1,.color=GY_ARGB(0xFF,92,155,220)};
        snprintf(cube.name,sizeof(cube.name),"Blue Cube");snprintf(cube.type,sizeof(cube.type),"立方体");
        cube.mesh=SceneEditorPlace_CreateMesh(&cube);COLOR_CHECK(cube.mesh);
        SceneEditorPlace_TransformMesh(cube.mesh,0,15,0,0,1,0);objectPlaced(&cube,NULL);
        SceneEditorObject* object=g_selected;
        SceneEditorPlaceResult lamp={.kind=SCENE_PLACE_POINT_LIGHT,.x=-40,.y=65,.z=-45,.strength=.8f,.color=GY_ARGB(0xFF,255,255,255)};
        snprintf(lamp.name,sizeof(lamp.name),"Bake Lamp");objectPlaced(&lamp,NULL);selectObject(object,"蓝色烘焙测试");
        g_activeCamera->x=65;g_activeCamera->y=55;g_activeCamera->z=-90;g_activeCamera->targetY=15;renderScene();
        if(YMGUI_Checkbox_GetChecked(g_bakeLighting)!=mode) {
            YMGUI_Inject_Pointer(30,718,1);YMGUI_Inject_Pointer(30,718,0);
        }
        COLOR_CHECK(YMGUI_Checkbox_GetChecked(g_bakeLighting)==mode);
        YMGUI_Inject_Pointer(760,27,1);YMGUI_Inject_Pointer(760,27,0);
        COLOR_CHECK(object->mesh->lightmap && object->mesh->lightmap->materialOnly==!mode);
        COLOR_CHECK(object->mesh->lightmap->enabled==mode);
        int exportX=860;
        YMGUI_Inject_Pointer(exportX,27,1);YMGUI_Inject_Pointer(exportX,27,0);
        COLOR_CHECK(YMGUI_FileDialog_IsShown(g_bakeExportDialog));
        COLOR_CHECK(YMGUI_FileDialog_Navigate(g_bakeExportDialog,root));
        COLOR_CHECK(YMGUI_FileDialog_Confirm(g_bakeExportDialog));
        snprintf(path,sizeof(path),"%s",YMGUI_TextInput_GetText(g_bakeExportPath));
        COLOR_CHECK(strstr(path,"/baked_model.mesh") && access(path,R_OK)==0);
        COLOR_CHECK(SceneEditorImport_Validate(path,error,sizeof(error)));
        snprintf(scene,sizeof(scene),"%s/%s-source.scene",root,mode?"with-light":"color-only");
        COLOR_CHECK(writeCurrentScene(scene,error,sizeof(error)));loadSceneFromPath(scene,NULL);
        COLOR_CHECK(!strcmp(g_currentScenePath,scene));
        object=NULL;for(int i=0;i<32;i++)if(g_scene[i].active && g_scene[i].kind==SCENE_OBJECT_MESH)object=&g_scene[i];
        COLOR_CHECK(object && object->mesh->lightmap && object->mesh->lightmap->materialOnly==!mode);
        /* A material-only bake is independent of scene light edits. */
        GRE_Lightmap previous=object->mesh->lightmap;
        g_globalLight->strength+=.1f;SceneEditorObject_SyncHandle(g_globalLight);renderScene();
        COLOR_CHECK(mode?!object->mesh->lightmap:object->mesh->lightmap==previous);
        createNewScene();COLOR_CHECK(importMesh(path,0,NULL));
        object=g_selected;COLOR_CHECK(object->mesh->pointNum==36 && object->mesh->polygonNum==12);
        COLOR_CHECK(object->mesh->importedUvCount==1 && !object->mesh->lightmap);
        GRE_Material material=YMGRE_Material_Find(&g_importContext.MaterialList,object->mesh->materiaName);
        COLOR_CHECK(material && material->unlit==mode && material->width==256 && material->height==256);
        COLOR_CHECK(material->diffuse.R==255 && material->diffuse.G==255 && material->diffuse.B==255);
        unsigned blue=0;int minimum=255,maximum=0;
        for(unsigned i=0;i<256*256;i++) {
            GRErgb24 c=material->pixel[i];if(!c.R && !c.G && !c.B)continue;
            COLOR_CHECK(c.B>c.G && c.G>c.R);blue++;
            if(!mode)COLOR_CHECK(c.R==92 && c.G==155 && c.B==220);
            if(c.B<minimum)minimum=c.B;if(c.B>maximum)maximum=c.B;
        }
        COLOR_CHECK(blue>40000);if(mode)COLOR_CHECK(maximum-minimum>30);
        snprintf(preview,sizeof(preview),"%s/%s-texture.bmp",root,mode?"with-light":"color-only");
        YMGRE_Image_LoadTo_Bmp_File(preview,material->pixel,material->width,material->height);
        g_activeCamera->x=65;g_activeCamera->y=55;g_activeCamera->z=-90;g_activeCamera->targetY=15;
        g_globalLight->strength=.2f;SceneEditorObject_SyncHandle(g_globalLight);renderScene();
        unsigned size=(unsigned)g_target.width*g_target.height;
        GRErgb24* pixels=malloc(size*sizeof(*pixels));COLOR_CHECK(pixels);
        for(unsigned i=0;i<size;i++)pixels[i]=GRE_FramePixel_To_RGB24(g_target.data[i]);
        g_globalLight->strength=.9f;SceneEditorObject_SyncHandle(g_globalLight);renderScene();
        unsigned changed=0;for(unsigned i=0;i<size;i++){GRErgb24 c=GRE_FramePixel_To_RGB24(g_target.data[i]);if(memcmp(&c,&pixels[i],sizeof(c)))changed++;}
        COLOR_CHECK(mode?changed==0:changed>500);
        for(unsigned i=0;i<size;i++)pixels[i]=GRE_FramePixel_To_RGB24(g_target.data[i]);
        snprintf(preview,sizeof(preview),"%s/%s-preview.bmp",root,mode?"with-light":"color-only");
        YMGRE_Image_LoadTo_Bmp_File(preview,pixels,g_target.width,g_target.height);free(pixels);
        snprintf(scene,sizeof(scene),"%s/%s-exported.scene",root,mode?"with-light":"color-only");
        COLOR_CHECK(writeCurrentScene(scene,error,sizeof(error)));
        YMGUI_TextInput_SetText(g_bakeExportPath,path);
        setStatus(mode?"含光照烘焙：彩色贴图已带明暗，重导入后不重复打光":"不含光照烘焙：蓝色已写入贴图，重导入后接受实时光照");
        YMGUI_Obj_Invalidate(g_ui->host.context->root);YMGUI_Refresh(g_ui->host.context);
        unsigned screenSize=(unsigned)g_ui->host.display.hor_res*g_ui->host.display.ver_res;
        GRErgb24* screen=malloc(screenSize*sizeof(*screen));COLOR_CHECK(screen);
        for(unsigned i=0;i<screenSize;i++){GYcolor c=GY_PxToColor(g_ui->host.display.buf1[i]);screen[i]=(GRErgb24){c>>16,c>>8,c};}
        snprintf(preview,sizeof(preview),"%s/%s-editor.bmp",root,mode?"with-light":"color-only");
        YMGRE_Image_LoadTo_Bmp_File(preview,screen,g_ui->host.display.hor_res,g_ui->host.display.ver_res);free(screen);
        printf("Color bake %s: PASS (blue atlas, UV0, Ogre reload, lighting %s, changed pixels=%u)\nmesh: %s\n",mode?"with light":"material only",mode?"off":"on",changed,path);
    }
    if(savedRoot){setenv("YMGRE_BAKE_DIR",savedRoot,1);free(savedRoot);}else unsetenv("YMGRE_BAKE_DIR");
    YMGUI_Checkbox_SetChecked(g_bakeLighting,1);
    printf("Color bake artifacts: %s\n",root);
#undef COLOR_CHECK
    return 0;
}

static int runSpecularBakeSelfTest(void)
{
#define SPEC_CHECK(c) do {if(!(c)){fprintf(stderr,"Specular bake line %d: %s\n",__LINE__,#c);return 1;}}while(0)
    char temp[]="/tmp/ymgre-spec-bake-XXXXXX",error[256],sourcePath[4096],meshPath[4096],imagePath[4096];
    const char* root=getenv("YMGRE_SPECULAR_BAKE_DIR");if(!root)root=mkdtemp(temp);SPEC_CHECK(root);
    char* oldRoot=getenv("YMGRE_BAKE_DIR")?strdup(getenv("YMGRE_BAKE_DIR")):NULL;
    setenv("YMGRE_BAKE_DIR",root,1);createNewScene();
    g_globalLight->strength=.1f;SceneEditorObject_SyncHandle(g_globalLight);
    SceneEditorPlaceResult cube={.kind=SCENE_PLACE_CUBE,.y=15,.scale=1,.color=GY_ARGB(0xFF,92,155,220)};
    snprintf(cube.name,sizeof(cube.name),"Glossy Blue Cube");snprintf(cube.type,sizeof(cube.type),"立方体");
    cube.mesh=SceneEditorPlace_CreateMesh(&cube);SPEC_CHECK(cube.mesh);
    SceneEditorPlace_TransformMesh(cube.mesh,0,15,0,0,1,0);objectPlaced(&cube,NULL);
    SceneEditorObject* object=g_selected;object->mirrorKs=object->mesh->mirrorKs=.9f;object->specularPower=24;
    g_activeCamera->x=40;g_activeCamera->y=35;g_activeCamera->z=-70;g_activeCamera->targetY=15;
    SceneEditorPlaceResult lamp={.kind=SCENE_PLACE_POINT_LIGHT,.x=-40,.y=-5,.z=-70,.strength=.7f,.color=GY_ARGB(0xFF,255,255,255)};
    snprintf(lamp.name,sizeof(lamp.name),"Highlight Lamp");objectPlaced(&lamp,NULL);selectObject(object,"高光烘焙验收");renderScene();
    if(getenv("YMGRE_BAKE_COMPARE_STRESS")) {
        g_globalLight->strength=.7f;SceneEditorObject_SyncHandle(g_globalLight);
        for(int i=0;i<32;i++)if(g_scene[i].active && g_scene[i].kind==SCENE_OBJECT_LIGHT && g_scene[i].lightType==GRE_PointLight) {
            g_scene[i].strength=5;SceneEditorObject_SyncHandle(&g_scene[i]);
        }
        object->mirrorKs=object->mesh->mirrorKs=.2f;object->specularPower=30;renderScene();
    }
    unsigned liveSize=(unsigned)g_target.width*g_target.height;
    GRErgb24* live=malloc(liveSize*sizeof(*live));SPEC_CHECK(live);
    for(unsigned i=0;i<liveSize;i++)live[i]=GRE_FramePixel_To_RGB24(g_target.data[i]);
    snprintf(imagePath,sizeof(imagePath),"%s/before-bake.bmp",root);YMGRE_Image_LoadTo_Bmp_File(imagePath,live,g_target.width,g_target.height);
    YMGUI_Checkbox_SetChecked(g_bakeLighting,1);
    YMGUI_Inject_Pointer(760,27,1);YMGUI_Inject_Pointer(760,27,0);
    renderScene();
    unsigned lostHighlights=0,brightPixels=0;unsigned long totalError=0;
    for(unsigned i=0;i<liveSize;i++) {
        GRErgb24 c=GRE_FramePixel_To_RGB24(g_target.data[i]);
        if(live[i].R>80 && live[i].B>=live[i].R) {brightPixels++;if(c.R<live[i].R/2)lostHighlights++;}
        totalError+=abs((int)c.R-live[i].R)+abs((int)c.G-live[i].G)+abs((int)c.B-live[i].B);live[i]=c;
    }
    snprintf(imagePath,sizeof(imagePath),"%s/after-bake.bmp",root);YMGRE_Image_LoadTo_Bmp_File(imagePath,live,g_target.width,g_target.height);free(live);
    printf("Bake appearance: bright=%u lost=%u error=%lu\n",brightPixels,lostHighlights,totalError);
    SPEC_CHECK(brightPixels>100 && lostHighlights==0);
    if(getenv("YMGRE_BAKE_COMPARE_STRESS")) {
        SPEC_CHECK(totalError<=liveSize);
        if(oldRoot){setenv("YMGRE_BAKE_DIR",oldRoot,1);free(oldRoot);}else unsetenv("YMGRE_BAKE_DIR");
        printf("compare artifacts: %s\n",root);return 0;
    }
    SPEC_CHECK(object->mesh->lightmap && object->mesh->lightmap->specularPixels);
    GRE_Lightmap map=object->mesh->lightmap;unsigned highlights=0;
    for(unsigned i=0;i<256*256;i++)if(map->specularPixels[i].R>40)highlights++;
    SPEC_CHECK(highlights>100);
    snprintf(meshPath,sizeof(meshPath),"%s",YMGUI_TextInput_GetText(g_bakeExportPath));
    snprintf(imagePath,sizeof(imagePath),"%s/highlight.bmp",root);
    YMGRE_Image_LoadTo_Bmp_File(imagePath,map->specularPixels,256,256);
    snprintf(sourcePath,sizeof(sourcePath),"%s/glossy-source.scene",root);SPEC_CHECK(writeCurrentScene(sourcePath,error,sizeof(error)));
    loadSceneFromPath(sourcePath,NULL);SPEC_CHECK(!strcmp(sourcePath,g_currentScenePath));
    object=NULL;for(int i=0;i<32;i++)if(g_scene[i].active && g_scene[i].kind==SCENE_OBJECT_MESH)object=&g_scene[i];
    SPEC_CHECK(object && object->mesh->lightmap && object->mesh->lightmap->specularPixels);
    map=object->mesh->lightmap;SPEC_CHECK(map->specularPower==24 && fabsf(map->specularKs-.9f)<1e-6f);
    g_activeCamera->x+=10;renderScene();SPEC_CHECK(object->mesh->lightmap==map);
    /* Editing the material's highlight response requires a fresh bake. */
    object->specularPower=8;renderScene();SPEC_CHECK(!object->mesh->lightmap);
    loadSceneFromPath(sourcePath,NULL);
    object=NULL;for(int i=0;i<32;i++)if(g_scene[i].active && g_scene[i].kind==SCENE_OBJECT_MESH)object=&g_scene[i];
    SPEC_CHECK(object && object->mesh->lightmap && object->mesh->lightmap->colorsBaked);
    SceneEditorObject_SetColor(object,GY_ARGB(0xFF,220,80,40));renderScene();SPEC_CHECK(!object->mesh->lightmap);
    /* Reimport only the three ordinary output files; no bake sidecar is attached. */
    createNewScene();SPEC_CHECK(importMesh(meshPath,0,NULL));object=g_selected;
    SPEC_CHECK(!object->mesh->lightmap);
    GRE_Material material=YMGRE_Material_Find(&g_importContext.MaterialList,object->mesh->materiaName);
    SPEC_CHECK(material && material->unlit);
    unsigned neutralBright=0;for(unsigned i=0;i<256*256;i++)if(material->pixel[i].R>100)neutralBright++;
    SPEC_CHECK(neutralBright>100);
    snprintf(imagePath,sizeof(imagePath),"%s/final-texture.bmp",root);
    YMGRE_Image_LoadTo_Bmp_File(imagePath,material->pixel,material->width,material->height);
    g_activeCamera->x=40;g_activeCamera->y=35;g_activeCamera->z=-70;g_activeCamera->targetY=15;renderScene();
    unsigned count=(unsigned)g_target.width*g_target.height;GRErgb24* pixels=malloc(count*sizeof(*pixels));SPEC_CHECK(pixels);
    for(unsigned i=0;i<count;i++)pixels[i]=GRE_FramePixel_To_RGB24(g_target.data[i]);
    YMGRE_List_Clear(&g_lightsList,YMGRE_Free_Light);
    for(int i=0;i<32;i++)if(g_scene[i].active && g_scene[i].kind==SCENE_OBJECT_LIGHT){g_scene[i].active=0;g_scene[i].light=NULL;}
    g_globalLight=NULL;rebuildHierarchyTree();selectObject(object,"无光源：烘焙明暗和高光仍保留");renderScene();
    unsigned differences=0;for(unsigned i=0;i<count;i++){GRErgb24 c=GRE_FramePixel_To_RGB24(g_target.data[i]);if(memcmp(&c,&pixels[i],sizeof(c)))differences++;}
    SPEC_CHECK(differences==0);
    snprintf(imagePath,sizeof(imagePath),"%s/no-lights-preview.bmp",root);YMGRE_Image_LoadTo_Bmp_File(imagePath,pixels,g_target.width,g_target.height);free(pixels);
    snprintf(sourcePath,sizeof(sourcePath),"%s/glossy-no-lights.scene",root);SPEC_CHECK(writeCurrentScene(sourcePath,error,sizeof(error)));
    loadSceneFromPath(sourcePath,NULL);SPEC_CHECK(!strcmp(sourcePath,g_currentScenePath));
    SPEC_CHECK(!g_lightsList.listhead && !g_globalLight);
    object=NULL;for(int i=0;i<32;i++)if(g_scene[i].active && g_scene[i].kind==SCENE_OBJECT_MESH)object=&g_scene[i];
    SPEC_CHECK(object);selectObject(object,"无光源成品场景重开成功");renderScene();
    YMGUI_TextInput_SetText(g_bakeExportPath,meshPath);
    setStatus("高光已写入彩色贴图；当前无光源，烘焙明暗和高光完整保留");
    YMGUI_Obj_Invalidate(g_ui->host.context->root);YMGUI_Refresh(g_ui->host.context);
    unsigned screenSize=(unsigned)g_ui->host.display.hor_res*g_ui->host.display.ver_res;
    GRErgb24* screen=malloc(screenSize*sizeof(*screen));SPEC_CHECK(screen);
    for(unsigned i=0;i<screenSize;i++){GYcolor c=GY_PxToColor(g_ui->host.display.buf1[i]);screen[i]=(GRErgb24){c>>16,c>>8,c};}
    snprintf(imagePath,sizeof(imagePath),"%s/editor.bmp",root);YMGRE_Image_LoadTo_Bmp_File(imagePath,screen,g_ui->host.display.hor_res,g_ui->host.display.ver_res);free(screen);
    if(oldRoot){setenv("YMGRE_BAKE_DIR",oldRoot,1);free(oldRoot);}else unsetenv("YMGRE_BAKE_DIR");
    printf("Specular bake: PASS (highlight texels=%u; source reload; material edits invalidate; camera movement preserves bake; all lights removed, pixel differences=%u)\nmesh: %s\nartifacts: %s\n",highlights,differences,meshPath,root);
#undef SPEC_CHECK
    return 0;
}
