/* Adaptive scanline lighting for constant-depth, flat-basis triangles.
   Texture sampling preserves texel boundaries and per-pixel depth tests. Only the smooth
   light response inside one constant-normal run is interpolated. */
typedef struct { float32 diffuse,specular; } GRE_LightResponse;
typedef struct {
 GRE_PixelLight *light;
 gre_fvector4d normal;
 float32 invN,lightN,py,z,fx,halfWidth,viewRow,lightRow,dotRow,mirror;
 float32 diffuseWeight,specularWeight;
 uint8 power;
} GRE_LightCurve;
GRE_RASTER_INLINE GRE_LightResponse GRE_EvaluateLightCurve(const GRE_LightCurve *c,int x)
{
 GRE_LightResponse out={0,0};GRE_PixelLight *l=c->light;
 if(!l || (!c->diffuseWeight && !c->specularWeight))return out;
 float32 px=(x-c->halfWidth)*c->fx;
 float32 pn=px*c->normal.x+c->py*c->normal.y+c->z*c->normal.z,dot=c->lightN-pn;
 if(dot<=0)return out;
 float32 lx=l->pos.x-px,len=YMGRE_Sqrt(lx*lx+c->lightRow);
 if(len<=1e-6f)return out;
 float32 il=1/len,atten=l->k0+l->k1*len+l->k2*len*len;
 atten=atten>1e-6f?l->strength/atten:0;
 if(c->diffuseWeight)out.diffuse=atten*dot*c->invN*il/255;
 if(c->specularWeight) {
  float32 v2=px*px+c->viewRow,iv=1/YMGRE_Sqrt(v2);
  float32 ndh=(dot*il-pn*iv)*c->invN,h2=2-2*(l->pos.x*px+c->dotRow-v2)*il*iv;
  if(ndh>0 && h2>1e-16f)out.specular=atten*c->mirror*
   (!(c->power&1)?YMGRE_Light_IntegerPower(ndh*ndh/h2,c->power>>1):YMGRE_Light_IntegerPower(ndh/YMGRE_Sqrt(h2),c->power));
 }
 return out;
}
typedef struct {
 const GRE_LightCurve *curve;
 int x;
 uint32 valid[4];
 GRE_LightResponse exact[128],result[128];
} GRE_CurveSpan;
static GRE_LightResponse GRE_CurveSample(GRE_CurveSpan *s,int i)
{
 uint32 bit=(uint32)1<<(i&31);unsigned word=(unsigned)i>>5;
 if(!(s->valid[word]&bit)){s->exact[i]=GRE_EvaluateLightCurve(s->curve,s->x+i);s->valid[word]|=bit;}
 return s->exact[i];
}
static void GRE_CurveSubdivide(GRE_CurveSpan *s,int first,int last)
{
 GRE_LightResponse a=GRE_CurveSample(s,first),b=GRE_CurveSample(s,last);
 if(last-first<=1){s->result[first]=a;s->result[last]=b;return;}
 float32 inv=1.0f/(last-first);uint8 smooth=1;int mid=(first+last)/2;
 GRE_LightResponse m=GRE_CurveSample(s,mid);float32 tm=(mid-first)*inv;
 GRE_LightResponse q={(m.diffuse-a.diffuse-(b.diffuse-a.diffuse)*tm)/(tm*(tm-1)),(m.specular-a.specular-(b.specular-a.specular)*tm)/(tm*(tm-1))};
 for(int k=1;k<=3;k+=2) {
  int i=first+(last-first)*k/4;GRE_LightResponse v=GRE_CurveSample(s,i);float32 t=(i-first)*inv;
  float32 de=fabsf(v.diffuse-(a.diffuse+(b.diffuse-a.diffuse)*t+q.diffuse*t*(t-1)))*s->curve->diffuseWeight;
  float32 se=fabsf(v.specular-(a.specular+(b.specular-a.specular)*t+q.specular*t*(t-1)))*s->curve->specularWeight;
  if(de+se>YMGRE_RASTER_LIGHT_CURVE_ERROR){smooth=0;break;}
 }
 if(smooth) {
  GRE_LightResponse delta={(b.diffuse-a.diffuse)*inv+q.diffuse*(inv*inv-inv),(b.specular-a.specular)*inv+q.specular*(inv*inv-inv)};
  GRE_LightResponse dd={2*q.diffuse*inv*inv,2*q.specular*inv*inv};
  if(!s->curve->diffuseWeight)for(int i=first;i<=last;i++) {
   s->result[i]=(GRE_LightResponse){0,GREMax(a.specular,0)};a.specular+=delta.specular;delta.specular+=dd.specular;
  }
  else for(int i=first;i<=last;i++) {
   s->result[i]=(GRE_LightResponse){GREMax(a.diffuse,0),GREMax(a.specular,0)};
   a.diffuse+=delta.diffuse;a.specular+=delta.specular;delta.diffuse+=dd.diffuse;delta.specular+=dd.specular;
  }
 } else {GRE_CurveSubdivide(s,first,mid);GRE_CurveSubdivide(s,mid,last);}
}
static void GRE_RasterConstantDepth(GRE_Vertex4d_wN a,GRE_Vertex4d_wN b,GRE_Vertex4d_wN c,
 GRE_Material material,GRE_Camera4d camera,float32 den,GRE_PixelLight *light,GRErgb24 ambient,
 const gre_fvector4d *basisN,const gre_fvector4d *basisT,const gre_fvector4d *basisB,float32 mirror,uint8 power,
 YMGRE_ColorMipView colorMip)
{
 int width=camera->img.width,height=camera->img.height;
 int minX=GREMax(YMGRE_Raster_Floor(GREMin(a->base.pos.x,GREMin(b->base.pos.x,c->base.pos.x))),0);
 int maxX=GREMin(YMGRE_Raster_Ceil(GREMax(a->base.pos.x,GREMax(b->base.pos.x,c->base.pos.x))),width-1);
 int minY=GREMax(YMGRE_Raster_Floor(GREMin(a->base.pos.y,GREMin(b->base.pos.y,c->base.pos.y))),0);
 int maxY=GREMin(YMGRE_Raster_Ceil(GREMax(a->base.pos.y,GREMax(b->base.pos.y,c->base.pos.y))),height-1);
 float32 dy0=b->base.pos.y-c->base.pos.y,dx0=c->base.pos.x-b->base.pos.x;
 float32 dy1=c->base.pos.y-a->base.pos.y,dx1=a->base.pos.x-c->base.pos.x,cx=c->base.pos.x,cy=c->base.pos.y;
 GRE_FrameBuffer colorBuffer=camera->img.data;float32 *depthBuffer=camera->img.zbuff;
 float32 id=1/den,z=a->base.pos.z,gx0=dy0*id,gx1=dy1*id,gx2=-gx0-gx1;
 if(z<=camera->frustum.Znear)return;
 float32 au=a->base.u-c->base.u,bu=b->base.u-c->base.u,av=a->base.v-c->base.v,bv=b->base.v-c->base.v;
 float32 ux=au*gx0+bu*gx1,vx=av*gx0+bv*gx1;
 float32 fx=(camera->perspectPlane.pR-camera->perspectPlane.pL)*z/(width*camera->perspectPlane.Dis);
 float32 fy=-(camera->perspectPlane.pU-camera->perspectPlane.pD)*z/(height*camera->perspectPlane.Dis);
 GRErgb24 *map=material && material->advanced?material->advanced->normalPixel:NULL;
 uint16 mw=map?material->advanced->normalWidth:0,mh=map?material->advanced->normalHeight:0;
 uint8 oneTexel=colorMip.pixels && colorMip.width==1 && colorMip.height==1;
 GRErgb24 texture=oneTexel?colorMip.pixels[0]:DefaultPolygonColor,tint=a->color;
 GRErgb24 specColor=material?material->specular:(GRErgb24){255,255,255};
 GRErgb24 diffuse=light?light->base:(GRErgb24){0,0,0};
 float32 dr=light?light->color.R*diffuse.R:0,dg=light?light->color.G*diffuse.G:0,db=light?light->color.B*diffuse.B:0;
 float32 sr=light?light->color.R*specColor.R/255.0f:0,sg=light?light->color.G*specColor.G/255.0f:0,sb=light?light->color.B*specColor.B/255.0f:0;
 GRE_LightCurve curve;curve.light=light;curve.z=z;curve.fx=fx;curve.halfWidth=width*.5f;curve.mirror=mirror;curve.power=power;
 curve.diffuseWeight=GREMax(dr,GREMax(dg,db));curve.specularWeight=mirror>0?GREMax(sr,GREMax(sg,sb)):0;
 uint8 fixedBase=oneTexel && !curve.diffuseWeight;
 GRErgb24 base={texture.R*ambient.R/255*tint.R/255,texture.G*ambient.G/255*tint.G/255,texture.B*ambient.B/255*tint.B/255};
 uint32 normalKeys[8];gre_fvector4d normalValues[8];float32 normalInverse[8];unsigned cachedNormals=0,nextNormal=0;
 int cachedMapRow=-1;uint32 mapRowKeys[64];uint16 mapRowEnds[64];
 for(int y=minY;y<=maxY;y++) {
  float32 w0=(dy0*(minX-cx)+dx0*(y-cy))*id,w1=(dy1*(minX-cx)+dx1*(y-cy))*id;
  float32 ws[3]={w0,w1,1-w0-w1},gs[3]={gx0,gx1,gx2};int first=minX,last=maxX;
  for(unsigned k=0;k<3;k++) {
   if(gs[k]>1e-12f)first=GREMax(first,YMGRE_Raster_Ceil(minX-ws[k]/gs[k]-1e-3f));
   else if(gs[k]<-1e-12f)last=GREMin(last,YMGRE_Raster_Floor(minX-ws[k]/gs[k]+1e-3f));
   else if(ws[k]<-1e-6f){first=1;last=0;}
  }
  while(first<=last) {
   float32 wa=(dy0*(first-cx)+dx0*(y-cy))/den,wb=(dy1*(first-cx)+dx1*(y-cy))/den;
   if(wa>=0 && wb>=0 && 1-wa-wb>=0)break;first++;
  }
  while(first<=last) {
   float32 wa=(dy0*(last-cx)+dx0*(y-cy))/den,wb=(dy1*(last-cx)+dx1*(y-cy))/den;
   if(wa>=0 && wb>=0 && 1-wa-wb>=0)break;last--;
  }
  curve.py=(y-height*.5f)*fy;curve.viewRow=curve.py*curve.py+z*z+1;
  float32 ly=light?light->pos.y-curve.py:0,lz=light?light->pos.z-z:0;
  curve.lightRow=ly*ly+lz*lz;curve.dotRow=light?light->pos.y*curve.py+light->pos.z*z+1:0;
  uint32 rowMap[128];GRErgb24 rowTexel[128];uint8 mask[128];
  for(int start=first;start<=last;start+=128) {
   int length=GREMin(128,last-start+1);
   float32 sa=(dy0*(start-cx)+dx0*(y-cy))*id,sbary=(dy1*(start-cx)+dx1*(y-cy))*id;
   float32 u0=c->base.u+sa*au+sbary*bu,v0=c->base.v+sa*av+sbary*bv;
   for(int i=0;i<length;i++) {
    int x=start+i;unsigned index=y*width+x;mask[i]=depthBuffer[index]>z;
    if(!mask[i])continue;
    float32 u=u0+i*ux,v=v0+i*vx;
    uint8 boundary=0;int mx=0,my=0;
    if(map){mx=GRE_VertexTexelCoordinate(u,mw,&boundary);my=GRE_VertexTexelCoordinate(v,mh,&boundary);}
    if(map && mw<=64 && oneTexel && vx==0 && ux>0 && !boundary && u>=0 && u0+(length-1)*ux<1 && v>=0 && v<1) {
     if(cachedMapRow!=my) {
      cachedMapRow=my;
      for(int k=mw-1;k>=0;k--) {
       GRErgb24 nm=map[my*mw+k];mapRowKeys[k]=(uint32)nm.R|((uint32)nm.G<<8)|((uint32)nm.B<<16);
       mapRowEnds[k]=(k+1<mw && mapRowKeys[k]==mapRowKeys[k+1])?mapRowEnds[k+1]:k+1;
      }
     }
     uint32 nmKey=mapRowKeys[mx];int edge=mapRowEnds[mx];
     int take=(int)(((float32)edge/mw-u)/ux)-1;
     take=GREMax(1,GREMin(take,length-i));
     for(int k=i;k<i+take;k++) {
      mask[k]=depthBuffer[y*width+start+k]>z;
      rowMap[k]=nmKey;if(!fixedBase)rowTexel[k]=texture;
     }
     i+=take-1;continue;
    }
    if(!oneTexel && colorMip.pixels)boundary|=YMGRE_Raster_TextureBoundary(u,colorMip.width)||YMGRE_Raster_TextureBoundary(v,colorMip.height);
    if(boundary) {
     float32 wa=(dy0*(x-cx)+dx0*(y-cy))/den,wb=(dy1*(x-cx)+dx1*(y-cy))/den,wc=1-wa-wb;
     float32 ia=wa/z,ib=wb/z,ic=wc/z,q=ia+ib+ic;
     u=(ia/q)*a->base.u+(ib/q)*b->base.u+(ic/q)*c->base.u;v=(ia/q)*a->base.v+(ib/q)*b->base.v+(ic/q)*c->base.v;
    }
    GRErgb24 nm=map?(boundary?EMaterial_GetPixel(map,mw,mh,u,v):map[my*mw+mx]):(GRErgb24){128,128,255};
    rowMap[i]=(uint32)nm.R|((uint32)nm.G<<8)|((uint32)nm.B<<16);
    if(!fixedBase)rowTexel[i]=oneTexel?texture:EMaterial_GetPixel(colorMip.pixels,colorMip.width,colorMip.height,u,v);
   }
   for(int i=0;i<length;) {
    if(!mask[i]){i++;continue;}int end=i;
    while(end+1<length && mask[end+1] && rowMap[end+1]==rowMap[i])end++;
    unsigned found=0;
    while(found<cachedNormals && normalKeys[found]!=rowMap[i])found++;
    if(found==cachedNormals) {
     found=nextNormal;nextNormal=(nextNormal+1)&7;if(cachedNormals<8)cachedNormals++;
     normalKeys[found]=rowMap[i];curve.normal=*basisN;
     if(map) {
      float32 nx=(rowMap[i]&255)/127.5f-1,ny=((rowMap[i]>>8)&255)/127.5f-1,nz=(rowMap[i]>>16)/127.5f-1;
      curve.normal=(gre_fvector4d){basisT->x*nx+basisB->x*ny+basisN->x*nz,basisT->y*nx+basisB->y*ny+basisN->y*nz,basisT->z*nx+basisB->z*ny+basisN->z*nz,0};
      YMGRE_Fvector4d_Normalize(&curve.normal);
     }
     normalValues[found]=curve.normal;normalInverse[found]=1/YMGRE_Fvector4d_Len1(&curve.normal);
    }
    curve.normal=normalValues[found];curve.invN=normalInverse[found];
    curve.lightN=light?light->pos.x*curve.normal.x+light->pos.y*curve.normal.y+light->pos.z*curve.normal.z:0;
    GRE_CurveSpan span;span.curve=&curve;span.x=start+i;for(unsigned k=0;k<4;k++)span.valid[k]=0;
    /* Restrict sample spacing using light distance and shininess. Close
       lights and sharp highlights receive finer samples before refinement. */
    float32 distance=light?fabsf(light->pos.z-z):z;
    int safe=(int)(4*distance/(GREMax(fabsf(fx),1e-6f)*YMGRE_Sqrt((float32)power+1)));
    safe=GREMax(1,GREMin(safe,32));
    for(int f=0;f<=end-i;f+=safe)GRE_CurveSubdivide(&span,f,GREMin(f+safe-1,end-i));
    if(fixedBase)for(int j=i;j<=end;j++) {
     float32 spec=span.result[j-i].specular;
     int r=base.R+(uint32)(sr*spec),g=base.G+(uint32)(sg*spec),bv=base.B+(uint32)(sb*spec);
     unsigned index=y*width+start+j;depthBuffer[index]=z;
     colorBuffer[index]=GRE_FramePixel_From_RGB24((GRErgb24){GRE_CLAMP_COLOR8(r),GRE_CLAMP_COLOR8(g),GRE_CLAMP_COLOR8(bv)});
    }
    else     for(int j=i;j<=end;j++) {
     GRE_LightResponse response=span.result[j-i];GRErgb24 t=rowTexel[j];
     int lr=GREMin(ambient.R+(uint32)(dr*response.diffuse),255),lg=GREMin(ambient.G+(uint32)(dg*response.diffuse),255),lb=GREMin(ambient.B+(uint32)(db*response.diffuse),255);
     int r=(t.R*lr/255)*tint.R/255+(uint32)(sr*response.specular);
     int g=(t.G*lg/255)*tint.G/255+(uint32)(sg*response.specular);
     int bv=(t.B*lb/255)*tint.B/255+(uint32)(sb*response.specular);
     unsigned index=y*width+start+j;depthBuffer[index]=z;
     colorBuffer[index]=GRE_FramePixel_From_RGB24((GRErgb24){GREMin(r,255),GREMin(g,255),GREMin(bv,255)});
    }
    i=end+1;
   }
  }
 }
}
