#ifndef YMGRE_MATERIAL_RASTER_H
#define YMGRE_MATERIAL_RASTER_H
#include "YMGRE_Color.h"
#include <math.h>
static inline GRErgb24 YMGRE_Material_Texel(const GRErgb24* p,uint16 w,uint16 h,float u,float v)
{
 if(!p||!w||!h)return (GRErgb24){128,128,128};
 int x=(int)(fabsf(u-(int)u)*w+1e-5f),y=(int)(fabsf(v-(int)v)*h+1e-5f);
 if(x>=w)x=w-1;if(y>=h)y=h-1;return p[(size_t)y*w+x];
}
/* LOD is triangle-invariant: compute once, never log2 per fragment. */
static inline unsigned YMGRE_Material_OpacityLOD(GRE_Material m,GRE_Vertex4d_wN a,GRE_Vertex4d_wN b,GRE_Vertex4d_wN c,float den)
{
#if YMGRE_ENABLE_OPACITY_MIPMAP
 if(m&&m->advanced&&m->advanced->opacityPixel&&m->advanced->opacityUseMip&&m->advanced->opacityMipCount>1){
  GRE_MaterialAdvanced q=m->advanced;
  float du0=a->base.u-c->base.u,du1=b->base.u-c->base.u,dv0=a->base.v-c->base.v,dv1=b->base.v-c->base.v;
  float ux=((b->base.pos.y-c->base.pos.y)*du0+(c->base.pos.y-a->base.pos.y)*du1)/den;
  float vx=((b->base.pos.y-c->base.pos.y)*dv0+(c->base.pos.y-a->base.pos.y)*dv1)/den;
  float uy=((c->base.pos.x-b->base.pos.x)*du0+(a->base.pos.x-c->base.pos.x)*du1)/den;
  float vy=((c->base.pos.x-b->base.pos.x)*dv0+(a->base.pos.x-c->base.pos.x)*dv1)/den;
  float sx=hypotf(ux*q->opacityWidth,vx*q->opacityHeight),sy=hypotf(uy*q->opacityWidth,vy*q->opacityHeight);
  float footprint=sx>sy?sx:sy;unsigned level=0;
  while(footprint>=2&&level+1<q->opacityMipCount){footprint*=.5f;level++;}
  return level;
 }
#else
 (void)m;(void)a;(void)b;(void)c;(void)den;
#endif
 return 0;
}
static inline uint8 YMGRE_Material_Opacity(GRE_Material m,float u,float v,unsigned level)
{
#if YMGRE_ENABLE_TRANSPARENCY
 if(m&&m->advanced&&m->advanced->opacityPixel){
  GRE_MaterialAdvanced a=m->advanced;const uint8* p=a->opacityPixel;unsigned w=a->opacityWidth,h=a->opacityHeight;
#if YMGRE_ENABLE_OPACITY_MIPMAP
  if(level<a->opacityMipCount&&a->opacityMipPixels[level]){p=a->opacityMipPixels[level];w=a->opacityMipWidth[level];h=a->opacityMipHeight[level];}
#else
  (void)level;
#endif
  if(!w||!h)return 255;
  int iu=(int)u,iv=(int)v;float fu=u-iu,fv=v-iv;if(fu<0)fu+=1;if(fv<0)fv+=1;
  unsigned x=(unsigned)(fu*w),y=(unsigned)(fv*h);if(x>=w)x=w-1;if(y>=h)y=h-1;
  return p[(size_t)y*w+x];
 }
#else
 (void)m;(void)u;(void)v;(void)level;
#endif
 return 255;
}
static inline int YMGRE_Material_AcceptAlpha(GRE_Camera4d cam,uint8 alpha)
{
#if YMGRE_ENABLE_TRANSPARENCY
 return alpha && !(cam->opacityPass==1&&alpha<255) && !(cam->opacityPass==2&&alpha==255);
#else
 (void)cam;return alpha!=0;
#endif
}
static inline void YMGRE_Material_WriteRGB(GRE_Camera4d cam,size_t i,float z,GRErgb24 c,uint8 alpha)
{
 if(!alpha)return;
 if(alpha==255)cam->img.zbuff[i]=z;
#if YMGRE_ENABLE_LINEAR_COLOR
 if(cam->linearColorEnabled&&cam->linearColor){
  float a=alpha/255.f,*p=cam->linearColor+3*i;
  p[0]=YMGRE_Color_Decode(c.R)*a+p[0]*(1-a);p[1]=YMGRE_Color_Decode(c.G)*a+p[1]*(1-a);p[2]=YMGRE_Color_Decode(c.B)*a+p[2]*(1-a);return;
 }
#endif
 if(alpha<255){
  GRErgb24 b=GRE_FramePixel_To_RGB24(cam->img.data[i]);
  c=(GRErgb24){(c.R*alpha+b.R*(255-alpha)+127)/255,(c.G*alpha+b.G*(255-alpha)+127)/255,(c.B*alpha+b.B*(255-alpha)+127)/255};
 }
 cam->img.data[i]=GRE_FramePixel_From_RGB24(c);
}
#endif
