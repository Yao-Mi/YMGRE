#include <SDL.h>
#include <ctype.h>
static GYOBJ uvTestFindButton(GYOBJ parent)
{
    if(parent->area.x==112&&parent->area.y==0&&parent->area.w==88&&parent->area.h==24)return parent;
    for(GYOBJ c=parent->child_head;c;c=c->sibling){GYOBJ found=uvTestFindButton(c);if(found)return found;}
    return NULL;
}
static void uvTestClick(int x,int y)
{YMGUI_Inject_Pointer(x,y,1);YMGUI_Inject_Pointer(x,y,0);}
static int uvTestOpen(void)
{
    GYOBJ button=uvTestFindButton(g_rightPanel);if(!button)return 0;
    GYrect area;YMGUI_Obj_GetAbsArea(button,&area);uvTestClick(area.x+20,area.y+10);return 1;
}
static float* uvTestCapture(GRE_Object4d mesh,int* count)
{
    int n=0;for(GRE_Object4d m=mesh;m;m=m->nextObject)n+=m->polygonNum*6;
    float* values=malloc(n*sizeof(float));if(!values)return NULL;*count=n;int k=0;
    for(GRE_Object4d m=mesh;m;m=m->nextObject)for(int i=0;i<m->polygonNum;i++)for(int j=0;j<3;j++) {
        gre_vertex4d v=m->pointList[m->polygonList[i].index[j]];values[k++]=v.u;values[k++]=v.v;
    }
    return values;
}
static SceneEditorObject* uvTestFindObject(void)
{
    for(int i=0;i<32;i++)if(g_scene[i].active&&g_scene[i].kind==SCENE_OBJECT_MESH)return g_scene+i;
    return NULL;
}
static int uvTestMatches(GRE_Object4d mesh,const float* original,int count)
{
    int n;float* values=uvTestCapture(mesh,&n);if(!values)return 0;
    int ok=n==count;for(int i=0;i<n&&ok;i++)if(fabsf(values[i]-original[i])>1e-5f) {
        fprintf(stderr,"UV reload mismatch %d: %g != %g\n",i,values[i],original[i]);ok=0;
    }
    free(values);return ok;
}
static GYOBJ uvTestWidget(GYOBJ parent,int x,int y,int w,int h)
{
    GYrect a;YMGUI_Obj_GetAbsArea(parent,&a);
    if(a.x==x&&a.y==y&&a.w==w&&a.h==h)return parent;
    for(GYOBJ c=parent->child_head;c;c=c->sibling){GYOBJ found=uvTestWidget(c,x,y,w,h);if(found)return found;}
    return NULL;
}
static int uvTestInput(int x,int y,int w,const char* text)
{
    GYOBJ input=uvTestWidget(YMGUI_Ctx_GetTopLayer(g_rightPanel->ctx),x,y,w,28);
    if(!input)return 0;YMGUI_TextInput_SetText(input,text);return 1;
}
static float uvTestCapRatio(GRE_Object4d mesh,int cap)
{
    float lo=INFINITY,hi=0;
    for(int f=cap;f<mesh->polygonNum;f+=4) {
        GRE_Polygon4d face=mesh->polygonList+f;gre_vertex4d center=mesh->pointList[face->index[0]];
        for(int j=1;j<3;j++){gre_vertex4d p=mesh->pointList[face->index[j]];float radius=hypotf(p.u-center.u,p.v-center.v);lo=fminf(lo,radius);hi=fmaxf(hi,radius);}
    }
    return hi/lo;
}
static uint64_t uvTestPreviewHash(void)
{
    YMGUI_Obj_Invalidate(g_ui->host.context->root);YMGUI_Refresh(g_ui->host.context);
    uint64_t hash=0;
    for(int y=112;y<512;y++)hash=signatureBytes(hash,g_ui->host.display.buf1+y*g_ui->host.display.hor_res+556,400*sizeof(GYpx));
    return hash;
}
#include <time.h>
static int runUvEditorSelfTest(void)
{
#define UV_CHECK(c) do{if(!(c)){fprintf(stderr,"UV editor line %d: %s\n",__LINE__,#c);return 1;}}while(0)
    char temporary[]="/tmp/ymgre-uv-XXXXXX",path[4096],exported[4096],error[256];
    const char* root=getenv("YMGRE_UV_OUTPUT_DIR");if(!root)root=mkdtemp(temporary);UV_CHECK(root);
    if(getenv("YMGRE_UV_PRIMITIVES")) {
        createNewScene();SceneEditorPlaceResult legacy={.kind=SCENE_PLACE_CAPSULE,.scale=1,.detailA=4,.detailB=12,.color=0xFFFFFFFF,.preserveSourceUv=1,.legacyCapsule=1};
        strcpy(legacy.name,"Legacy capsule");strcpy(legacy.type,"胶囊");legacy.mesh=SceneEditorPlace_CreateMesh(&legacy);UV_CHECK(legacy.mesh);objectPlaced(&legacy,NULL);
        unsigned legacyVertices=g_selected->mesh->pointNum;size_t legacyBytes=legacyVertices*sizeof(gre_vertex4d);
        uint64_t legacyGeometry=signatureBytes(0,g_selected->mesh->pointList,legacyBytes);
        snprintf(path,sizeof(path),"%s/legacy-capsule.scene",root);UV_CHECK(writeCurrentScene(path,error,sizeof(error)));loadSceneFromPath(path,NULL);
        UV_CHECK(g_selected==NULL);SceneEditorObject* old=uvTestFindObject();UV_CHECK(old&&!old->autoUv&&old->mesh->pointNum==legacyVertices&&signatureBytes(0,old->mesh->pointList,legacyBytes)==legacyGeometry);
        selectObject(old,"旧胶囊升级验证");UV_CHECK(uvTestOpen());uvTestClick(714,662);uvTestClick(904,662);UV_CHECK(old->autoUv==5);
        int equator=0;for(int i=0;i<old->mesh->pointNum;i++)equator+=fabsf(old->mesh->pointList[i].pos.y+13)<1e-4f;UV_CHECK(equator>0);
        bakeObject(old,NULL);UV_CHECK(old->mesh->lightmap);
        const char* names[8]={"plane","cube","box","sphere","cylinder","cone","torus","capsule"};
        for(int kind=0;kind<8;kind++) {
            createNewScene();SceneEditorObject* object=opticsObject((SceneEditorPlaceKind)kind,names[kind],0,18,0,1,0xFFFFFFFF);
            UV_CHECK(object&&object->autoUv==(kind==3?2:5));
            int count;float* original=uvTestCapture(object->mesh,&count);UV_CHECK(original&&uvTestOpen());
            uvTestClick(900,56);uvTestClick(900,83);uint64_t preview=uvTestPreviewHash();
            uvTestClick(714,662);UV_CHECK(uvTestMatches(object->mesh,original,count)&&uvTestPreviewHash()==preview);
            rendererSaveScreen(root,names[kind]);uvTestClick(550,56);UV_CHECK(object->texturePath[0]);uvTestClick(904,662);
            SceneEditorObject_SetRotationAxis(object,0,19);SceneEditorObject_SetRotationAxis(object,1,31);SceneEditorObject_SetScale(object,1.4f);
            snprintf(path,sizeof(path),"%s/%s.scene",root,names[kind]);UV_CHECK(writeCurrentScene(path,error,sizeof(error)));loadSceneFromPath(path,NULL);object=uvTestFindObject();
            UV_CHECK(object&&object->texturePath[0]&&uvTestMatches(object->mesh,original,count));
            UV_CHECK(SceneModel_Export(object->mesh,&g_importContext.MaterialList,NULL,NULL,root,names[kind],exported,sizeof(exported),error,sizeof(error)));
            gre_scence imported={0};GRE_Object4d restored=YMGRE_LoadOgreMeshAndMaterial(&imported,exported);UV_CHECK(restored&&uvTestMatches(restored,original,count));
            GRE_Material material=YMGRE_Material_Find(&imported.MaterialList,restored->materiaName);UV_CHECK(material&&material->pixel&&material->height==200);
            YMGRE_Free_Object(restored);YMGRE_List_Clear(&imported.MaterialList,YMGRE_Free_Material);
            selectObject(object,"基本形状复制验证");duplicateObject(object,NULL);UV_CHECK(g_selected!=object&&uvTestMatches(g_selected->mesh,original,count));deleteObject(g_selected,NULL);
            selectObject(object,"基本形状烘焙验证");bakeObject(object,NULL);UV_CHECK(object->mesh->lightmap);
            snprintf(path,sizeof(path),"%s/%s-baked.scene",root,names[kind]);UV_CHECK(writeCurrentScene(path,error,sizeof(error)));loadSceneFromPath(path,NULL);object=uvTestFindObject();UV_CHECK(object&&object->mesh->lightmap);
            if(kind!=1&&kind!=2){UV_CHECK(inspectorRemesh(object,6,12,NULL)&&object->autoUv==(kind==3?2:5));int n;float* uv=uvTestCapture(object->mesh,&n);UV_CHECK(uv&&uvTestOpen());uvTestClick(714,662);UV_CHECK(uvTestMatches(object->mesh,uv,n));uvTestClick(904,662);free(uv);}
            free(original);printf("Primitive %s: default/re-unwrap, preview, texture, transforms, scene/export roundtrip, duplicate, bake and remesh PASS\n",names[kind]);
        }
        printf("Primitive UV screenshots: %s\n",root);return 0;
    }
    if(getenv("YMGRE_UV_PERF")) {
        GRErgb24* pixels=malloc(400*400*sizeof(*pixels));UV_CHECK(pixels);
        for(int model=0;model<2;model++) {
            createNewScene();
            if(model)UV_CHECK(importMesh(YMGRE_TANK_FIXTURE,0,NULL));
            else {SceneEditorObject* sphere=opticsObject(SCENE_PLACE_SPHERE,"Perf sphere",0,18,0,1,0xFFFFFFFF);UV_CHECK(sphere&&inspectorRemesh(sphere,32,64,NULL));}
            SceneUvEdit* edit=SceneUvEdit_Create(g_selected->mesh);UV_CHECK(edit);
            clock_t start=clock();
            for(int frame=0;frame<8;frame++)UV_CHECK(SceneUvPreview_Render(edit,&g_importContext.MaterialList,1,(GRErgb24){255,255,255},32+frame*3,24,1,400,pixels));
            printf("UV PERF %s preview 400x400: %.2f ms/frame\n",model?"Tank":"sphere 4096 faces",1000.0*(clock()-start)/CLOCKS_PER_SEC/8);
            SceneUvEdit_Free(edit);UV_CHECK(uvTestOpen());renderEditorFrame();start=clock();
            YMGUI_Inject_Pointer(756,312,1);
            for(int frame=0;frame<8;frame++){YMGUI_Inject_Pointer(759+frame*3,312,1);renderEditorFrame();}
            YMGUI_Inject_Pointer(780,312,0);
            printf("UV PERF %s full UI orbit: %.2f ms/frame\n",model?"Tank":"sphere",1000.0*(clock()-start)/CLOCKS_PER_SEC/8);
            uvTestClick(904,662);
        }
        free(pixels);return 0;
    }
    if(getenv("YMGRE_UV_APPLY_DEMO")) {
        createNewScene();SceneEditorObject* object=opticsObject(SCENE_PLACE_SPHERE,"Applied checker sphere",0,18,0,1,0xFF5CB07E);UV_CHECK(object&&object->autoUv==2&&uvTestOpen());
        int count;float* original=uvTestCapture(object->mesh,&count);UV_CHECK(original);
        uint64_t initialPreview=uvTestPreviewHash();rendererSaveScreen(root,"sphere-default-uv");uvTestClick(714,662);
        UV_CHECK(uvTestMatches(object->mesh,original,count)&&uvTestPreviewHash()==initialPreview);
        for(int wide=0;wide<2;wide++) {
            const GRErgb24* grid=SceneUvPreview_Checker(wide);unsigned w=wide?400:200;
            for(unsigned i=0;i<w*200;i++){GRErgb24 expected=((i%w/20+i/w/20)%2)?(GRErgb24){78,138,201}:(GRErgb24){245,247,250};UV_CHECK(!memcmp(grid+i,&expected,sizeof(expected)));}
        }
        uint32 history=g_historyIndex;uvTestClick(550,56);
        UV_CHECK(object->texturePath[0]&&access(object->texturePath,R_OK)==0&&object->color==0xFFFFFFFF&&g_historyIndex==history+1);
        GRE_Material material=YMGRE_Material_Find(&g_importContext.MaterialList,object->mesh->materiaName);
        UV_CHECK(material&&material->width==400&&material->height==200&&!memcmp(material->pixel,SceneUvPreview_Checker(1),400*200*sizeof(GRErgb24)));
        rendererSaveScreen(root,"checker-applied-editor");uvTestClick(84,662);object=g_selected;UV_CHECK(object&&!object->texturePath[0]);
        uvTestClick(192,662);object=g_selected;UV_CHECK(object&&object->texturePath[0]);uvTestClick(904,662);
        renderScene();material=YMGRE_Material_Find(&g_importContext.MaterialList,object->mesh->materiaName);
        UV_CHECK(material&&!memcmp(material->pixel,SceneUvPreview_Checker(1),400*200*sizeof(GRErgb24)));
        unsigned blue=0;for(unsigned i=0;i<(unsigned)g_target.width*g_target.height;i++){GRErgb24 c=GRE_FramePixel_To_RGB24(g_target.data[i]);if(c.B>c.R+30&&c.B>c.G+15)blue++;}UV_CHECK(blue>100);
        rendererSaveScreen(root,"checker-applied-scene");
        snprintf(path,sizeof(path),"%s/applied-checker.scene",root);UV_CHECK(writeCurrentScene(path,error,sizeof(error)));loadSceneFromPath(path,NULL);object=uvTestFindObject();
        UV_CHECK(object&&object->texturePath[0]&&object->autoUv==2&&uvTestMatches(object->mesh,original,count));
        material=YMGRE_Material_Find(&g_importContext.MaterialList,object->mesh->materiaName);UV_CHECK(material&&!memcmp(material->pixel,SceneUvPreview_Checker(1),400*200*sizeof(GRErgb24)));
        UV_CHECK(SceneModel_Export(object->mesh,&g_importContext.MaterialList,NULL,NULL,root,"AppliedChecker",exported,sizeof(exported),error,sizeof(error)));
        gre_scence imported={0};GRE_Object4d mesh=YMGRE_LoadOgreMeshAndMaterial(&imported,exported);UV_CHECK(mesh&&uvTestMatches(mesh,original,count));
        material=YMGRE_Material_Find(&imported.MaterialList,mesh->materiaName);UV_CHECK(material&&material->width==400&&material->height==200&&!memcmp(material->pixel,SceneUvPreview_Checker(1),400*200*sizeof(GRErgb24)));
        YMGRE_Free_Object(mesh);YMGRE_List_Clear(&imported.MaterialList,YMGRE_Free_Material);free(original);
        selectObject(object,"复制已应用贴图的球体");duplicateObject(object,NULL);SceneEditorObject* copy=g_selected;UV_CHECK(copy!=object&&copy->texturePath[0]);
        UV_CHECK(inspectorRemesh(copy,12,24,NULL)&&copy->autoUv==2);material=YMGRE_Material_Find(&g_importContext.MaterialList,copy->mesh->materiaName);
        UV_CHECK(material&&material->width==400&&!memcmp(material->pixel,SceneUvPreview_Checker(1),400*200*sizeof(GRErgb24)));deleteObject(copy,NULL);
        selectObject(object,"验证应用贴图和烘焙");bakeObject(object,NULL);UV_CHECK(object->mesh->lightmap&&uvTestOpen());
        uvTestClick(550,56);UV_CHECK(!object->mesh->lightmap&&object->texturePath[0]);uvTestClick(84,662);object=g_selected;UV_CHECK(object&&object->mesh->lightmap);
        /* An unwritable output path cannot remove the active bake/material or create history. */
        GRE_Lightmap bake=object->mesh->lightmap;material=YMGRE_Material_Find(&g_importContext.MaterialList,object->mesh->materiaName);history=g_historyIndex;
        char blocked[4096];snprintf(blocked,sizeof(blocked),"%s/texture-output-blocked",root);FILE* f=fopen(blocked,"w");UV_CHECK(f);fclose(f);
        /* Reject a valid BMP header with missing pixels without changing the object. */
        unsigned char truncated[54];f=fopen(object->texturePath,"rb");UV_CHECK(f&&fread(truncated,1,sizeof(truncated),f)==sizeof(truncated));fclose(f);
        f=fopen(blocked,"wb");UV_CHECK(f&&fwrite(truncated,1,sizeof(truncated),f)==sizeof(truncated));fclose(f);
        UV_CHECK(!SceneEditorTexture_Load(object,&g_importContext.MaterialList,blocked,error,sizeof(error)));
        UV_CHECK(object->mesh->lightmap==bake&&YMGRE_Material_Find(&g_importContext.MaterialList,object->mesh->materiaName)==material);
        const char* previous=getenv("YMGRE_BAKE_DIR");char* saved=previous?strdup(previous):NULL;setenv("YMGRE_BAKE_DIR",blocked,1);
        uvTestClick(900,56);uvTestClick(900,83);uvTestClick(550,56);
        UV_CHECK(object->mesh->lightmap==bake&&g_historyIndex==history&&YMGRE_Material_Find(&g_importContext.MaterialList,object->mesh->materiaName)==material);
        if(saved){setenv("YMGRE_BAKE_DIR",saved,1);free(saved);}else unsetenv("YMGRE_BAKE_DIR");uvTestClick(904,662);
        /* Legacy sphere scenes and their copies keep the old UV layout until explicitly regenerated. */
        createNewScene();SceneEditorPlaceResult legacy={.kind=SCENE_PLACE_SPHERE,.scale=1,.detailA=6,.detailB=12,.color=0xFFFFFFFF,.preserveSourceUv=1};strcpy(legacy.name,"Legacy sphere");strcpy(legacy.type,"球体");legacy.mesh=SceneEditorPlace_CreateMesh(&legacy);UV_CHECK(legacy.mesh);objectPlaced(&legacy,NULL);object=g_selected;
        UV_CHECK(object&&!object->autoUv&&object->mesh->polygonNum==120);original=uvTestCapture(object->mesh,&count);UV_CHECK(original);
        snprintf(path,sizeof(path),"%s/legacy-sphere.scene",root);UV_CHECK(writeCurrentScene(path,error,sizeof(error)));loadSceneFromPath(path,NULL);object=uvTestFindObject();UV_CHECK(object&&!object->autoUv&&uvTestMatches(object->mesh,original,count));
        selectObject(object,"复制旧球体");duplicateObject(object,NULL);UV_CHECK(g_selected!=object&&!g_selected->autoUv&&uvTestMatches(g_selected->mesh,original,count));free(original);
        printf("UV apply PASS: new sphere consistency, pure blue/white grid, scene appearance, undo/redo, save/reload, export, duplicate/remesh, bake, failed apply and legacy sphere. Artifacts: %s\n",root);return 0;
    }
    if(getenv("YMGRE_UV_PREVIEW_DEMO")) {
        createNewScene();SceneEditorObject* object=opticsObject(SCENE_PLACE_SPHERE,"UV preview sphere",0,18,0,1,0xFF5CB07E);UV_CHECK(object&&uvTestOpen());
        /* Exercise the real frame loop: both cameras stop while the modal is open,
           UI interaction continues, and closing resumes rendering the changed scene. */
        uvTestClick(904,662);float cameraX=g_activeCamera->x;
        size_t frameBytes=(size_t)g_target.width*g_target.height*sizeof(*g_target.data);
        for(int mode=0;mode<2;mode++) {
            g_rendererMode=mode;renderEditorFrame();UV_CHECK(uvTestOpen()&&SceneUvEditor_IsOpen());
            uint64_t frozen=signatureBytes(0,g_target.data,frameBytes),preview=uvTestPreviewHash();int progress=g_rayProgress;
            g_activeCamera->x+=30;
            for(int frame=0;frame<3;frame++)renderEditorFrame();
            UV_CHECK(signatureBytes(0,g_target.data,frameBytes)==frozen&&g_rayProgress==progress);
            YMGUI_Inject_Wheel(756,312,0,1);renderEditorFrame();UV_CHECK(uvTestPreviewHash()!=preview);
            UV_CHECK(signatureBytes(0,g_target.data,frameBytes)==frozen);
            uvTestClick(904,662);UV_CHECK(!SceneUvEditor_IsOpen());renderEditorFrame();
            UV_CHECK(signatureBytes(0,g_target.data,frameBytes)!=frozen);
            g_activeCamera->x=cameraX;
        }
        g_rendererMode=0;renderEditorFrame();UV_CHECK(uvTestOpen());
        uvTestClick(714,662);
        int count;float* original=uvTestCapture(object->mesh,&count);UV_CHECK(original);
        uint32 history=g_historyIndex;uint64_t before=uvTestPreviewHash();
        rendererSaveScreen(root,"sphere-checker-preview");
        uvTestClick(706,574);YMGUI_Inject_Pointer(68,212,1);YMGUI_Inject_Pointer(74,216,1);
        UV_CHECK(uvTestMatches(object->mesh,original,count)&&g_historyIndex==history);
        UV_CHECK(uvTestPreviewHash()!=before);rendererSaveScreen(root,"sphere-checker-drag");
        YMGUI_Inject_Key(27,1);YMGUI_Inject_Pointer(74,216,0);
        UV_CHECK(uvTestPreviewHash()==before&&uvTestMatches(object->mesh,original,count)&&g_historyIndex==history);
        YMGUI_Inject_Pointer(756,312,1);YMGUI_Inject_Pointer(786,328,1);YMGUI_Inject_Pointer(786,328,0);
        uint64_t rotated=uvTestPreviewHash();UV_CHECK(rotated!=before);
        YMGUI_Inject_Wheel(756,312,0,1);UV_CHECK(uvTestPreviewHash()!=rotated&&g_historyIndex==history);
        /* Commit, undo, and redo while the preview is open must replace stale mesh references. */
        UV_CHECK(uvTestInput(68,602,80,"0.03"));uvTestClick(800,618);UV_CHECK(object->uvEdited);
        uint64_t committed=uvTestPreviewHash();uvTestClick(84,662);object=g_selected;
        UV_CHECK(object&&!object->uvEdited&&uvTestMatches(object->mesh,original,count));
        uvTestClick(192,662);object=g_selected;UV_CHECK(object&&object->uvEdited&&uvTestPreviewHash()==committed);
        free(original);uvTestClick(904,662);
        createNewScene();UV_CHECK(importMesh(YMGRE_TANK_FIXTURE,0,NULL));object=g_selected;renderScene();
        GRE_Material material=YMGRE_Material_Find(&g_importContext.MaterialList,object->mesh->materiaName);UV_CHECK(material&&material->pixel);
        gre_material saved=*material;uint64_t textureHash=signatureBytes(0,material->pixel,(size_t)material->width*material->height*sizeof(GRErgb24));
        original=uvTestCapture(object->mesh,&count);UV_CHECK(original);
        bakeObject(object,NULL);GRE_Lightmap bake=object->mesh->lightmap;UV_CHECK(bake);
        history=g_historyIndex;UV_CHECK(uvTestOpen());before=uvTestPreviewHash();
        uvTestClick(900,56);uvTestClick(900,105);uint64_t current=uvTestPreviewHash();UV_CHECK(current!=before);
        rendererSaveScreen(root,"tank-material-preview");
        UV_CHECK(!memcmp(material,&saved,sizeof(saved))&&textureHash==signatureBytes(0,material->pixel,(size_t)material->width*material->height*sizeof(GRErgb24)));
        UV_CHECK(object->mesh->lightmap==bake&&uvTestMatches(object->mesh,original,count)&&g_historyIndex==history);
        uvTestClick(728,56);uvTestClick(904,662);UV_CHECK(object->mesh->lightmap==bake&&g_historyIndex==history);free(original);
        printf("UV texture preview PASS: raster/ray viewport pause and resume, live modal input, checker, staged UV updates, Esc, orbit/zoom, undo/redo, original Tank material, bake and material isolation. Artifacts: %s\n",root);return 0;
    }
    if(getenv("YMGRE_UV_CYLINDER_DEMO")) {
        createNewScene();SceneEditorObject* cylinder=opticsObject(SCENE_PLACE_CYLINDER,"Cylinder",0,18,0,1,0xFF5CB07E);UV_CHECK(cylinder);
        UV_CHECK(inspectorRemesh(cylinder,32,32,NULL));
        UV_CHECK(SceneEditorObject_GenerateUv(cylinder,3,NULL,error,sizeof(error)));historyCommit();
        int count;float* old=uvTestCapture(cylinder->mesh,&count);UV_CHECK(old);
        char legacyPath[4096];snprintf(legacyPath,sizeof(legacyPath),"%s/cylinder-legacy.scene",root);UV_CHECK(writeCurrentScene(legacyPath,error,sizeof(error)));
        UV_CHECK(uvTestOpen());rendererSaveScreen(root,"cylinder-uv-before");uvTestClick(714,662);
        UV_CHECK(cylinder->autoUv==5&&uvTestCapRatio(cylinder->mesh,0)<1.001f&&uvTestCapRatio(cylinder->mesh,1)<1.001f);
        rendererSaveScreen(root,"cylinder-uv-after");uvTestClick(904,662);
        float* fixed=uvTestCapture(cylinder->mesh,&count);UV_CHECK(fixed);
        SceneEditorObject_SetRotationAxis(cylinder,0,17);SceneEditorObject_SetRotationAxis(cylinder,2,23);SceneEditorObject_SetScale(cylinder,1.7f);
        snprintf(path,sizeof(path),"%s/cylinder-fixed.scene",root);UV_CHECK(writeCurrentScene(path,error,sizeof(error)));
        loadSceneFromPath(path,NULL);cylinder=uvTestFindObject();UV_CHECK(cylinder&&cylinder->autoUv==5&&uvTestMatches(cylinder->mesh,fixed,count));
        UV_CHECK(SceneModel_Export(cylinder->mesh,&g_importContext.MaterialList,NULL,NULL,root,"CylinderUV",exported,sizeof(exported),error,sizeof(error)));
        gre_scence imported={0};GRE_Object4d mesh=YMGRE_LoadOgreMeshAndMaterial(&imported,exported);
        UV_CHECK(mesh&&uvTestMatches(mesh,fixed,count)&&uvTestCapRatio(mesh,0)<1.001f&&uvTestCapRatio(mesh,1)<1.001f);
        YMGRE_Free_Object(mesh);YMGRE_List_Clear(&imported.MaterialList,YMGRE_Free_Material);
        loadSceneFromPath(legacyPath,NULL);cylinder=uvTestFindObject();UV_CHECK(cylinder&&cylinder->autoUv==3&&uvTestMatches(cylinder->mesh,old,count));
        free(old);free(fixed);printf("Cylinder UV proportions: caps remain circular, UI, save/reload, export and legacy scene preservation PASS. Artifacts: %s\n",root);return 0;
    }
    if(getenv("YMGRE_UV_SPHERE_DEMO")) {
        createNewScene();SceneEditorObject* sphere=opticsObject(SCENE_PLACE_SPHERE,"Sphere",0,18,0,1,0xFF5CB07E);
        UV_CHECK(sphere&&inspectorRemesh(sphere,20,20,NULL));
        g_activeCamera->x=0;g_activeCamera->y=18;g_activeCamera->z=-60;g_activeCamera->targetY=18;
        g_globalLight->strength=.3f;SceneEditorObject_SyncHandle(g_globalLight);
        SceneEditorPlaceResult lamp={.kind=SCENE_PLACE_POINT_LIGHT,.x=-35,.y=50,.z=-55,.strength=1.2f,.color=0xFFFFFFFF};
        strcpy(lamp.name,"Point Light");objectPlaced(&lamp,NULL);
        sphere->analytic=1;g_referenceVisible=0;g_transformMode=TRANSFORM_NONE;g_selected=NULL;
        rendererClickOption(1);UV_CHECK(rendererFinish());rendererSaveScreen(root,"sphere");
        selectObject(sphere,"球体自动 UV 展开");UV_CHECK(uvTestOpen());uvTestClick(714,662);
        UV_CHECK(sphere->autoUv&&rendererFinish());rendererSaveScreen(root,"sphere-uv");uvTestClick(904,662);
        snprintf(path,sizeof(path),"%s/sphere-uv.scene",root);UV_CHECK(writeCurrentScene(path,error,sizeof(error)));
        SceneEditorObject_SetColor(sphere,0xFFFFFFFF);renderScene();
        GRE_Material earth=YMGRE_Material_Find(&g_importContext.MaterialList,sphere->mesh->materiaName);UV_CHECK(earth);
        GRErgb24* pixels=NULL;uint16 w=0,h=0;YMGRE_Bmp_File_LoadTo_Image(YMGRE_EARTH_FIXTURE,&pixels,&w,&h);UV_CHECK(pixels&&w&&h);
        GRE_ImageBuff_Free(earth->pixel);earth->pixel=pixels;earth->width=w;earth->height=h;
        earth->unlit=1;earth->diffuse=(GRErgb24){255,255,255};sphere->analytic=0;g_selected=NULL;
        UV_CHECK(rendererFinish());rendererSaveScreen(root,"sphere-map");
        UV_CHECK(SceneModel_Export(sphere->mesh,&g_importContext.MaterialList,NULL,NULL,root,"EarthUV",exported,sizeof(exported),error,sizeof(error)));
        printf("Sphere UV preview: %d triangles, %d seam vertices; %s\n",sphere->mesh->polygonNum,sphere->mesh->pointNum,root);
        return 0;
    }
    createNewScene();UV_CHECK(importMesh(YMGRE_TANK_FIXTURE,0,NULL));SceneEditorObject* object=g_selected;
    g_activeCamera->x=8;g_activeCamera->y=6;g_activeCamera->z=-12;g_activeCamera->targetY=1;g_activeCamera->targetZ=-1;
    renderScene();UV_CHECK(!g_rendererMode&&!object->autoUv&&object->mesh->importedUvCount==6);
    int count;float* original=uvTestCapture(object->mesh,&count);UV_CHECK(original);
    uint32 history=g_historyCount;UV_CHECK(uvTestOpen());rendererSaveScreen(root,"tank-original-uv");uvTestClick(904,662);
    UV_CHECK(!object->autoUv&&history==g_historyCount&&uvTestMatches(object->mesh,original,count));free(original);
    UV_CHECK(uvTestOpen());uvTestClick(714,662);
    UV_CHECK(object->autoUv&&g_historyCount==history+1&&object->mesh->importedUvCount==6);
    renderScene();rendererSaveScreen(root,"tank-uv");uvTestClick(904,662);
    original=uvTestCapture(object->mesh,&count);UV_CHECK(original);
    /* Save after moving/rotating: reconstruction must retain UVs despite world-space transforms. */
    SceneEditorObject_SetRotationAxis(object,0,17);SceneEditorObject_SetRotationAxis(object,2,23);
    SceneEditorObject_Translate(object,3,4,5);SceneEditorObject_SetScale(object,1.7f);historyCommit();
    snprintf(path,sizeof(path),"%s/automatic-uv.scene",root);UV_CHECK(writeCurrentScene(path,error,sizeof(error)));
    loadSceneFromPath(path,NULL);UV_CHECK(!strcmp(path,g_currentScenePath));object=uvTestFindObject();
    UV_CHECK(object&&object->autoUv&&uvTestMatches(object->mesh,original,count));
    selectObject(object,"自动 UV 导出验收");renderScene();
    UV_CHECK(SceneModel_Export(object->mesh,&g_importContext.MaterialList,NULL,NULL,root,"AutoUV",exported,sizeof(exported),error,sizeof(error)));
    gre_scence imported={0};GRE_Object4d restored=YMGRE_LoadOgreMeshAndMaterial(&imported,exported);
    UV_CHECK(restored&&uvTestMatches(restored,original,count)&&restored->importedUvCount==6);
    YMGRE_Free_Object(restored);YMGRE_List_Clear(&imported.MaterialList,YMGRE_Free_Material);free(original);
    /* Real undo/redo reopens history snapshots and restores the original imported UVs. */
    createNewScene();UV_CHECK(importMesh(YMGRE_TANK_FIXTURE,0,NULL));object=g_selected;
    original=uvTestCapture(object->mesh,&count);UV_CHECK(original&&uvTestOpen());uvTestClick(714,662);uvTestClick(904,662);
    undoClicked(NULL);object=uvTestFindObject();UV_CHECK(object&&!object->autoUv&&uvTestMatches(object->mesh,original,count));
    redoClicked(NULL);object=uvTestFindObject();UV_CHECK(object&&object->autoUv);free(original);
    selectObject(object,"自动 UV 复制验收");duplicateObject(object,NULL);UV_CHECK(g_selected!=object&&g_selected->autoUv);
    createNewScene();object=opticsObject(SCENE_PLACE_CUBE,"Cube",0,18,0,1,0xFF5CB07E);UV_CHECK(object&&uvTestOpen());
    uvTestClick(714,662);uvTestClick(904,662);original=uvTestCapture(object->mesh,&count);UV_CHECK(original);
    SceneEditorObject_SetRotationAxis(object,0,19);SceneEditorObject_SetRotationAxis(object,1,37);
    SceneEditorObject_SetRotationAxis(object,2,53);SceneEditorObject_SetScale(object,2.3f);
    snprintf(path,sizeof(path),"%s/cube-uv.scene",root);UV_CHECK(writeCurrentScene(path,error,sizeof(error)));
    loadSceneFromPath(path,NULL);object=uvTestFindObject();UV_CHECK(object&&object->autoUv&&uvTestMatches(object->mesh,original,count));free(original);
    createNewScene();object=opticsObject(SCENE_PLACE_SPHERE,"Map sphere",0,18,0,1,0xFFFFFFFF);UV_CHECK(object&&uvTestOpen());
    object->analytic=1;uvTestClick(714,662);uvTestClick(904,662);UV_CHECK(object->autoUv==2&&object->analytic);
    original=uvTestCapture(object->mesh,&count);UV_CHECK(original);
    SceneEditorObject_SetRotationAxis(object,0,23);SceneEditorObject_SetRotationAxis(object,1,51);
    snprintf(path,sizeof(path),"%s/map-sphere.scene",root);UV_CHECK(writeCurrentScene(path,error,sizeof(error)));
    loadSceneFromPath(path,NULL);object=uvTestFindObject();UV_CHECK(object&&object->autoUv==2&&uvTestMatches(object->mesh,original,count));free(original);
    selectObject(object,"矩形球体 UV 导出验收");renderScene();
    UV_CHECK(SceneModel_Export(object->mesh,&g_importContext.MaterialList,NULL,NULL,root,"MapSphere",exported,sizeof(exported),error,sizeof(error)));
    original=uvTestCapture(object->mesh,&count);UV_CHECK(original);
    gre_scence mapImport={0};restored=YMGRE_LoadOgreMeshAndMaterial(&mapImport,exported);
    UV_CHECK(restored&&uvTestMatches(restored,original,count));YMGRE_Free_Object(restored);free(original);
    YMGRE_List_Clear(&mapImport.MaterialList,YMGRE_Free_Material);
    UV_CHECK(inspectorRemesh(object,12,24,NULL)&&object->autoUv==2&&object->mesh->pointNum==13*25);
    /* A closed irregular surface, and the same UI in ray tracing mode. */
    createNewScene();object=opticsObject(SCENE_PLACE_TORUS,"Torus",0,18,0,1,0xFF5CB07E);UV_CHECK(object);
    g_activeCamera->x=55;g_activeCamera->y=65;g_activeCamera->z=-70;g_activeCamera->targetY=18;
    rendererClickOption(1);selectObject(object,"圆环自动展开");UV_CHECK(uvTestOpen());uvTestClick(714,662);
    UV_CHECK(object->autoUv&&rendererFinish());rendererSaveScreen(root,"torus-uv");uvTestClick(904,662);
    UV_CHECK(object->mesh->polygonNum>100);int islands;UV_CHECK(SceneEditorObject_GenerateUv(object,3,&islands,error,sizeof(error))&&islands<=16);
    SceneEditorObject_Translate(object,7,5,3);SceneEditorObject_SetRotationAxis(object,0,19);
    UV_CHECK(inspectorRemesh(object,12,18,NULL));original=uvTestCapture(object->mesh,&count);UV_CHECK(original);
    snprintf(path,sizeof(path),"%s/remeshed-torus.scene",root);UV_CHECK(writeCurrentScene(path,error,sizeof(error)));
    loadSceneFromPath(path,NULL);object=uvTestFindObject();UV_CHECK(object&&object->autoUv==3&&uvTestMatches(object->mesh,original,count));free(original);
    /* Manual editing through real controls, then persistence and export. */
    createNewScene();UV_CHECK(importMesh(YMGRE_TANK_FIXTURE,0,NULL));object=g_selected;
    UV_CHECK(uvTestOpen());uvTestClick(714,662);uvTestClick(904,662);UV_CHECK(uvTestOpen());
    original=uvTestCapture(object->mesh,&count);UV_CHECK(original);
    unsigned vertices=object->mesh->pointNum;
    gre_fvector4d* positions=malloc(vertices*sizeof(*positions));UV_CHECK(positions);
    for(unsigned i=0;i<vertices;i++)positions[i]=object->mesh->pointList[i].pos;
    history=g_historyIndex;
    uvTestClick(706,574); /* Select all, then translate and independently scale about bounds. */
    UV_CHECK(uvTestInput(68,602,80,"0.05")&&uvTestInput(192,602,80,"-0.02"));
    UV_CHECK(uvTestInput(488,602,76,"0.8")&&uvTestInput(632,602,76,"0.7"));
    uvTestClick(800,618);
    UV_CHECK(object->uvEdited&&g_historyIndex==history+1&&object->mesh->pointNum==vertices);
    float loU=INFINITY,loV=INFINITY,hiU=-INFINITY,hiV=-INFINITY;
    for(int i=0;i<count;i+=2){loU=fminf(loU,original[i]);hiU=fmaxf(hiU,original[i]);loV=fminf(loV,original[i+1]);hiV=fmaxf(hiV,original[i+1]);}
    float* expected=malloc(count*sizeof(float));UV_CHECK(expected);
    for(int i=0;i<count;i+=2){expected[i]=(loU+hiU)*.5f+.05f+(original[i]-(loU+hiU)*.5f)*.8f;expected[i+1]=(loV+hiV)*.5f-.02f+(original[i+1]-(loV+hiV)*.5f)*.7f;}
    UV_CHECK(uvTestMatches(object->mesh,expected,count));
    for(unsigned i=0;i<vertices;i++)UV_CHECK(!memcmp(positions+i,&object->mesh->pointList[i].pos,sizeof(*positions)));
    free(positions);
    /* Undo and redo from inside the editor must rebuild its mesh references safely. */
    uvTestClick(84,662);object=g_selected;UV_CHECK(object&&!object->uvEdited&&uvTestMatches(object->mesh,original,count));
    uvTestClick(192,662);object=g_selected;UV_CHECK(object&&object->uvEdited&&uvTestMatches(object->mesh,expected,count));
    /* Select an island by vertex position; Esc cancels a drag without writing any UVs/history. */
    gre_vertex4d vertex=object->mesh->pointList[0];int px=lroundf(268+(vertex.u-.5f)*400),py=lroundf(312+(vertex.v-.5f)*400);
    history=g_historyIndex;
    YMGUI_Inject_Pointer(px,py,1);YMGUI_Inject_Pointer(px+20,py+12,1);
    UV_CHECK(uvTestMatches(object->mesh,expected,count));SDL_Event escape={0};escape.type=SDL_KEYDOWN;escape.key.keysym.sym=SDLK_ESCAPE;UV_CHECK(SDL_PushEvent(&escape)==1&&SDL_LCD_PumpEvents());YMGUI_Inject_Pointer(px+20,py+12,0);
    UV_CHECK(g_historyIndex==history&&uvTestMatches(object->mesh,expected,count));
    YMGUI_Inject_Pointer(px,py,1);YMGUI_Inject_Pointer(px+20,py+12,1);YMGUI_Inject_Pointer(px+20,py+12,0);
    UV_CHECK(g_historyIndex==history+1);free(expected);expected=uvTestCapture(object->mesh,&count);UV_CHECK(expected);
    /* Switch to vertex mode using the actual dropdown and move one welded vertex. */
    uvTestClick(166,572);uvTestClick(160,599);
    GYOBJ selectionMode=uvTestWidget(YMGUI_Ctx_GetTopLayer(g_rightPanel->ctx),104,558,124,30);
    UV_CHECK(selectionMode&&YMGUI_Dropdown_GetSelected(selectionMode)==0);
    vertex=object->mesh->pointList[0];px=lroundf(268+(vertex.u-.5f)*400);py=lroundf(312+(vertex.v-.5f)*400);
    history=g_historyIndex;
    YMGUI_Inject_Pointer(px,py,1);YMGUI_Inject_Pointer(px+8,py+4,1);YMGUI_Inject_Pointer(px+8,py+4,0);
    UV_CHECK(g_historyIndex==history+1);
    int changedCorners=0;float* vertexResult=uvTestCapture(object->mesh,&count);UV_CHECK(vertexResult);
    for(int i=0;i<count;i+=2)if(fabsf(vertexResult[i]-expected[i])>1e-5f||fabsf(vertexResult[i+1]-expected[i+1])>1e-5f){changedCorners++;UV_CHECK(fabsf(vertexResult[i]-expected[i]-.02f)<1e-5f&&fabsf(vertexResult[i+1]-expected[i+1]-.01f)<1e-5f);}
    UV_CHECK(changedCorners>0&&changedCorners<16);
    /* SDL key mapping: Ctrl+Z undoes; Ctrl+Shift+Z restores from the reopened canvas. */
    SDL_Event key={0};key.type=SDL_KEYDOWN;key.key.keysym.sym=SDLK_z;SDL_SetModState(KMOD_CTRL);
    UV_CHECK(SDL_PushEvent(&key)==1&&SDL_LCD_PumpEvents());object=g_selected;UV_CHECK(object&&uvTestMatches(object->mesh,expected,count));
    GYOBJ canvas=uvTestWidget(YMGUI_Ctx_GetTopLayer(g_rightPanel->ctx),36,96,464,432);UV_CHECK(canvas&&canvas->ctx->focus_obj==canvas);
    SDL_SetModState(KMOD_CTRL|KMOD_SHIFT);UV_CHECK(SDL_PushEvent(&key)==1&&SDL_LCD_PumpEvents());SDL_SetModState(KMOD_NONE);
    object=g_selected;UV_CHECK(object&&uvTestMatches(object->mesh,vertexResult,count));free(expected);expected=vertexResult;
    uvTestClick(166,572);uvTestClick(160,621);uvTestClick(px+8,py+4);
    rendererSaveScreen(root,"tank-manual-uv");uvTestClick(904,662);
    snprintf(path,sizeof(path),"%s/manual-uv.scene",root);UV_CHECK(writeCurrentScene(path,error,sizeof(error)));
    loadSceneFromPath(path,NULL);object=uvTestFindObject();UV_CHECK(object&&object->uvEdited&&!object->pendingUv&&uvTestMatches(object->mesh,expected,count));
    selectObject(object,"手动 UV 导出验收");
    UV_CHECK(SceneModel_Export(object->mesh,&g_importContext.MaterialList,NULL,NULL,root,"ManualUV",exported,sizeof(exported),error,sizeof(error)));
    gre_scence manualImport={0};restored=YMGRE_LoadOgreMeshAndMaterial(&manualImport,exported);UV_CHECK(restored&&restored->importedUvCount==6&&uvTestMatches(restored,expected,count));YMGRE_Free_Object(restored);
    duplicateObject(object,NULL);UV_CHECK(g_selected!=object&&g_selected->uvEdited&&uvTestMatches(g_selected->mesh,expected,count));
    YMGRE_List_Clear(&manualImport.MaterialList,YMGRE_Free_Material);
    /* Source UVs, without an automatic generation marker, also persist. */
    free(original);free(expected);createNewScene();UV_CHECK(importMesh(YMGRE_TANK_FIXTURE,0,NULL));object=g_selected;UV_CHECK(uvTestOpen());
    uvTestClick(706,574);UV_CHECK(uvTestInput(68,602,80,"0.1"));uvTestClick(800,618);uvTestClick(904,662);
    UV_CHECK(object->uvEdited&&!object->autoUv);original=uvTestCapture(object->mesh,&count);UV_CHECK(original);
    snprintf(path,sizeof(path),"%s/manual-source-uv.scene",root);UV_CHECK(writeCurrentScene(path,error,sizeof(error)));
    loadSceneFromPath(path,NULL);object=uvTestFindObject();UV_CHECK(object&&object->uvEdited&&!object->autoUv&&uvTestMatches(object->mesh,original,count));
    selectObject(object,"手动 UV 烘焙保存验收");bakeObject(object,NULL);UV_CHECK(object->mesh->lightmap);
    UV_CHECK(writeCurrentScene(path,error,sizeof(error)));loadSceneFromPath(path,NULL);object=uvTestFindObject();
    UV_CHECK(object&&object->uvEdited&&object->mesh->lightmap&&uvTestMatches(object->mesh,original,count));
    FILE* input=fopen(path,"rb");UV_CHECK(input);fseek(input,0,SEEK_END);long length=ftell(input);rewind(input);char* xml=calloc(length+1,1);UV_CHECK(xml&&fread(xml,1,length,input)==(size_t)length);fclose(input);
    char* uvNode=strstr(xml,"<ymgre:uv0>");UV_CHECK(uvNode);uvNode+=strlen("<ymgre:uv0>");while(*uvNode&&isspace((unsigned char)*uvNode))uvNode++;UV_CHECK(*uvNode=='1');*uvNode='2';
    char invalidPath[4096];snprintf(invalidPath,sizeof(invalidPath),"%s/invalid-manual-uv.scene",root);FILE* output=fopen(invalidPath,"wb");UV_CHECK(output&&fwrite(xml,1,length,output)==(size_t)length);fclose(output);free(xml);
    GRE_Object4d live=object->mesh;loadSceneFromPath(invalidPath,NULL);UV_CHECK(object->mesh==live&&!strcmp(g_currentScenePath,path)&&uvTestMatches(object->mesh,original,count));free(original);
    YMGUI_MsgBox_Close(g_sceneOpenError);
    createNewScene();object=opticsObject(SCENE_PLACE_CUBE,"Manual bake cube",0,18,0,1,0xFF5CB07E);UV_CHECK(object&&uvTestOpen());
    uvTestClick(706,574);UV_CHECK(uvTestInput(68,602,80,"0.1"));uvTestClick(800,618);uvTestClick(904,662);
    UV_CHECK(object->uvEdited);bakeObject(object,NULL);UV_CHECK(object->mesh->lightmap);
    snprintf(path,sizeof(path),"%s/manual-baked-cube.scene",root);UV_CHECK(writeCurrentScene(path,error,sizeof(error)));
    uint64_t fingerprint=object->bakeFingerprint;loadSceneFromPath(path,NULL);object=uvTestFindObject();
    UV_CHECK(object&&object->uvEdited&&object->mesh->lightmap&&object->bakeFingerprint==fingerprint);
    selectObject(object,"烘焙后的 UV 编辑验收");UV_CHECK(uvTestOpen());uvTestClick(706,574);
    GRE_Lightmap previousBake=object->mesh->lightmap;uvTestClick(800,618);UV_CHECK(object->mesh->lightmap==previousBake);
    UV_CHECK(uvTestInput(68,602,80,"0.01"));uvTestClick(800,618);UV_CHECK(!object->mesh->lightmap&&!object->bakePath[0]);
    uvTestClick(84,662);object=g_selected;UV_CHECK(object&&object->mesh->lightmap);uvTestClick(904,662);
    printf("UV editor PASS: real UI, cancel, Tank extra UVs, transforms, save/reload, export/reimport, undo/redo, duplicate, torus in ray mode. Artifacts: %s\n",root);
#undef UV_CHECK
    return 0;
}
