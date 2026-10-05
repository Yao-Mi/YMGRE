#include "YMGRE_Color.h"
#include "../CONFIG/YMGRE_Mem.h"
#include <math.h>
#include <stdint.h>
#if YMGRE_ENABLE_LINEAR_COLOR
#include "YMGRE_ColorLUT.inc"
const float32* YMGRE_Color_DecodeTable(void){return decodeLut;}
float32 YMGRE_Color_Decode(uint8 value){return decodeLut[value];}
uint8 YMGRE_Color_Output(float32 linear)
{
 if(!(linear>0))return 0;
 if(linear>=16)return 255; // fitted curve reaches display white before this point
 return outputLut[(unsigned)(linear*4096+.5f)];
}
int YMGRE_Color_BeginFrame(GRE_Camera4d cam,GRE_RenderWorkspace ws,GRErgb24 background)
{
 if(!cam||!ws)return 0;
 cam->linearColor=NULL;
 if(!cam->linearColorEnabled)return 1;
 size_t pixels=(size_t)cam->img.width*cam->img.height;
 if(pixels>SIZE_MAX/(3*sizeof(float32)))return 0;
 size_t count=pixels*3;
 if(count>ws->linearCapacity){
  if(!ws->ownsMemory)return 0;
  float32* data=GRE_malloc1(count*sizeof(float32));if(!data)return 0;
  GRE_free1(ws->linearColor);ws->linearColor=data;ws->linearCapacity=count;
 }
 if(!ws->linearColor)return 0;
 cam->linearColor=ws->linearColor;
 float r=decodeLut[background.R],g=decodeLut[background.G],b=decodeLut[background.B];
 for(size_t i=0;i<count;i+=3){cam->linearColor[i]=r;cam->linearColor[i+1]=g;cam->linearColor[i+2]=b;}
 return 1;
}
void YMGRE_Color_EndFrame(GRE_Camera4d cam)
{
 if(!cam||!cam->linearColorEnabled||!cam->linearColor)return;
 float exposure=isfinite(cam->exposure)&&cam->exposure>=0?cam->exposure:1;
 for(size_t i=0;i<(size_t)cam->img.width*cam->img.height;i++){
  float* p=cam->linearColor+3*i;
  cam->img.data[i]=GRE_FramePixel_From_RGB24((GRErgb24){YMGRE_Color_Output(p[0]*exposure),YMGRE_Color_Output(p[1]*exposure),YMGRE_Color_Output(p[2]*exposure)});
 }
}
#endif
