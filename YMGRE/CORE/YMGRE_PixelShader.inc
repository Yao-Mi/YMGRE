/* Private flat-basis pixel shader. Dispatch is per triangle, keeping the
   fragment loop free of unrelated lightmap/spotlight/interpolated-basis code. */
#if YMGRE_RASTER_FAST_INTERPOLATION
/* Constant camera depth makes position and UV affine in pixel coordinates.
   This is a geometry invariant, useful for panels, billboards and planar
   surfaces; all other triangles retain the perspective shader below. */
#include "YMGRE_PixelCurve.inc"
static void GRE_RasterFlatBasis(GRE_Vertex4d_wN a,GRE_Vertex4d_wN b,GRE_Vertex4d_wN c,
 GRE_Material material,GRE_Camera4d camera,float32 den,GRE_PixelLight *lights,unsigned count,
 const gre_fvector4d *basisN,const gre_fvector4d *basisT,const gre_fvector4d *basisB,float32 mirror,uint8 power,
 YMGRE_ColorMipView colorMip)
{
 int width=camera->img.width,height=camera->img.height;
 int minX=GREMax(YMGRE_Raster_Floor(GREMin(a->base.pos.x,GREMin(b->base.pos.x,c->base.pos.x))),0);
 int maxX=GREMin(YMGRE_Raster_Ceil(GREMax(a->base.pos.x,GREMax(b->base.pos.x,c->base.pos.x))),width-1);
 int minY=GREMax(YMGRE_Raster_Floor(GREMin(a->base.pos.y,GREMin(b->base.pos.y,c->base.pos.y))),0);
 int maxY=GREMin(YMGRE_Raster_Ceil(GREMax(a->base.pos.y,GREMax(b->base.pos.y,c->base.pos.y))),height-1);
 float32 dy0=b->base.pos.y-c->base.pos.y,dx0=c->base.pos.x-b->base.pos.x;
 float32 dy1=c->base.pos.y-a->base.pos.y,dx1=a->base.pos.x-c->base.pos.x;
 float32 cx=c->base.pos.x,cy=c->base.pos.y;
 float32 qa=1/a->base.pos.z,qb=1/b->base.pos.z,qc=1/c->base.pos.z;
 float32 fx=(camera->perspectPlane.pR-camera->perspectPlane.pL)/(width*camera->perspectPlane.Dis);
 float32 fy=-(camera->perspectPlane.pU-camera->perspectPlane.pD)/(height*camera->perspectPlane.Dis);
 GRErgb24 ambient={0,0,0};
 for(unsigned i=0;i<count;i++)if(lights[i].global) {
  GRE_PixelLight *l=&lights[i];
  ambient.R=GREMin(ambient.R+(int)(l->color.R*l->base.R*l->strength/255.0f),255);
  ambient.G=GREMin(ambient.G+(int)(l->color.G*l->base.G*l->strength/255.0f),255);
  ambient.B=GREMin(ambient.B+(int)(l->color.B*l->base.B*l->strength/255.0f),255);
 }
 GRErgb24 *map=material && material->advanced?material->advanced->normalPixel:NULL;
 if(YMGRE_RASTER_LIGHT_CURVE && a->base.pos.z==b->base.pos.z && a->base.pos.z==c->base.pos.z) {
  GRE_PixelLight *point=NULL;unsigned points=0;
  for(unsigned i=0;i<count;i++)if(!lights[i].global){point=&lights[i];points++;}
  if(points<=1 && (!point || (point->pos.w==1 && point->k0>=0 && point->k1>=0 && point->k2>=0))) {
   GRE_RasterConstantDepth(a,b,c,material,camera,den,point,ambient,basisN,basisT,basisB,mirror,power,colorMip);return;
  }
 }
 uint16 mw=map?material->advanced->normalWidth:0,mh=map?material->advanced->normalHeight:0;
 GRErgb24 previous={0,0,0};uint8 cached=0;
 gre_fvector4d normal=*basisN;float32 invN=1/YMGRE_Fvector4d_Len1(&normal);
 GRErgb24 specColor=material?material->specular:(GRErgb24){255,255,255};
 GRErgb24 tint=a->color;
 GRE_FrameBuffer pixels=camera->img.data;float32 *depths=camera->img.zbuff;
 for(int y=minY;y<=maxY;y++)for(int x=minX;x<=maxX;x++) {
  float32 w0=(dy0*(x-cx)+dx0*(y-cy))/den,w1=(dy1*(x-cx)+dx1*(y-cy))/den,w2=1-w0-w1;
  if(w0<0 || w1<0 || w2<0)continue;
  float32 ia=w0*qa,ib=w1*qb,ic=w2*qc,q=ia+ib+ic;
  if(q<=0)continue;
  float32 z=1/q;unsigned index=y*width+x;
  if(z<=camera->frustum.Znear)continue;
  /* Preserve the reference depth ownership near coplanar surfaces. */
  if(fabsf(depths[index]-z)<2e-6f*z) {
   q=w0/a->base.pos.z+w1/b->base.pos.z+w2/c->base.pos.z;z=1/q;
  }
  if(depths[index]<=z)continue;
  float32 wa=ia*z,wb=ib*z,wc=ic*z;
  float32 u=wa*a->base.u+wb*b->base.u+wc*c->base.u,v=wa*a->base.v+wb*b->base.v+wc*c->base.v;
  if((map && (YMGRE_Raster_TextureBoundary(u,mw)||YMGRE_Raster_TextureBoundary(v,mh))) ||
     (material && material->pixel && (YMGRE_Raster_TextureBoundary(u,material->width)||YMGRE_Raster_TextureBoundary(v,material->height)))) {
   float32 pa=w0/a->base.pos.z,pb=w1/b->base.pos.z,pc=w2/c->base.pos.z,pq=pa+pb+pc;
   wa=pa/pq;wb=pb/pq;wc=pc/pq;
   u=wa*a->base.u+wb*b->base.u+wc*c->base.u;v=wa*a->base.v+wb*b->base.v+wc*c->base.v;
  }
  if(map) {
   GRErgb24 nm=EMaterial_GetPixel(map,mw,mh,u,v);
   if(!cached || nm.R!=previous.R || nm.G!=previous.G || nm.B!=previous.B) {
    float32 nx=nm.R/127.5f-1,ny=nm.G/127.5f-1,nz=nm.B/127.5f-1;
    normal=(gre_fvector4d){basisT->x*nx+basisB->x*ny+basisN->x*nz,
     basisT->y*nx+basisB->y*ny+basisN->y*nz,basisT->z*nx+basisB->z*ny+basisN->z*nz,0};
    YMGRE_Fvector4d_Normalize(&normal);invN=1/YMGRE_Fvector4d_Len1(&normal);
    previous=nm;cached=1;
   }
  }
  gre_fvector4d p={(x-width*.5f)*fx*z,(y-height*.5f)*fy*z,z,1};
  GRErgb24 lighting=ambient,specular={0,0,0};
  for(unsigned i=0;i<count;i++)if(!lights[i].global)
   GRE_ShadePointPrepared(&lights[i],&p,&normal,invN,mirror,power,specColor,&lighting,&specular);
  GRErgb24 texel=EMaterial_GetPixel(colorMip.pixels,colorMip.width,colorMip.height,u,v);
  int r=texel.R*lighting.R/255,g=texel.G*lighting.G/255,bv=texel.B*lighting.B/255;
  r=r*tint.R/255+specular.R;g=g*tint.G/255+specular.G;bv=bv*tint.B/255+specular.B;
  depths[index]=z;pixels[index]=GRE_FramePixel_From_RGB24((GRErgb24){GREMin(r,255),GREMin(g,255),GREMin(bv,255)});
 }
}
#endif
