#include "scene_texture_image.h"
#include "YMGUI_Canvas.h"
#include <png.h>
#include <jpeglib.h>
static uint64_t paintBufferHash(GYOBJ canvas)
{return signatureBytes(0,YMGUI_Canvas_GetBuffer(canvas),(size_t)YMGUI_Canvas_GetW(canvas)*YMGUI_Canvas_GetH(canvas)*sizeof(GYpx));}
static GYOBJ textureFileDialog(void)
{GYOBJ card=uvTestWidget(YMGUI_Ctx_GetTopLayer(g_rightPanel->ctx),72,88,880,640);return card?card->parent:NULL;}
static int chooseTextureFile(const char* root,const char* name)
{
    GYOBJ fd=textureFileDialog();if(!fd||!YMGUI_FileDialog_IsShown(fd)||!YMGUI_FileDialog_Navigate(fd,root))return 0;
    for(unsigned i=0;i<YMGUI_FileDialog_GetEntryCount(fd);i++)if(!strcmp(YMGUI_FileDialog_GetEntry(fd,i)->name,name))return YMGUI_FileDialog_SelectIndex(fd,i)&&YMGUI_FileDialog_Confirm(fd);
    return 0;
}
static int runTextureToolsSelfTest(void)
{
#define TEX_CHECK(c) do{if(!(c)){fprintf(stderr,"Texture tools line %d: %s\n",__LINE__,#c);return 1;}}while(0)
    char temporary[]="/tmp/ymgre-texture-tools-XXXXXX",path[4096],scene[4096],exported[4096],error[256];
    const char* root=getenv("YMGRE_TEXTURE_OUTPUT_DIR");if(!root)root=mkdtemp(temporary);TEX_CHECK(root);
    unsigned char rgba[64*32*4];GRErgb24 rgb[64*32];
    for(unsigned y=0;y<32;y++)for(unsigned x=0;x<64;x++){unsigned i=y*64+x;rgba[i*4]=rgb[i].R=x<32?220:30;rgba[i*4+1]=rgb[i].G=y<16?50:200;rgba[i*4+2]=rgb[i].B=90;rgba[i*4+3]=x<32?255:128;}
    png_image png={.version=PNG_IMAGE_VERSION,.width=64,.height=32,.format=PNG_FORMAT_RGBA};
    snprintf(path,sizeof(path),"%s/pattern.png",root);TEX_CHECK(png_image_write_to_file(&png,path,0,rgba,0,NULL));
    SceneTextureImage decoded={0};TEX_CHECK(SceneTextureImage_Read(path,&decoded,error,sizeof(error))&&decoded.width==64&&decoded.height==32);
    TEX_CHECK(decoded.pixels[0].R==220&&decoded.pixels[0].G==50&&decoded.pixels[32].R==142&&decoded.pixels[32].G==152);
    snprintf(path,sizeof(path),"%s/pattern.bmp",root);YMGRE_Image_LoadTo_Bmp_File(path,rgb,64,32);TEX_CHECK(SceneTextureImage_Read(path,&decoded,error,sizeof(error))&&!memcmp(decoded.pixels,rgb,sizeof(rgb)));
    snprintf(path,sizeof(path),"%s/pattern.jpg",root);FILE* f=fopen(path,"wb");TEX_CHECK(f);struct jpeg_compress_struct jpeg={0};struct jpeg_error_mgr je;jpeg.err=jpeg_std_error(&je);jpeg_create_compress(&jpeg);jpeg_stdio_dest(&jpeg,f);jpeg.image_width=64;jpeg.image_height=32;jpeg.input_components=3;jpeg.in_color_space=JCS_RGB;jpeg_set_defaults(&jpeg);jpeg_set_quality(&jpeg,95,TRUE);jpeg_start_compress(&jpeg,TRUE);
    unsigned char row[64*3];while(jpeg.next_scanline<32){for(unsigned x=0;x<64;x++){GRErgb24 c=rgb[jpeg.next_scanline*64+x];row[x*3]=c.R;row[x*3+1]=c.G;row[x*3+2]=c.B;}JSAMPROW p=row;jpeg_write_scanlines(&jpeg,&p,1);}jpeg_finish_compress(&jpeg);jpeg_destroy_compress(&jpeg);fclose(f);
    TEX_CHECK(SceneTextureImage_Read(path,&decoded,error,sizeof(error))&&decoded.width==64&&abs(decoded.pixels[4].R-220)<5);
    GRErgb24* retained=decoded.pixels;
    for(unsigned i=0;i<2;i++) {
        const char* ext=i?"jpg":"png";snprintf(path,sizeof(path),"%s/pattern.%s",root,ext);f=fopen(path,"rb");TEX_CHECK(f);
        TEX_CHECK(!fseek(f,0,SEEK_END));long size=ftell(f);TEX_CHECK(size>32&&!fseek(f,0,SEEK_SET));unsigned char* partial=malloc(size/2);TEX_CHECK(partial&&fread(partial,1,size/2,f)==(size_t)size/2);fclose(f);
        snprintf(path,sizeof(path),"%s/truncated.%s",root,ext);f=fopen(path,"wb");TEX_CHECK(f&&fwrite(partial,1,size/2,f)==(size_t)size/2);fclose(f);free(partial);
        TEX_CHECK(!SceneTextureImage_Read(path,&decoded,error,sizeof(error))&&decoded.pixels==retained);
    }
    unsigned char oversized[26]={'B','M'};oversized[19]=32;oversized[22]=1;
    snprintf(path,sizeof(path),"%s/oversized.bmp",root);f=fopen(path,"wb");TEX_CHECK(f);fwrite(oversized,1,sizeof(oversized),f);fclose(f);TEX_CHECK(!SceneTextureImage_Read(path,&decoded,error,sizeof(error))&&decoded.pixels==retained);
    snprintf(path,sizeof(path),"%s/broken.bmp",root);f=fopen(path,"wb");TEX_CHECK(f);fwrite("BMbad",1,5,f);fclose(f);TEX_CHECK(!SceneTextureImage_Read(path,&decoded,error,sizeof(error))&&decoded.pixels==retained);SceneTextureImage_Free(&decoded);
    createNewScene();SceneEditorObject* object=opticsObject(SCENE_PLACE_SPHERE,"Painted sphere",0,18,0,1,0xFFFFFFFF);TEX_CHECK(object&&uvTestOpen());
    uint32 history=g_historyIndex;uvTestClick(900,56);uvTestClick(900,127);renderEditorFrame();
    YMGUI_Inject_Key(27,1);renderEditorFrame();TEX_CHECK(g_historyIndex==history&&!object->texturePath[0]);
    uvTestClick(900,56);uvTestClick(900,127);renderEditorFrame();
    GYOBJ canvas=uvTestWidget(YMGUI_Ctx_GetTopLayer(g_rightPanel->ctx),264,112,528,560);TEX_CHECK(canvas&&YMGUI_Canvas_GetW(canvas)==512&&YMGUI_Canvas_GetH(canvas)==256);
    uint64_t blank=paintBufferHash(canvas);YMGUI_Inject_Pointer(400,392,1);YMGUI_Inject_Pointer(560,392,1);YMGUI_Inject_Pointer(560,392,0);renderEditorFrame();uint64_t stroke=paintBufferHash(canvas);TEX_CHECK(stroke!=blank&&g_historyIndex==history&&!object->texturePath[0]);
    uvTestClick(74,690);renderEditorFrame();TEX_CHECK(paintBufferHash(canvas)==blank);uvTestClick(182,690);renderEditorFrame();TEX_CHECK(paintBufferHash(canvas)==stroke);
    /* Keyboard undo belongs to the paint canvas, never to the scene underneath. */
    YMGUI_SetFocus(canvas->ctx,canvas);YMGUI_Inject_Key(GY_KEY_UNDO,1);renderEditorFrame();TEX_CHECK(paintBufferHash(canvas)==blank&&g_historyIndex==history);YMGUI_Inject_Key(0x110E,1);renderEditorFrame();TEX_CHECK(paintBufferHash(canvas)==stroke);
    uvTestClick(852,366);uvTestClick(150,305);uvTestClick(150,441);YMGUI_Inject_Pointer(440,320,1);YMGUI_Inject_Pointer(610,460,1);YMGUI_Inject_Pointer(610,460,0);renderEditorFrame();TEX_CHECK(paintBufferHash(canvas)!=stroke);
    uint64_t shapes=paintBufferHash(canvas);
    uvTestClick(150,305);uvTestClick(150,551);uvTestClick(500,350);renderEditorFrame();TEX_CHECK(paintBufferHash(canvas)!=shapes);
    uvTestClick(74,690);renderEditorFrame();TEX_CHECK(paintBufferHash(canvas)==shapes);
    uvTestClick(150,305);uvTestClick(150,353);uvTestClick(500,320);renderEditorFrame();TEX_CHECK(paintBufferHash(canvas)!=shapes);
    uvTestClick(74,690);renderEditorFrame();TEX_CHECK(paintBufferHash(canvas)==shapes);
    rendererSaveScreen(root,"paint-editor");uvTestClick(900,40);renderEditorFrame();TEX_CHECK(g_historyIndex==history&&!object->texturePath[0]);
    /* Turn the painted hemisphere toward the preview camera and verify its color. */
    YMGUI_Inject_Pointer(570,310,1);YMGUI_Inject_Pointer(967,310,1);YMGUI_Inject_Pointer(967,310,0);renderEditorFrame();
    unsigned redPixels=0;for(int y=112;y<512;y++)for(int x=556;x<956;x++) {
        GYcolor c=GY_PxToColor(g_ui->host.display.buf1[y*g_ui->host.display.hor_res+x]);unsigned r=(c>>16)&255,g=(c>>8)&255,b=c&255;
        if(r>g*2&&r>b*2&&r>80)redPixels++;
    }
    TEX_CHECK(redPixels>1000);rendererSaveScreen(root,"paint-model-preview");
    uvTestClick(550,56);TEX_CHECK(object->texturePath[0]&&g_historyIndex==history+1);GRE_Material material=YMGRE_Material_Find(&g_importContext.MaterialList,object->mesh->materiaName);TEX_CHECK(material&&material->width==512&&material->height==256);
    size_t bytes=512*256*sizeof(GRErgb24);GRErgb24* painted=malloc(bytes);TEX_CHECK(painted);memcpy(painted,material->pixel,bytes);
    history=g_historyIndex;uvTestClick(294,56);renderEditorFrame();canvas=uvTestWidget(YMGUI_Ctx_GetTopLayer(g_rightPanel->ctx),264,112,528,560);TEX_CHECK(canvas);YMGUI_Inject_Pointer(360,330,1);YMGUI_Inject_Pointer(630,430,1);YMGUI_Inject_Pointer(630,430,0);renderEditorFrame();uvTestClick(700,40);TEX_CHECK(g_historyIndex==history&&!memcmp(material->pixel,painted,bytes));
    uvTestClick(84,662);object=g_selected;TEX_CHECK(object&&!object->texturePath[0]);uvTestClick(192,662);object=g_selected;TEX_CHECK(object&&object->texturePath[0]);uvTestClick(904,662);
    snprintf(scene,sizeof(scene),"%s/painted.scene",root);TEX_CHECK(writeCurrentScene(scene,error,sizeof(error)));loadSceneFromPath(scene,NULL);object=uvTestFindObject();TEX_CHECK(object);material=YMGRE_Material_Find(&g_importContext.MaterialList,object->mesh->materiaName);TEX_CHECK(material&&!memcmp(material->pixel,painted,bytes));
    TEX_CHECK(SceneModel_Export(object->mesh,&g_importContext.MaterialList,NULL,NULL,root,"Painted",exported,sizeof(exported),error,sizeof(error)));gre_scence imported={0};GRE_Object4d mesh=YMGRE_LoadOgreMeshAndMaterial(&imported,exported);TEX_CHECK(mesh);material=YMGRE_Material_Find(&imported.MaterialList,mesh->materiaName);TEX_CHECK(material&&material->width==512&&!memcmp(material->pixel,painted,bytes));YMGRE_Free_Object(mesh);YMGRE_List_Clear(&imported.MaterialList,YMGRE_Free_Material);free(painted);
    selectObject(object,"图片导入验证");TEX_CHECK(uvTestOpen());history=g_historyIndex;uvTestClick(900,56);uvTestClick(900,149);TEX_CHECK(textureFileDialog()&&YMGUI_FileDialog_IsShown(textureFileDialog()));uvTestClick(906,708);TEX_CHECK(!YMGUI_FileDialog_IsShown(textureFileDialog())&&g_historyIndex==history);
    uvTestClick(900,56);uvTestClick(900,149);TEX_CHECK(chooseTextureFile(root,"pattern.png"));renderEditorFrame();TEX_CHECK(g_historyIndex==history);uint64_t preview=uvTestPreviewHash();rendererSaveScreen(root,"imported-image-preview");
    uvTestClick(294,56);TEX_CHECK(chooseTextureFile(root,"broken.bmp"));renderEditorFrame();TEX_CHECK(g_historyIndex==history&&uvTestPreviewHash()==preview);
    uvTestClick(406,56);renderEditorFrame();canvas=uvTestWidget(YMGUI_Ctx_GetTopLayer(g_rightPanel->ctx),264,112,528,560);
    TEX_CHECK(canvas&&YMGUI_Canvas_GetW(canvas)==64&&YMGUI_Canvas_GetH(canvas)==32);
    GYcolor seed=GY_PxToColor(YMGUI_Canvas_GetBuffer(canvas)[32]);TEX_CHECK(((seed>>16)&255)==142&&((seed>>8)&255)==152);
    uvTestClick(700,40);renderEditorFrame();TEX_CHECK(g_historyIndex==history&&uvTestPreviewHash()==preview);
    uvTestClick(550,56);TEX_CHECK(g_historyIndex==history+1);material=YMGRE_Material_Find(&g_importContext.MaterialList,object->mesh->materiaName);TEX_CHECK(material&&material->width==64&&material->height==32&&material->pixel[32].R==142);
    snprintf(path,sizeof(path),"%s/pattern.png",root);TEX_CHECK(unlink(path)==0);uvTestClick(904,662);snprintf(scene,sizeof(scene),"%s/imported.scene",root);TEX_CHECK(writeCurrentScene(scene,error,sizeof(error)));loadSceneFromPath(scene,NULL);object=uvTestFindObject();material=object?YMGRE_Material_Find(&g_importContext.MaterialList,object->mesh->materiaName):NULL;TEX_CHECK(material&&material->width==64&&material->pixel[32].R==142);
    printf("Texture tools PASS: PNG/JPEG/BMP, alpha, failed import, real paint/layers/shapes, undo/redo, cancellation, scene isolation, apply, save/reload, export/reimport and independent image assets. Artifacts: %s\n",root);return 0;
#undef TEX_CHECK
}
