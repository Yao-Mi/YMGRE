#include "../CONFIG/YMGRE_Profile.h"
#include "YMGRE_MaterialRaster.h"
#include "YMGRE_PBR.h"
#include "./YMGRE_TriangleRaster.h"
#include "./YMGRE_MathBase.h"
#include "./YMGRE_Light.h"

extern GRErgb24 GRE_brush;

#define YMGRE_RASTER_AREA_EPSILON 1e-5f
#define GRE_RASTER_INLINE static inline
#define GRE_CLAMP_COLOR8(value) ((uint8)GREMin(GREMax((value),0),255))

static inline uint8 EMaterial_GetSpecularPower(GRE_Material material)
{
	return material && material->advanced && material->advanced->specularPower > 0 ?
		material->advanced->specularPower : 30;
}

//光栅化热路径使用整数转换完成取整，避免 MCU 上逐行调用 libm
static inline int YMGRE_Raster_Ceil(float32 value)
{
	int result = (int)value;
	return result + (value > result);
}

static inline int YMGRE_Raster_Floor(float32 value)
{
	int result = (int)value;
	return result - (value < result);
}

/* UV fallback near discontinuous texel/repeat boundaries. */
static inline uint8 YMGRE_Raster_TextureBoundary(float32 uv, uint16 extent)
{
 if (!extent) return 0;
 float32 coordinate=YMGRE_Fabs(uv-(int)uv)*extent+1e-5f;
 float32 fraction=coordinate-(int)coordinate;
 float32 tolerance=1e-5f*extent*(YMGRE_Fabs(uv)+1.0f);
 return fraction<tolerance || fraction>1.0f-tolerance;
}
static inline int GRE_VertexTexelCoordinate(float32 uv,uint16 extent,uint8 *boundary)
{
 float32 coordinate=YMGRE_Fabs(uv-(int)uv)*extent+1e-5f;
 int pixel=(int)coordinate;
 float32 fraction=coordinate-pixel,tolerance=1e-5f*extent*(YMGRE_Fabs(uv)+1);
 *boundary |= fraction<tolerance || fraction>1-tolerance;
 return GREMin(pixel,extent-1);
}

/////////////////////////////////////////// 平面着色器 --三角形快速光栅化//////////////////////////////////////////////////////////////

//获取贴图材质的颜色
static inline GRErgb24 EMaterial_GetPixel(GRErgb24* bitmap, uint16 width, uint16 height, float32 u, float32 v)
{
	if (bitmap && width > 0 && height > 0)
	{
		int x = YMGRE_Fabs(u - (int)u) * width + 1e-5f;
		int y = YMGRE_Fabs(v - (int)v) * height + 1e-5f;
		if (x >= width) x = width - 1;
		if (y >= height) y = height - 1;
		return bitmap[y * width + x];
	}
	else
		return DefaultPolygonColor;
}

void YMGRE_TriangleRaster_FillVertexColor_wN(GRE_Vertex4d_wN vertexList,
	GRE_Polygon4d polygon,GRE_Camera4d camera)
{
	if(vertexList==NULL||polygon==NULL||camera==NULL) return;
	GRE_Vertex4d_wN a=&vertexList[polygon->index[0]];
	GRE_Vertex4d_wN b=&vertexList[polygon->index[1]];
	GRE_Vertex4d_wN c=&vertexList[polygon->index[2]];
	float32 den=(b->base.pos.y-c->base.pos.y)*(a->base.pos.x-c->base.pos.x)+
		(c->base.pos.x-b->base.pos.x)*(a->base.pos.y-c->base.pos.y);
	if (YMGRE_Fabs(den)<1e-6f) return;
	int minX=GREMax(YMGRE_Raster_Floor(GREMin(a->base.pos.x,
		GREMin(b->base.pos.x,c->base.pos.x))),0);
	int maxX=GREMin(YMGRE_Raster_Ceil(GREMax(a->base.pos.x,
		GREMax(b->base.pos.x,c->base.pos.x))),camera->img.width-1);
	int minY=GREMax(YMGRE_Raster_Floor(GREMin(a->base.pos.y,
		GREMin(b->base.pos.y,c->base.pos.y))),0);
	int maxY=GREMin(YMGRE_Raster_Ceil(GREMax(a->base.pos.y,
		GREMax(b->base.pos.y,c->base.pos.y))),camera->img.height-1);
	for(int y=minY;y<=maxY;y++) for(int x=minX;x<=maxX;x++)
	{
		float32 w0=((b->base.pos.y-c->base.pos.y)*(x-c->base.pos.x)+
			(c->base.pos.x-b->base.pos.x)*(y-c->base.pos.y))/den;
		float32 w1=((c->base.pos.y-a->base.pos.y)*(x-c->base.pos.x)+
			(a->base.pos.x-c->base.pos.x)*(y-c->base.pos.y))/den;
		float32 w2=1.0f-w0-w1;
		if(w0<0||w1<0||w2<0) continue;
		float32 iw0=w0/a->base.pos.z,iw1=w1/b->base.pos.z,iw2=w2/c->base.pos.z;
		float32 invz=iw0+iw1+iw2;
		if(invz<=0) continue;
		float32 wa=iw0/invz,wb=iw1/invz,wc=iw2/invz,z=1.0f/invz;
		int index=y*camera->img.width+x;
		if(camera->img.zbuff[index]<=z||z<=camera->frustum.Znear) continue;
		camera->img.zbuff[index]=z;
		float32 red=wa*a->color.R+wb*b->color.R+wc*c->color.R;
		float32 green=wa*a->color.G+wb*b->color.G+wc*c->color.G;
		float32 blue=wa*a->color.B+wb*b->color.B+wc*c->color.B;
		camera->img.data[index]=GRE_FramePixel_From_RGB24((GRErgb24){
			(uint8)GREMin(GREMax(red,0),255),(uint8)GREMin(GREMax(green,0),255),
			(uint8)GREMin(GREMax(blue,0),255)});
	}
}

void YMGRE_TriangleRaster_ComputeVertexLighting_wN(GRE_Vertex4d_wN vertices,
	uint16 vertexCount, GRE_Polygon4d polygon, GRE_Material material,
	GRE_List lights, gre_fvector4d* lightPos, GRE_FMat4x4 worldToCamera,
	float32 mirrorKs)
{
	YMGRE_TriangleRaster_ComputeVertexLightingMasked_wN(vertices, vertexCount, polygon,
		material, lights, lightPos, worldToCamera, mirrorKs, NULL);
}

#if YMGRE_PROFILE_RENDER_STAGES
#define YMGRE_TriangleRaster_ComputeVertexLightingMasked_wN YMGRE_TriangleRaster_ComputeVertexLightingMasked_wN_Measured
#endif
void YMGRE_TriangleRaster_ComputeVertexLightingMasked_wN(GRE_Vertex4d_wN vertices,
	uint16 vertexCount, GRE_Polygon4d polygon, GRE_Material material,
	GRE_List lights, gre_fvector4d* lightPos, GRE_FMat4x4 worldToCamera,
	float32 mirrorKs, const uint8* active)
{
	if (vertices == NULL || vertexCount == 0 || polygon == NULL || lights == NULL || worldToCamera == NULL) return;
	uint8 specularPower = EMaterial_GetSpecularPower(material);
	for (uint16 i = 0; i < vertexCount; i++) {
		if (active && !active[i]) continue;
		vertices[i].vertexLighting = (GRErgb24){0, 0, 0};
		vertices[i].vertexSpecular = (GRErgb24){0, 0, 0};
	}
	/* Prepare each light once; each vertex retains the original light order. */
	gre_polygon4d litPolygon = *polygon;
	uint32 lightIndex = 0;
	for (GRE_ListNode node = lights->listhead; node != NULL; node = node->next) {
		gre_light4d light = *(GRE_Light4d)node->data;
		litPolygon.planeColor = (material && light.type == GRE_GlobalLight) ?
			material->ambient : (material ? material->diffuse : (GRErgb24){255, 255, 255});
		light.proper.pos_ = lightPos[lightIndex++];
		if (light.type == GRE_SpotLight) {
			YMGRE_Fvector4d_MatMultTo(worldToCamera, &light.proper.spot.direct,
				&light.proper.spot.direct);
			light.proper.spot.direct.w = 0;
		}
		for (uint16 i = 0; i < vertexCount; i++)
			if (!active || active[i]) YMGRE_PolygonLighting_ComponentsAdvanced(&litPolygon,
				&vertices[i].base.pos, &vertices[i].normal, &light,
				&vertices[i].vertexLighting, &vertices[i].vertexSpecular, mirrorKs,
				specularPower, material ? material->specular : (GRErgb24){255, 255, 255});
	}
}
#if YMGRE_PROFILE_RENDER_STAGES
#undef YMGRE_TriangleRaster_ComputeVertexLightingMasked_wN
void YMGRE_TriangleRaster_ComputeVertexLightingMasked_wN(GRE_Vertex4d_wN vertices,
	uint16 vertexCount, GRE_Polygon4d polygon, GRE_Material material,
	GRE_List lights, gre_fvector4d* lightPos, GRE_FMat4x4 worldToCamera,
	float32 mirrorKs, const uint8* active)
{
 uint32 start=YMGRE_ProfileNow();
 YMGRE_TriangleRaster_ComputeVertexLightingMasked_wN_Measured(vertices,vertexCount,polygon,material,lights,lightPos,worldToCamera,mirrorKs,active);
 YMGRE_ProfileCycles[3]+=YMGRE_ProfileNow()-start;
}
#endif


#if YMGRE_RASTER_FAST_INTERPOLATION
/* Affine attribute/z planes. Values advance by addition across a scanline;
   division is shared only by the surviving fragment's attributes. */
typedef struct { float32 base,dx,dy; } GRE_AttributePlane;
GRE_RASTER_INLINE GRE_AttributePlane GRE_MakeAttributePlane(float32 a,float32 b,float32 c,
 float32 w0,float32 w1,float32 w0dx,float32 w1dx,float32 w0dy,float32 w1dy)
{
 GRE_AttributePlane p;
 p.base=c+w0*(a-c)+w1*(b-c);
 p.dx=w0dx*(a-c)+w1dx*(b-c); p.dy=w0dy*(a-c)+w1dy*(b-c);
 return p;
}
/* Reuse wrapped texel coordinates for boundary checking and sampling. */
static void GRE_RasterTexturedPlanes(GRE_Vertex4d_wN a,GRE_Vertex4d_wN b,
 GRE_Vertex4d_wN c,GRE_Material material,GRE_Camera4d camera,float32 den)
{
 const int width=camera->img.width,height=camera->img.height;
 const float32 znear=camera->frustum.Znear;
 float32 *depthBuffer=camera->img.zbuff;GRE_FrameBuffer colorBuffer=camera->img.data;
 float32 boundEpsilon=1e-5f*(GREMax(YMGRE_Fabs(a->base.pos.x),GREMax(YMGRE_Fabs(b->base.pos.x),YMGRE_Fabs(c->base.pos.x)))+
  GREMax(YMGRE_Fabs(a->base.pos.y),GREMax(YMGRE_Fabs(b->base.pos.y),YMGRE_Fabs(c->base.pos.y)))+1);
 int minX=GREMax(YMGRE_Raster_Ceil(GREMin(a->base.pos.x,GREMin(b->base.pos.x,c->base.pos.x))-boundEpsilon),0);
 int maxX=GREMin(YMGRE_Raster_Floor(GREMax(a->base.pos.x,GREMax(b->base.pos.x,c->base.pos.x))+boundEpsilon),width-1);
 int minY=GREMax(YMGRE_Raster_Ceil(GREMin(a->base.pos.y,GREMin(b->base.pos.y,c->base.pos.y))-boundEpsilon),0);
 int maxY=GREMin(YMGRE_Raster_Floor(GREMax(a->base.pos.y,GREMax(b->base.pos.y,c->base.pos.y))+boundEpsilon),height-1);
 float32 dy0=b->base.pos.y-c->base.pos.y,dx0=c->base.pos.x-b->base.pos.x;
 float32 dy1=c->base.pos.y-a->base.pos.y,dx1=a->base.pos.x-c->base.pos.x;
 float32 cx=c->base.pos.x,cy=c->base.pos.y,invDen=1.0f/den;
 float32 w0start=(dy0*(minX-cx)+dx0*(minY-cy))*invDen;
 float32 w1start=(dy1*(minX-cx)+dx1*(minY-cy))*invDen;
 float32 q0=1.0f/a->base.pos.z,q1=1.0f/b->base.pos.z,q2=1.0f/c->base.pos.z;
 float32 w0dx=dy0*invDen,w1dx=dy1*invDen,w0dy=dx0*invDen,w1dy=dx1*invDen;
 #define GRE_PLANE(a,b,c) GRE_MakeAttributePlane(a,b,c,w0start,w1start,w0dx,w1dx,w0dy,w1dy)
 GRE_AttributePlane up=GRE_PLANE(a->base.u*q0,b->base.u*q1,c->base.u*q2);
 GRE_AttributePlane vp=GRE_PLANE(a->base.v*q0,b->base.v*q1,c->base.v*q2);
 float32 sr=a->color.R/(255.0f*255.0f),sg=a->color.G/(255.0f*255.0f),sb=a->color.B/(255.0f*255.0f);
 GRE_AttributePlane rp=GRE_PLANE(a->vertexLighting.R*q0*sr,b->vertexLighting.R*q1*sr,c->vertexLighting.R*q2*sr);
 GRE_AttributePlane gp=GRE_PLANE(a->vertexLighting.G*q0*sg,b->vertexLighting.G*q1*sg,c->vertexLighting.G*q2*sg);
 GRE_AttributePlane bp=GRE_PLANE(a->vertexLighting.B*q0*sb,b->vertexLighting.B*q1*sb,c->vertexLighting.B*q2*sb);
 #undef GRE_PLANE
 YMGRE_ColorMipView colorMip=YMGRE_Material_ColorMipForTriangle(material,a,b,c);
 GRErgb24 *texture=colorMip.pixels;
 uint16 tw=colorMip.width,th=colorMip.height;
 for(int y=minY;y<=maxY;y++) {
  int rowMin=minX,rowMax=maxX;
  if((maxX-minX)*(maxY-minY)>128) {
  float32 wrow[3]={ (dy0*(minX-cx)+dx0*(y-cy))*invDen,
                   (dy1*(minX-cx)+dx1*(y-cy))*invDen,0 };
  float32 wx[3]={w0dx,w1dx,-w0dx-w1dx};
  wrow[2]=1.0f-wrow[0]-wrow[1];
  for(int edge=0;edge<3;edge++) {
   if(wx[edge]>1e-9f) rowMin=GREMax(rowMin,YMGRE_Raster_Floor(minX-wrow[edge]/wx[edge])-2);
   else if(wx[edge]<-1e-9f) rowMax=GREMin(rowMax,YMGRE_Raster_Ceil(minX-wrow[edge]/wx[edge])+2);
  }
  }
  if(rowMin>rowMax) continue;
  float32 ry=(float32)(y-minY);
  float32 u=up.base+ry*up.dy,v=vp.base+ry*vp.dy;
  float32 r=rp.base+ry*rp.dy,g=gp.base+ry*gp.dy,bl=bp.base+ry*bp.dy;
  float32 skip=(float32)(rowMin-minX);
  u+=skip*up.dx;v+=skip*vp.dx;r+=skip*rp.dx;g+=skip*gp.dx;bl+=skip*bp.dx;
  for(int x=rowMin;x<=rowMax;x++,u+=up.dx,v+=vp.dx,r+=rp.dx,g+=gp.dx,bl+=bp.dx) {
   float32 n0=dy0*(x-cx)+dx0*(y-cy),w0=n0*invDen;
   if(w0<0) continue;
   float32 n1=dy1*(x-cx)+dx1*(y-cy),w1=n1*invDen,w2=1.0f-w0-w1;
   if(w1<0) continue;
   if(YMGRE_Fabs(w2)<1e-5f) {w0=n0/den;w1=n1/den;w2=1.0f-w0-w1;}
   if(w2<0) continue;
   float32 depth=1.0f/(w0*q0+w1*q1+w2*q2);
   int index=y*width+x;
   float32 priorDepth=depthBuffer[index];
   if(YMGRE_Fabs(priorDepth-depth)<2e-6f*depth) {
    float32 pw0=n0/den,pw1=n1/den,pw2=1-pw0-pw1;
    depth=1.0f/(pw0/a->base.pos.z+pw1/b->base.pos.z+pw2/c->base.pos.z);
   }
   if(priorDepth<=depth || depth<=znear) continue;
   float32 uv=u*depth,vv=v*depth;
   GRErgb24 texel=DefaultPolygonColor;
   if(texture && tw && th) {
   uint8 boundary=0;
   int tx=GRE_VertexTexelCoordinate(uv,tw,&boundary),ty=GRE_VertexTexelCoordinate(vv,th,&boundary);
   if(boundary) {
    float32 ew0=n0/den,ew1=n1/den,ew2=1.0f-ew0-ew1;
    float32 eq0=ew0/a->base.pos.z,eq1=ew1/b->base.pos.z,eq2=ew2/c->base.pos.z,eq=eq0+eq1+eq2;
    float32 wa=eq0/eq,wb=eq1/eq,wc=eq2/eq;
    uv=wa*a->base.u+wb*b->base.u+wc*c->base.u;vv=wa*a->base.v+wb*b->base.v+wc*c->base.v;
    tx=(int)(YMGRE_Fabs(uv-(int)uv)*tw+1e-5f);ty=(int)(YMGRE_Fabs(vv-(int)vv)*th+1e-5f);
    tx=GREMin(tx,tw-1);ty=GREMin(ty,th-1);
   }
   texel=texture[ty*tw+tx];
   }
   int cr=(int)(texel.R*r*depth),cg=(int)(texel.G*g*depth),cb=(int)(texel.B*bl*depth);
   depthBuffer[index]=depth;
   colorBuffer[index]=GRE_FramePixel_From_RGB24((GRErgb24){
    GRE_CLAMP_COLOR8(cr),GRE_CLAMP_COLOR8(cg),GRE_CLAMP_COLOR8(cb)});
  }
 }
}
/* Achromatic tint and lighting share one scalar perspective plane. */
static void GRE_RasterTexturedScalarPlanes(GRE_Vertex4d_wN a,GRE_Vertex4d_wN b,
 GRE_Vertex4d_wN c,GRE_Material material,GRE_Camera4d camera,float32 den)
{
 const int width=camera->img.width,height=camera->img.height;
 const float32 znear=camera->frustum.Znear;
 float32 *depthBuffer=camera->img.zbuff;GRE_FrameBuffer colorBuffer=camera->img.data;
 float32 boundEpsilon=1e-5f*(GREMax(YMGRE_Fabs(a->base.pos.x),GREMax(YMGRE_Fabs(b->base.pos.x),YMGRE_Fabs(c->base.pos.x)))+
  GREMax(YMGRE_Fabs(a->base.pos.y),GREMax(YMGRE_Fabs(b->base.pos.y),YMGRE_Fabs(c->base.pos.y)))+1);
 int minX=GREMax(YMGRE_Raster_Ceil(GREMin(a->base.pos.x,GREMin(b->base.pos.x,c->base.pos.x))-boundEpsilon),0);
 int maxX=GREMin(YMGRE_Raster_Floor(GREMax(a->base.pos.x,GREMax(b->base.pos.x,c->base.pos.x))+boundEpsilon),width-1);
 int minY=GREMax(YMGRE_Raster_Ceil(GREMin(a->base.pos.y,GREMin(b->base.pos.y,c->base.pos.y))-boundEpsilon),0);
 int maxY=GREMin(YMGRE_Raster_Floor(GREMax(a->base.pos.y,GREMax(b->base.pos.y,c->base.pos.y))+boundEpsilon),height-1);
 float32 dy0=b->base.pos.y-c->base.pos.y,dx0=c->base.pos.x-b->base.pos.x;
 float32 dy1=c->base.pos.y-a->base.pos.y,dx1=a->base.pos.x-c->base.pos.x;
 float32 cx=c->base.pos.x,cy=c->base.pos.y,invDen=1.0f/den;
 float32 w0start=(dy0*(minX-cx)+dx0*(minY-cy))*invDen;
 float32 w1start=(dy1*(minX-cx)+dx1*(minY-cy))*invDen;
 float32 q0=1.0f/a->base.pos.z,q1=1.0f/b->base.pos.z,q2=1.0f/c->base.pos.z;
 float32 w0dx=dy0*invDen,w1dx=dy1*invDen,w0dy=dx0*invDen,w1dy=dx1*invDen;
 #define GRE_PLANE(a,b,c) GRE_MakeAttributePlane(a,b,c,w0start,w1start,w0dx,w1dx,w0dy,w1dy)
 GRE_AttributePlane up=GRE_PLANE(a->base.u*q0,b->base.u*q1,c->base.u*q2);
 GRE_AttributePlane vp=GRE_PLANE(a->base.v*q0,b->base.v*q1,c->base.v*q2);
 float32 sr=a->color.R/(255.0f*255.0f);
 GRE_AttributePlane rp=GRE_PLANE(a->vertexLighting.R*q0*sr,b->vertexLighting.R*q1*sr,c->vertexLighting.R*q2*sr);
 #undef GRE_PLANE
 YMGRE_ColorMipView colorMip=YMGRE_Material_ColorMipForTriangle(material,a,b,c);
 GRErgb24 *texture=colorMip.pixels;
 uint16 tw=colorMip.width,th=colorMip.height;
 for(int y=minY;y<=maxY;y++) {
  int rowMin=minX,rowMax=maxX;
  if((maxX-minX)*(maxY-minY)>128) {
  float32 wrow[3]={ (dy0*(minX-cx)+dx0*(y-cy))*invDen,
                   (dy1*(minX-cx)+dx1*(y-cy))*invDen,0 };
  float32 wx[3]={w0dx,w1dx,-w0dx-w1dx};
  wrow[2]=1.0f-wrow[0]-wrow[1];
  for(int edge=0;edge<3;edge++) {
   if(wx[edge]>1e-9f) rowMin=GREMax(rowMin,YMGRE_Raster_Floor(minX-wrow[edge]/wx[edge])-2);
   else if(wx[edge]<-1e-9f) rowMax=GREMin(rowMax,YMGRE_Raster_Ceil(minX-wrow[edge]/wx[edge])+2);
  }
  }
  if(rowMin>rowMax) continue;
  float32 ry=(float32)(y-minY);
  float32 u=up.base+ry*up.dy,v=vp.base+ry*vp.dy;
  float32 r=rp.base+ry*rp.dy;
  float32 skip=(float32)(rowMin-minX);
  u+=skip*up.dx;v+=skip*vp.dx;r+=skip*rp.dx;
  for(int x=rowMin;x<=rowMax;x++,u+=up.dx,v+=vp.dx,r+=rp.dx) {
   float32 n0=dy0*(x-cx)+dx0*(y-cy),w0=n0*invDen;
   if(w0<0) continue;
   float32 n1=dy1*(x-cx)+dx1*(y-cy),w1=n1*invDen,w2=1.0f-w0-w1;
   if(w1<0) continue;
   if(YMGRE_Fabs(w2)<1e-5f) {w0=n0/den;w1=n1/den;w2=1.0f-w0-w1;}
   if(w2<0) continue;
   float32 depth=1.0f/(w0*q0+w1*q1+w2*q2);
   int index=y*width+x;
   float32 priorDepth=depthBuffer[index];
   if(YMGRE_Fabs(priorDepth-depth)<2e-6f*depth) {
    float32 pw0=n0/den,pw1=n1/den,pw2=1-pw0-pw1;
    depth=1.0f/(pw0/a->base.pos.z+pw1/b->base.pos.z+pw2/c->base.pos.z);
   }
   if(priorDepth<=depth || depth<=znear) continue;
   float32 uv=u*depth,vv=v*depth;
   GRErgb24 texel=DefaultPolygonColor;
   if(texture && tw && th) {
   uint8 boundary=0;
   int tx=GRE_VertexTexelCoordinate(uv,tw,&boundary),ty=GRE_VertexTexelCoordinate(vv,th,&boundary);
   if(boundary) {
    float32 ew0=n0/den,ew1=n1/den,ew2=1.0f-ew0-ew1;
    float32 eq0=ew0/a->base.pos.z,eq1=ew1/b->base.pos.z,eq2=ew2/c->base.pos.z,eq=eq0+eq1+eq2;
    float32 wa=eq0/eq,wb=eq1/eq,wc=eq2/eq;
    uv=wa*a->base.u+wb*b->base.u+wc*c->base.u;vv=wa*a->base.v+wb*b->base.v+wc*c->base.v;
    tx=(int)(YMGRE_Fabs(uv-(int)uv)*tw+1e-5f);ty=(int)(YMGRE_Fabs(vv-(int)vv)*th+1e-5f);
    tx=GREMin(tx,tw-1);ty=GREMin(ty,th-1);
   }
   texel=texture[ty*tw+tx];
   }
   float32 shade=r*depth;
   int cr=(int)(texel.R*shade),cg=(int)(texel.G*shade),cb=(int)(texel.B*shade);
   depthBuffer[index]=depth;
   colorBuffer[index]=GRE_FramePixel_From_RGB24((GRErgb24){
    GRE_CLAMP_COLOR8(cr),GRE_CLAMP_COLOR8(cg),GRE_CLAMP_COLOR8(cb)});
  }
 }
}
#endif


#if YMGRE_PROFILE_RENDER_STAGES
#define YMGRE_TriangleRaster_FillVertexLit_wN YMGRE_TriangleRaster_FillVertexLit_wN_Measured
#endif
void YMGRE_TriangleRaster_FillVertexLit_wN(GRE_Vertex4d_wN vertexList,
	GRE_Polygon4d polygon, GRE_Material material, GRE_Camera4d camera)
{
	if (vertexList == NULL || polygon == NULL || camera == NULL) return;
	GRE_Vertex4d_wN a = &vertexList[polygon->index[0]];
	GRE_Vertex4d_wN b = &vertexList[polygon->index[1]];
	GRE_Vertex4d_wN c = &vertexList[polygon->index[2]];
	float32 den = (b->base.pos.y-c->base.pos.y)*(a->base.pos.x-c->base.pos.x)+
		(c->base.pos.x-b->base.pos.x)*(a->base.pos.y-c->base.pos.y);
	if (YMGRE_Fabs(den) < 1e-6f) return;
#if YMGRE_RASTER_FAST_INTERPOLATION
	uint8 direct = 1;
#if YMGRE_ENABLE_LINEAR_COLOR
	if (camera->linearColorEnabled) direct = 0;
#endif
#if YMGRE_ENABLE_TRANSPARENCY
	if (camera->opacityPass == 2 || (material && material->advanced && material->advanced->opacityPixel)) direct = 0;
#endif
	float32 zmin = GREMin(a->base.pos.z,GREMin(b->base.pos.z,c->base.pos.z));
	float32 zmax = GREMax(a->base.pos.z,GREMax(b->base.pos.z,c->base.pos.z));
	float32 spanX = GREMax(a->base.pos.x,GREMax(b->base.pos.x,c->base.pos.x))-
		GREMin(a->base.pos.x,GREMin(b->base.pos.x,c->base.pos.x));
	float32 spanY = GREMax(a->base.pos.y,GREMax(b->base.pos.y,c->base.pos.y))-
		GREMin(a->base.pos.y,GREMin(b->base.pos.y,c->base.pos.y));
	uint8 constantBoundary = material && material->pixel &&
		((a->base.u==b->base.u && a->base.u==c->base.u &&
		YMGRE_Raster_TextureBoundary(a->base.u,material->width)) ||
		(a->base.v==b->base.v && a->base.v==c->base.v &&
		YMGRE_Raster_TextureBoundary(a->base.v,material->height)));
	if (direct && spanX<=32 && spanY<=32 && !constantBoundary && YMGRE_Fabs(den)>0.01f &&
		zmin>1e-6f && zmax<zmin*8 &&
		a->color.R==b->color.R && a->color.R==c->color.R &&
		a->color.G==b->color.G && a->color.G==c->color.G &&
		a->color.B==b->color.B && a->color.B==c->color.B &&
		!(a->vertexSpecular.R||a->vertexSpecular.G||a->vertexSpecular.B||
		  b->vertexSpecular.R||b->vertexSpecular.G||b->vertexSpecular.B||
		  c->vertexSpecular.R||c->vertexSpecular.G||c->vertexSpecular.B)) {
		if(a->color.R==a->color.G && a->color.R==a->color.B &&
		   a->vertexLighting.R==a->vertexLighting.G && a->vertexLighting.R==a->vertexLighting.B &&
		   b->vertexLighting.R==b->vertexLighting.G && b->vertexLighting.R==b->vertexLighting.B &&
		   c->vertexLighting.R==c->vertexLighting.G && c->vertexLighting.R==c->vertexLighting.B)
			GRE_RasterTexturedScalarPlanes(a,b,c,material,camera,den);
		else GRE_RasterTexturedPlanes(a,b,c,material,camera,den);
		return;
	}
#endif
 unsigned lod=YMGRE_Material_OpacityLOD(material,a,b,c,den);
	YMGRE_ColorMipView colorMip=YMGRE_Material_ColorMipForTriangle(material,a,b,c);
	int minX = GREMax(YMGRE_Raster_Floor(GREMin(a->base.pos.x,
		GREMin(b->base.pos.x,c->base.pos.x))), 0);
	int maxX = GREMin(YMGRE_Raster_Ceil(GREMax(a->base.pos.x,
		GREMax(b->base.pos.x,c->base.pos.x))), camera->img.width-1);
	int minY = GREMax(YMGRE_Raster_Floor(GREMin(a->base.pos.y,
		GREMin(b->base.pos.y,c->base.pos.y))), 0);
	int maxY = GREMin(YMGRE_Raster_Ceil(GREMax(a->base.pos.y,
		GREMax(b->base.pos.y,c->base.pos.y))), camera->img.height-1);
	/* The reciprocal is invariant across this triangle. Opacity maps retain the
	 * reference arithmetic to avoid shifting a coverage threshold. */
	int fast = YMGRE_RASTER_FAST_INTERPOLATION;
#if YMGRE_ENABLE_TRANSPARENCY
	if (material && material->advanced && material->advanced->opacityPixel) fast = 0;
#endif
	float32 qa = fast ? 1.0f/a->base.pos.z : 0;
	float32 qb = fast ? 1.0f/b->base.pos.z : 0;
	float32 qc = fast ? 1.0f/c->base.pos.z : 0;
	for (int y = minY; y <= maxY; y++) for (int x = minX; x <= maxX; x++)
	{
		float32 w0=((b->base.pos.y-c->base.pos.y)*(x-c->base.pos.x)+
			(c->base.pos.x-b->base.pos.x)*(y-c->base.pos.y))/den;
		float32 w1=((c->base.pos.y-a->base.pos.y)*(x-c->base.pos.x)+
			(a->base.pos.x-c->base.pos.x)*(y-c->base.pos.y))/den;
		float32 w2=1.0f-w0-w1;
		if (w0 < 0 || w1 < 0 || w2 < 0) continue;
		float32 iw0=fast?w0*qa:w0/a->base.pos.z;
		float32 iw1=fast?w1*qb:w1/b->base.pos.z;
		float32 iw2=fast?w2*qc:w2/c->base.pos.z;
		float32 invz=iw0+iw1+iw2;
		if (invz <= 0) continue;
		float32 wa=iw0/invz, wb=iw1/invz, wc=iw2/invz, z=1.0f/invz;
		int index=y*camera->img.width+x;
		if (fast && YMGRE_Fabs(camera->img.zbuff[index]-z) < 2e-6f*z) {
			/* Preserve ownership at nearly coplanar depth crossings. */
			iw0=w0/a->base.pos.z;
			iw1=w1/b->base.pos.z;
			iw2=w2/c->base.pos.z;
			invz=iw0+iw1+iw2;
			z=1.0f/invz;
			wa=iw0/invz; wb=iw1/invz; wc=iw2/invz;
		}
		if (camera->img.zbuff[index] <= z || z <= camera->frustum.Znear) continue;
		float32 u=wa*a->base.u+wb*b->base.u+wc*c->base.u;
		float32 v=wa*a->base.v+wb*b->base.v+wc*c->base.v;
		uint8 alpha=YMGRE_Material_Opacity(material,u,v,lod);
		if(!YMGRE_Material_AcceptAlpha(camera,alpha))continue;
		GRErgb24 texel=EMaterial_GetPixel(colorMip.pixels,colorMip.width,colorMip.height,u,v);
		float32 lr=wa*a->vertexLighting.R+wb*b->vertexLighting.R+wc*c->vertexLighting.R;
		float32 lg=wa*a->vertexLighting.G+wb*b->vertexLighting.G+wc*c->vertexLighting.G;
		float32 lb=wa*a->vertexLighting.B+wb*b->vertexLighting.B+wc*c->vertexLighting.B;
		float32 sr=wa*a->vertexSpecular.R+wb*b->vertexSpecular.R+wc*c->vertexSpecular.R;
		float32 sg=wa*a->vertexSpecular.G+wb*b->vertexSpecular.G+wc*c->vertexSpecular.G;
		float32 sb=wa*a->vertexSpecular.B+wb*b->vertexSpecular.B+wc*c->vertexSpecular.B;
		float32 vr=wa*a->color.R+wb*b->color.R+wc*c->color.R;
		float32 vg=wa*a->color.G+wb*b->color.G+wc*c->color.G;
		float32 vb=wa*a->color.B+wb*b->color.B+wc*c->color.B;
		int cr=(int)(texel.R*lr*vr/(255.0f*255.0f)+sr);
		int cg=(int)(texel.G*lg*vg/(255.0f*255.0f)+sg);
		int cb=(int)(texel.B*lb*vb/(255.0f*255.0f)+sb);
		YMGRE_Material_WriteRGB(camera,index,z,(GRErgb24){
			(uint8)GREMin(GREMax(cr,0),255), (uint8)GREMin(GREMax(cg,0),255),
			(uint8)GREMin(GREMax(cb,0),255) },alpha);
	}
}
#if YMGRE_PROFILE_RENDER_STAGES
#undef YMGRE_TriangleRaster_FillVertexLit_wN
void YMGRE_TriangleRaster_FillVertexLit_wN(GRE_Vertex4d_wN vertexList,
	GRE_Polygon4d polygon, GRE_Material material, GRE_Camera4d camera)
{
 uint32 start=YMGRE_ProfileNow();
 YMGRE_TriangleRaster_FillVertexLit_wN_Measured(vertexList,polygon,material,camera);
 YMGRE_ProfileCycles[5]+=YMGRE_ProfileNow()-start;
}
#endif


typedef struct { gre_fvector4d pos;float32 strength,k0,k1,k2;GRErgb24 color,base;uint8 global; } GRE_PixelLight;
GRE_RASTER_INLINE void GRE_ShadePointPrepared(const GRE_PixelLight *l,const gre_fvector4d *p,
 const gre_fvector4d *n,float32 invN,float32 mirror,uint8 power,GRErgb24 specColor,
 GRErgb24 *base,GRErgb24 *spec)
{
 float32 lx=l->pos.x-p->x,ly=l->pos.y-p->y,lz=l->pos.z-p->z,lw=l->pos.w-p->w;
 float32 dot=lx*n->x+ly*n->y+lz*n->z+lw*n->w;
 if(dot<=0)return;
 float32 len=YMGRE_Sqrt(lx*lx+ly*ly+lz*lz+lw*lw);if(len<=1e-6f)return;
 float32 attenuation=l->k0+l->k1*len+l->k2*len*len;
 attenuation=attenuation>1e-6f?1.0f/attenuation:0;
 attenuation*=l->strength;
 float32 lr=l->color.R*attenuation,lg=l->color.G*attenuation,lb=l->color.B*attenuation;
 float32 cosine=dot*invN/(len*255);
 base->R=GREMin(base->R+(uint32)(lr*l->base.R*cosine),255);
 base->G=GREMin(base->G+(uint32)(lg*l->base.G*cosine),255);
 base->B=GREMin(base->B+(uint32)(lb*l->base.B*cosine),255);
 if(mirror>0 && (specColor.R || specColor.G || specColor.B)) {
  float32 il=1.0f/len;
  float32 iv=1.0f/YMGRE_Sqrt(p->x*p->x+p->y*p->y+p->z*p->z+p->w*p->w);
  float32 hx=lx*il-p->x*iv,hy=ly*il-p->y*iv,hz=lz*il-p->z*iv,hw=lw*il-p->w*iv;
  float32 ndh=(hx*n->x+hy*n->y+hz*n->z+hw*n->w)*invN;
  float32 highlight=0;
  if(ndh>0) {
   float32 h2=hx*hx+hy*hy+hz*hz+hw*hw;
   if(h2>1e-16f) {
    if(!(power&1))highlight=YMGRE_Light_IntegerPower(ndh*ndh/h2,power>>1);
    else highlight=YMGRE_Light_IntegerPower(ndh/YMGRE_Sqrt(h2),power);
   }
  }
  spec->R=GREMin(spec->R+(uint32)(lr*specColor.R/255.0f*mirror*highlight),255);
  spec->G=GREMin(spec->G+(uint32)(lg*specColor.G/255.0f*mirror*highlight),255);
  spec->B=GREMin(spec->B+(uint32)(lb*specColor.B/255.0f*mirror*highlight),255);
 }
}
#include "YMGRE_PixelShader.inc"
static void FillAdvanced(GRE_Vertex4d_wN vertexList, GRE_Polygon4d polygon,
	GRE_Material material, GRE_List lights, gre_fvector4d* lightPos, GRE_FMat4x4 worldToCamera,
	float32 mirrorKs, GRE_Camera4d camera, GRE_Lightmap lightmap)
{
	if(vertexList==NULL||polygon==NULL||(lights==NULL && lightmap==NULL)||camera==NULL) return;
#if YMGRE_ENABLE_PBR
 if(camera->pbrEnabled && material && !material->unlit && material->advanced && material->advanced->pbrEnabled && lights && !lightmap){
  YMGRE_PBR_Fill(vertexList,polygon,material,lights,lightPos,worldToCamera,camera);return;
 }
#endif
	uint8 specularPower = EMaterial_GetSpecularPower(material);
	GRE_Vertex4d_wN a = &vertexList[polygon->index[0]], b = &vertexList[polygon->index[1]], c = &vertexList[polygon->index[2]];
	// Tangent handedness is a discrete, triangle-flat attribute. Never
	// interpolate it: interpolating +/-1 creates a zero crossing and a
	// visible 180-degree bitangent flip inside the triangle.
	float32 du1 = b->base.u - a->base.u, dv1 = b->base.v - a->base.v;
	float32 du2 = c->base.u - a->base.u, dv2 = c->base.v - a->base.v;
	float32 handedness = (du1 * dv2 - du2 * dv1 < -1e-8f) ? -1.0f : 1.0f;
	float32 den = (b->base.pos.y - c->base.pos.y) * (a->base.pos.x - c->base.pos.x) +
		(c->base.pos.x - b->base.pos.x) * (a->base.pos.y - c->base.pos.y);
	if (YMGRE_Fabs(den) < 1e-6f) return;
 unsigned lod=YMGRE_Material_OpacityLOD(material,a,b,c,den);
	YMGRE_ColorMipView colorMip=YMGRE_Material_ColorMipForTriangle(material,a,b,c);
	int minX = GREMax(YMGRE_Raster_Floor(GREMin(a->base.pos.x,
		GREMin(b->base.pos.x,c->base.pos.x))),0);
	int maxX = GREMin(YMGRE_Raster_Ceil(GREMax(a->base.pos.x,
		GREMax(b->base.pos.x,c->base.pos.x))),camera->img.width-1);
	int minY = GREMax(YMGRE_Raster_Floor(GREMin(a->base.pos.y,
		GREMin(b->base.pos.y,c->base.pos.y))),0);
	int maxY = GREMin(YMGRE_Raster_Ceil(GREMax(a->base.pos.y,
		GREMax(b->base.pos.y,c->base.pos.y))),camera->img.height-1);
#if YMGRE_RASTER_FAST_INTERPOLATION
 uint8 directColor = 1;
#if YMGRE_ENABLE_LINEAR_COLOR
 if (camera->linearColorEnabled) directColor = 0;
#endif
 if (directColor && !lightmap &&
#if YMGRE_ENABLE_TRANSPARENCY
     !(material && material->advanced && material->advanced->opacityPixel) &&
#endif
     !(material && material->unlit)) {
	GRE_PixelLight prepared[8];unsigned preparedCount=0;
	uint8 canPrepare=!lightmap && lights && !(material && material->unlit);
	if(canPrepare) {
	 unsigned index=0;
	 for(GRE_ListNode ln=lights->listhead;ln;ln=ln->next,index++) {
	  GRE_Light4d l=ln->data;
	  if(preparedCount==8 || (l->type!=GRE_GlobalLight && l->type!=GRE_PointLight) ||
	     (l->type!=GRE_GlobalLight && l->proper.shadowK>0) || l->proper.strength<0) {canPrepare=0;break;}
	  GRE_PixelLight *out=&prepared[preparedCount++];out->pos=lightPos[index];
	  out->strength=l->proper.strength;out->k0=l->proper.kc0;out->k1=l->proper.kc1;out->k2=l->proper.kc2;
	  out->color=l->proper.lightcolor;out->global=l->type==GRE_GlobalLight;
	  out->base=material?(out->global?material->ambient:material->diffuse):polygon->planeColor;
	 }
	}
	uint8 flatBasis=a->normal.x==b->normal.x && a->normal.x==c->normal.x &&
	 a->normal.y==b->normal.y && a->normal.y==c->normal.y && a->normal.z==b->normal.z && a->normal.z==c->normal.z &&
	 a->tangent.x==b->tangent.x && a->tangent.x==c->tangent.x && a->tangent.y==b->tangent.y &&
	 a->tangent.y==c->tangent.y && a->tangent.z==b->tangent.z && a->tangent.z==c->tangent.z;
	gre_fvector4d flatNormal=a->normal,flatTangent=a->tangent,flatBitangent;
	flatNormal.w=flatTangent.w=0;
	if(flatBasis) {
	 YMGRE_Fvector4d_Normalize(&flatNormal);
	 float32 dot=YMGRE_Fvector4d_Dot(&flatNormal,&flatTangent);
	 flatTangent.x-=flatNormal.x*dot;flatTangent.y-=flatNormal.y*dot;flatTangent.z-=flatNormal.z*dot;
	 YMGRE_Fvector4d_Normalize(&flatTangent);
	 YMGRE_Fvector4d_CrossToResult(&flatNormal,&flatTangent,&flatBitangent);
	 if(handedness<0){flatBitangent.x=-flatBitangent.x;flatBitangent.y=-flatBitangent.y;flatBitangent.z=-flatBitangent.z;}
	}
#if YMGRE_RASTER_FAST_INTERPOLATION
	if(canPrepare && flatBasis && a->color.R==b->color.R && a->color.R==c->color.R &&
	 a->color.G==b->color.G && a->color.G==c->color.G && a->color.B==b->color.B && a->color.B==c->color.B &&
	 YMGRE_Fvector4d_Len2(&flatNormal)>1e-12f) {
		 GRE_RasterFlatBasis(a,b,c,material,camera,den,prepared,preparedCount,&flatNormal,&flatTangent,&flatBitangent,mirrorKs,specularPower,colorMip);
	 return;
	}
#endif
 }
#endif
	for (int y = minY; y <= maxY; y++) for (int x = minX; x <= maxX; x++)
	{
		float32 w0 = ((b->base.pos.y-c->base.pos.y)*(x-c->base.pos.x)+(c->base.pos.x-b->base.pos.x)*(y-c->base.pos.y))/den;
		float32 w1 = ((c->base.pos.y-a->base.pos.y)*(x-c->base.pos.x)+(a->base.pos.x-c->base.pos.x)*(y-c->base.pos.y))/den;
		float32 w2 = 1.0f-w0-w1;
		if (w0 < 0 || w1 < 0 || w2 < 0) continue;
		float32 iw0= w0/(a->base.pos.z), iw1=w1/(b->base.pos.z), iw2=w2/(c->base.pos.z), invz=iw0+iw1+iw2;
		if (invz <= 0) continue;
		float32 wa=iw0/invz, wb=iw1/invz, wc=iw2/invz;
		float32 z=1.0f/invz;
		int index=y*camera->img.width+x;
		if (camera->img.zbuff[index] <= z || z <= camera->frustum.Znear) continue;
		float32 u=wa*a->base.u+wb*b->base.u+wc*c->base.u, v=wa*a->base.v+wb*b->base.v+wc*c->base.v;
		uint8 alpha=YMGRE_Material_Opacity(material,u,v,lod);
		if(!YMGRE_Material_AcceptAlpha(camera,alpha))continue;
		gre_fvector4d normal={wa*a->normal.x+wb*b->normal.x+wc*c->normal.x,wa*a->normal.y+wb*b->normal.y+wc*c->normal.y,wa*a->normal.z+wb*b->normal.z+wc*c->normal.z,0};
		gre_fvector4d tangent={wa*a->tangent.x+wb*b->tangent.x+wc*c->tangent.x,wa*a->tangent.y+wb*b->tangent.y+wc*c->tangent.y,wa*a->tangent.z+wb*b->tangent.z+wc*c->tangent.z,0};
		YMGRE_Fvector4d_Normalize(&normal);
		if (material && material->advanced && material->advanced->normalPixel)
		{
			GRErgb24 nm=EMaterial_GetPixel(material->advanced->normalPixel,
				material->advanced->normalWidth,material->advanced->normalHeight,u,v);
			float32 nx=nm.R/127.5f-1.0f, ny=nm.G/127.5f-1.0f, nz=nm.B/127.5f-1.0f;
			float32 ndt=YMGRE_Fvector4d_Dot(&normal,&tangent);
			tangent.x-=normal.x*ndt; tangent.y-=normal.y*ndt; tangent.z-=normal.z*ndt;
			YMGRE_Fvector4d_Normalize(&tangent);
			gre_fvector4d bitangent;
			YMGRE_Fvector4d_CrossToResult(&normal,&tangent,&bitangent);
			if(handedness<0){bitangent.x=-bitangent.x;bitangent.y=-bitangent.y;bitangent.z=-bitangent.z;}
			normal=(gre_fvector4d){tangent.x*nx+bitangent.x*ny+normal.x*nz,tangent.y*nx+bitangent.y*ny+normal.y*nz,tangent.z*nx+bitangent.z*ny+normal.z*nz,0};
			YMGRE_Fvector4d_Normalize(&normal);
		}
		float32 viewW=camera->perspectPlane.pR-camera->perspectPlane.pL, viewH=camera->perspectPlane.pU-camera->perspectPlane.pD;
		gre_fvector4d fragment={((x-camera->img.width*.5f)*viewW/camera->img.width)*z/camera->perspectPlane.Dis,((y-camera->img.height*.5f)*-viewH/camera->img.height)*z/camera->perspectPlane.Dis,z,1};
		gre_polygon4d litPolygon=*polygon;
		/* Built-in editor meshes have no material entry; preserve their polygon color. */
		litPolygon.planeColor=material?material->diffuse:polygon->planeColor;
		GRErgb24 lighting={0,0,0}, specular={0,0,0}; uint32 lightIndex=0;
		if (lightmap != NULL) {
			float32 lu=wa*a->lightmapU+wb*b->lightmapU+wc*c->lightmapU;
			float32 lv=wa*a->lightmapV+wb*b->lightmapV+wc*c->lightmapV;
			int lx=GREMin(GREMax((int)(lu*lightmap->width),0),lightmap->width-1);
			int ly=GREMin(GREMax((int)(lv*lightmap->height),0),lightmap->height-1);
			GRErgb24 irradiance=lightmap->pixels[ly*lightmap->width+lx];
			if(lightmap->specularPixels)specular=lightmap->specularPixels[ly*lightmap->width+lx];
			GRErgb24 albedo=material?material->diffuse:polygon->planeColor;
			lighting=lightmap->colorsBaked?irradiance:(GRErgb24){irradiance.R*albedo.R/255,
				irradiance.G*albedo.G/255,irradiance.B*albedo.B/255};
		}
		if(material && material->unlit)lighting=material->diffuse;
		for(GRE_ListNode ln=(lightmap || (material && material->unlit))?NULL:lights->listhead;ln;ln=ln->next)
		{
			gre_light4d light=*(GRE_Light4d)ln->data;
			litPolygon.planeColor=(material && light.type==GRE_GlobalLight)?material->ambient:
				(material?material->diffuse:polygon->planeColor);
			light.proper.pos_=lightPos[lightIndex++];
			if(light.type==GRE_SpotLight){YMGRE_Fvector4d_MatMultTo(worldToCamera,&light.proper.spot.direct,&light.proper.spot.direct);light.proper.spot.direct.w=0;}
			YMGRE_PolygonLighting_ComponentsAdvanced(&litPolygon, &fragment,
				&normal, &light, &lighting, &specular, mirrorKs, specularPower,
				material ? material->specular : (GRErgb24){ 255, 255, 255 });
		}
		GRErgb24 texel=EMaterial_GetPixel(colorMip.pixels,colorMip.width,colorMip.height,u,v);
		// Advanced lighting uses the standard 0..255 color range. The legacy
		// rasterizer's neutral value of 128 must not be reused here: white light
		// would otherwise double the color and create intersecting saturation bands.
		int cr=texel.R*lighting.R/255, cg=texel.G*lighting.G/255, cb=texel.B*lighting.B/255;
		cr=cr*(wa*a->color.R+wb*b->color.R+wc*c->color.R)/255;
		cg=cg*(wa*a->color.G+wb*b->color.G+wc*c->color.G)/255;
		cb=cb*(wa*a->color.B+wb*b->color.B+wc*c->color.B)/255;
		cr += specular.R; cg += specular.G; cb += specular.B;
		YMGRE_Material_WriteRGB(camera,index,z,(GRErgb24){GREMin(cr,255),GREMin(cg,255),GREMin(cb,255)},alpha);
	}
}
void YMGRE_TriangleRaster_Fill_wN(GRE_Vertex4d_wN vertices, GRE_Polygon4d polygon,
	GRE_Material material, GRE_List lights, gre_fvector4d* positions, GRE_FMat4x4 matrix,
	float32 mirrorKs, GRE_Camera4d camera)
{
	FillAdvanced(vertices, polygon, material, lights, positions, matrix, mirrorKs, camera, NULL);
}

void YMGRE_TriangleRaster_FillLightmap_wN(GRE_Vertex4d_wN vertices,
	GRE_Polygon4d polygon, GRE_Material material, GRE_Lightmap lightmap, GRE_Camera4d camera)
{
	if (!lightmap || !lightmap->pixels || !lightmap->width || !lightmap->height) return;
	FillAdvanced(vertices, polygon, material, NULL, NULL, NULL, 0, camera, lightmap);
}
//////////////////////////////////////////////// 使用材质绘制三角形 /////////////////////////////////////

// 绘制平底为下三角的三角形
//       v0
//       /\
//      /  \
//  v1 ------ v2
//只填充平底三角形，热路径不包含线框判断
// Private scanline inputs: z is reciprocal camera depth; u/v are u/z and v/z.
static inline void Fill_Top_Trangle(float32 x0, float32 y0, float32 z0, float32 u0, float32 v0,
	float32 x1, float32 y1, float32 z1, float32 u1, float32 v1,
	float32 x2, float32 y2, float32 z2, float32 u2, float32 v2, GRErgb24 planecolor, GRE_Material mymater, YMGRE_ColorMipView colorMip, GRE_Camera4d mycam)
{
	if (y2 < 0 || y0 > mycam->img.height - 1)// 高度在图像范围之外
		return;

	uint16 height = mycam->img.height;
	uint16 width = mycam->img.width;
	float32 znear_v = mycam->frustum.Znear;

	float32 div10 = 1.0f / (y1 - y0);
	float32 div20 = div10; // y1 == y2
	//计算v0到两个顶点的直线增长量
	float32 dxdl = (x1 - x0) * div10; // dx L--R
	float32 dxdr = (x2 - x0) * div20;
	float32 dzdl = (z1 - z0) * div10; // dz L--R
	float32 dzdr = (z2 - z0) * div20;

	float32 startL = x0;
	float32 startR = x0;
	float32 zl = 0;
	float32 zr = 0;

	int begX;
	int endX;
	//不使用材质，则采用平面着色
	if ((mymater == NULL) || (mymater->pixel == NULL) || !mymater->valid)
	{
		//扫描线采用左闭右闭、下边界不包含规则，避免填充越过三角形边界
		int begY = GREMax(YMGRE_Raster_Ceil(y0), 0);
		int endY = GREMin(YMGRE_Raster_Ceil(y2), height);
		startL = x0 + (begY - y0) * dxdl;
		startR = x0 + (begY - y0) * dxdr;
		zl = z0 + (begY - y0) * dzdl;
		zr = z0 + (begY - y0) * dzdr;

		for (int y = begY; y < endY; y++)
		{
			begX = YMGRE_Raster_Ceil(startL);
			endX = YMGRE_Raster_Floor(startR);

			//水平方向限幅
			if (begX < 0) begX = 0;
			if (endX > width - 1)
				endX = width - 1;
			if (begX > endX)
			{
				startL += dxdl;
				startR += dxdr;
				zl += dzdl;
				zr += dzdr;
				continue;
			}
			//-----------
			float32* zbuff_i = &mycam->img.zbuff[y * width];//深度缓冲器，该行起点
			GRE_FrameBuffer frame_i = &mycam->img.data[y * width];//帧缓冲区中，该行起点

			float32 zd = (startL == startR) ? 0 : (zr - zl) / (startR - startL);
			float32 invDepth = zl + (begX - startL) * zd;
			//填充固定颜色
			for (int x = begX; x <= endX; x++)
			{
				// One reciprocal per varying-depth fragment, shared by depth and UVs.
				float32 depth = invDepth > 0.0f ? 1.0f / invDepth : 0.0f;
				if (zbuff_i[x] > depth)
				{
					//且在近景平面内
					if (depth > znear_v)
					{
						zbuff_i[x] = depth;
						frame_i[x] = GRE_FramePixel_From_RGB24(planecolor);
					}
				}
				invDepth += zd;
			}
			startL += dxdl; //dx
			startR += dxdr;
			zl += dzdl;
			zr += dzdr;
		}
	}
	else
	{
		float32 dudl = (u1 - u0) * div10; //du
		float32 dudr = (u2 - u0) * div20;
		float32 dvdl = (v1 - v0) * div10; //dv
		float32 dvdr = (v2 - v0) * div20;

		float32 startLU = u0;// L -- R
		float32 startRU = u0;
		float32 startLV = v0;
		float32 startRV = v0;

		//
		float32 begU = 0; // B -- E
		float32 endU = 0;
		float32 begV = 0;
		float32 endV = 0;

		float32 dx = 0;
		float32 ui = 0;
		float32 vi = 0;

		float32 zl = 0;
		float32 zr = 0;
		float32 invDepth = 0;

		int begY = GREMax(YMGRE_Raster_Ceil(y0), 0);
		int endY = GREMin(YMGRE_Raster_Ceil(y2), height);
		startL = x0 + (begY - y0) * dxdl;
		startR = x0 + (begY - y0) * dxdr;
		startLU = u0 + (begY - y0) * dudl;
		startRU = u0 + (begY - y0) * dudr;
		startLV = v0 + (begY - y0) * dvdl;
		startRV = v0 + (begY - y0) * dvdr;
		zl = z0 + (begY - y0) * dzdl;
		zr = z0 + (begY - y0) * dzdr;

		for (int y = begY; y < endY; y++)
		{
			//初始化 L -- R
			begX = YMGRE_Raster_Ceil(startL);
			endX = YMGRE_Raster_Floor(startR);
			//u,v
			begU = startLU; endU = startRU;
			begV = startLV; endV = startRV;
			//计算水平方向插值增量
			dx = startR - startL;
			float32 invWidth = (dx == 0) ? 0 : 1.0f / dx;
			ui = (endU - begU) * invWidth;
			vi = (endV - begV) * invWidth;
			float32 zd = (zr - zl) * invWidth;
			begU += (begX - startL) * ui;
			begV += (begX - startL) * vi;
			invDepth = zl + (begX - startL) * zd;
			//修正x的范围
			if (begX < 0)
			{
				begU -= begX * ui;
				begV -= begX * vi;
				invDepth -= begX * zd;
				begX = 0;
			}
			if (endX > width - 1)
				endX = width - 1;
			//在图像范围内
			if (begX <= endX)
			{
				float32* zbuff_i = &mycam->img.zbuff[y * width];//深度缓冲器，该行起点
				GRE_FrameBuffer frame_i = &mycam->img.data[y * width];//帧缓冲区中，该行起点
				for (int x = begX; x <= endX; x++)
				{
					// One reciprocal per varying-depth fragment, shared by depth and UVs.
					float32 depth = invDepth > 0.0f ? 1.0f / invDepth : 0.0f;
					if (zbuff_i[x] > depth)
					{
						//且在近景平面内
						if (depth > znear_v)
						{
							zbuff_i[x] = depth;
							GRErgb24 texel = EMaterial_GetPixel(colorMip.pixels,colorMip.width,colorMip.height,begU * depth,begV * depth);
							//添加光照影响，最后一步才量化到帧缓冲格式
							int cr = texel.R * planecolor.R / DefaultPolygonClv;
							int cg = texel.G * planecolor.G / DefaultPolygonClv;
							int cb = texel.B * planecolor.B / DefaultPolygonClv;
							texel.R = GREMin(cr, 255);
							texel.G = GREMin(cg, 255);
							texel.B = GREMin(cb, 255);
							frame_i[x] = GRE_FramePixel_From_RGB24(texel);
						}
					}
					begU += ui; begV += vi;
					invDepth += zd;
				}
			}
			//L,R
			startL += dxdl; startR += dxdr;
			//u,v
			startLU += dudl; startLV += dvdl;
			startRU += dudr; startRV += dvdr;
			zl += dzdl; zr += dzdr;
		}
	}
}

// 绘制下三角
//  v1     v0
//   ------
//    \  /
//     \/
//     v2
//只填充平顶三角形，热路径不包含线框判断
static inline void Fill_Botton_Trangle(float32 x0, float32 y0, float32 z0, float32 u0, float32 v0,
	float32 x1, float32 y1, float32 z1, float32 u1, float32 v1,
	float32 x2, float32 y2, float32 z2, float32 u2, float32 v2, GRErgb24 planecolor, GRE_Material mymater, YMGRE_ColorMipView colorMip, GRE_Camera4d mycam)
{
	if (y2 < 0 || y0 > mycam->img.height - 1)// 高度在图像范围之外
		return;

	uint16 height = mycam->img.height;
	uint16 width = mycam->img.width;
	float32 znear_v = mycam->frustum.Znear;
	//通过绘制水平直线来完成
	float32 invHeight = 1.0f / (y1 - y2); // y0 == y1
	float32 dxdl = (x1 - x2) * invHeight;//dx
	float32 dxdr = (x0 - x2) * invHeight;
	float32 dzdl = (z1 - z2) * invHeight;//d(1/z)
	float32 dzdr = (z0 - z2) * invHeight;

	float32 startL = x1;
	float32 startR = x0;
	int begX = 0;
	int endX = 0;
	float32 zl = 0;
	float32 zr = 0;
	// 没有材质则使用平面着色
	if ((mymater == NULL) || (mymater->pixel == NULL) || !mymater->valid)
	{
		//扫描线采用左闭右闭、下边界不包含规则，避免填充越过三角形边界
		int begY = GREMax(YMGRE_Raster_Ceil(y0), 0);
		int endY = GREMin(YMGRE_Raster_Ceil(y2), height);
		startL = x1 + (begY - y1) * dxdl;
		startR = x0 + (begY - y0) * dxdr;
		zl = z1 + (begY - y1) * dzdl;
		zr = z0 + (begY - y0) * dzdr;

		for (int y = begY; y < endY; y++)
		{
			begX = YMGRE_Raster_Ceil(startL);
			endX = YMGRE_Raster_Floor(startR);

			//水平方向限幅
			if (begX < 0) begX = 0;
			if (endX > width - 1)
				endX = width - 1;
			if (begX > endX)
			{
				startL += dxdl;
				startR += dxdr;
				zl += dzdl;
				zr += dzdr;
				continue;
			}
			//-----------
			float32* zbuff_i = &mycam->img.zbuff[y * width];//深度缓冲器，该行起点
			GRE_FrameBuffer frame_i = &mycam->img.data[y * width];//帧缓冲区中，该行起点

			float32 zd = (startL == startR) ? 0 : (zr - zl) / (startR - startL);
			float32 invDepth = zl + (begX - startL) * zd;
			//填充固定颜色
			for (int x = begX; x <= endX; x++)
			{
				// One reciprocal per varying-depth fragment, shared by depth and UVs.
				float32 depth = invDepth > 0.0f ? 1.0f / invDepth : 0.0f;
				if (zbuff_i[x] > depth)
				{
					//且在近景平面内
					if (depth > znear_v)
					{
						zbuff_i[x] = depth;
						frame_i[x] = GRE_FramePixel_From_RGB24(planecolor);
					}
				}
				invDepth += zd;
			}
			startL += dxdl; //dx
			startR += dxdr;
			zl += dzdl;
			zr += dzdr;
		}
	}
	else
	{
		float32 dudl = (u1 - u2) * invHeight;// du
		float32 dudr = (u0 - u2) * invHeight;
		float32 dvdl = (v1 - v2) * invHeight;// dv
		float32 dvdr = (v0 - v2) * invHeight;

		float32 startLU = u1;// U  L -- R
		float32 startRU = u0;
		float32 startLV = v1;// V  L -- R
		float32 startRV = v0;

		float32 begU = 0;
		float32 endU = 0;
		float32 begV = 0;
		float32 endV = 0;

		float32 dx = 0;
		float32 ui = 0;
		float32 vi = 0;

		float32 invDepth = 0;
		int begY = GREMax(YMGRE_Raster_Ceil(y0), 0);
		int endY = GREMin(YMGRE_Raster_Ceil(y2), height);
		startL = x1 + (begY - y1) * dxdl;
		startR = x0 + (begY - y0) * dxdr;
		startLU = u1 + (begY - y1) * dudl;
		startRU = u0 + (begY - y0) * dudr;
		startLV = v1 + (begY - y1) * dvdl;
		startRV = v0 + (begY - y0) * dvdr;
		zl = z1 + (begY - y1) * dzdl;
		zr = z0 + (begY - y0) * dzdr;

		for (int y = begY; y < endY; y++)
		{
			//初始化 L -- R
			begX = YMGRE_Raster_Ceil(startL);
			endX = YMGRE_Raster_Floor(startR);
			//u,v
			begU = startLU; endU = startRU;
			begV = startLV; endV = startRV;
			//计算水平方向插值增量
			dx = startR - startL;
			float32 invWidth = (dx == 0) ? 0 : 1.0f / dx;
			ui = (endU - begU) * invWidth;
			vi = (endV - begV) * invWidth;
			float32 zd = (zr - zl) * invWidth;
			begU += (begX - startL) * ui;
			begV += (begX - startL) * vi;
			invDepth = zl + (begX - startL) * zd;
			//修正x的范围
			if (begX < 0)
			{
				begU -= begX * ui;
				begV -= begX * vi;
				invDepth -= begX * zd;
				begX = 0;
			}
			if (endX > width - 1)
				endX = width - 1;
			//在图像范围内
			if (begX <= endX)
			{
				float32* zbuff_i = &mycam->img.zbuff[y * width];//深度缓冲器，该行起点
				GRE_FrameBuffer frame_i = &mycam->img.data[y * width];//帧缓冲区中，该行起点
				for (int x = begX; x <= endX; x++)
				{
					// One reciprocal per varying-depth fragment, shared by depth and UVs.
					float32 depth = invDepth > 0.0f ? 1.0f / invDepth : 0.0f;
					if (zbuff_i[x] > depth)
					{
						//且在近景平面内
						if (depth > znear_v)
						{
							zbuff_i[x] = depth;
							GRErgb24 texel = EMaterial_GetPixel(colorMip.pixels,colorMip.width,colorMip.height,begU * depth,begV * depth);
							//添加光照影响，最后一步才量化到帧缓冲格式
							int cr = texel.R * planecolor.R / DefaultPolygonClv;
							int cg = texel.G * planecolor.G / DefaultPolygonClv;
							int cb = texel.B * planecolor.B / DefaultPolygonClv;
							texel.R = GREMin(cr, 255);
							texel.G = GREMin(cg, 255);
							texel.B = GREMin(cb, 255);
							frame_i[x] = GRE_FramePixel_From_RGB24(texel);
						}
					}
					begU += ui; begV += vi;
					invDepth += zd;
				}
			}
			startL += dxdl;
			startR += dxdr;

			startLU += dudl;
			startLV += dvdl;
			startRU += dudr;
			startRV += dvdr;
			zl += dzdl;
			zr += dzdr;
		}
	}
}

typedef struct triangle_split_
{
	GRE_Vertex4d top;
	GRE_Vertex4d left;
	GRE_Vertex4d right;
	GRE_Vertex4d bottom;
	gre_vertex4d middle;
	uint8 type;
}gre_triangle_split;

//排序三角形顶点并计算上下三角形共用的分割点
static uint8 YMGRE_TriangleRaster_Split(GRE_Vertex4d vertexList,
	GRE_Polygon4d polygon, gre_triangle_split* split)
{
	GRE_Vertex4d v0 = &vertexList[polygon->index[0]];
	GRE_Vertex4d v1 = &vertexList[polygon->index[1]];
	GRE_Vertex4d v2 = &vertexList[polygon->index[2]];
	GRE_Vertex4d temp;
	float32 area = (v1->pos.x - v0->pos.x) * (v2->pos.y - v0->pos.y) -
		(v1->pos.y - v0->pos.y) * (v2->pos.x - v0->pos.x);
	if ((area <= YMGRE_RASTER_AREA_EPSILON) && (area >= -YMGRE_RASTER_AREA_EPSILON))
		return 0;

#define GRE_SWAP_POINT(a,b) {temp = a; a = b; b = temp;}
	if (v1->pos.y < v0->pos.y)
		GRE_SWAP_POINT(v0, v1);
	if (v2->pos.y < v0->pos.y)
		GRE_SWAP_POINT(v0, v2);
	if (v2->pos.y < v1->pos.y)
		GRE_SWAP_POINT(v1, v2);
	if (v0->pos.y == v2->pos.y)
		return 0;

	if (v1->pos.y == v2->pos.y)
	{
		split->top = v0;
		split->left = (v1->pos.x <= v2->pos.x) ? v1 : v2;
		split->right = (v1->pos.x <= v2->pos.x) ? v2 : v1;
		split->type = 1;
	}
	else if (v0->pos.y == v1->pos.y)
	{
		split->left = (v0->pos.x <= v1->pos.x) ? v0 : v1;
		split->right = (v0->pos.x <= v1->pos.x) ? v1 : v0;
		split->bottom = v2;
		split->type = 2;
	}
	else
	{
		float32 factor = (v1->pos.y - v0->pos.y) / (v2->pos.y - v0->pos.y);
		split->middle.pos.x = v0->pos.x + factor * (v2->pos.x - v0->pos.x);
		split->middle.pos.y = v1->pos.y;
		split->middle.pos.z = v0->pos.z + factor * (v2->pos.z - v0->pos.z);
		split->middle.u = v0->u + factor * (v2->u - v0->u);
		split->middle.v = v0->v + factor * (v2->v - v0->v);
		split->top = v0;
		split->left = (v1->pos.x <= split->middle.pos.x) ? v1 : &split->middle;
		split->right = (v1->pos.x <= split->middle.pos.x) ? &split->middle : v1;
		split->bottom = v2;
		split->type = 3;
	}
#undef GRE_SWAP_POINT
	return 1;
}

//绘制纯填充三角形
void YMGRE_TriangleRaster_Fill(GRE_Vertex4d vertexList, GRE_Polygon4d polygon,
	GRErgb24 planecolor, GRE_Material material, GRE_Camera4d camera)
{
	GRE_Vertex4d a=&vertexList[polygon->index[0]],b=&vertexList[polygon->index[1]],c=&vertexList[polygon->index[2]];
	YMGRE_ColorMipView colorMip=YMGRE_Material_ColorMipForUV(material,
		a->pos.x,a->pos.y,a->u,a->v,b->pos.x,b->pos.y,b->u,b->v,c->pos.x,c->pos.y,c->u,c->v);
	/* Convert before splitting: every screen-space interpolation, including the
	 * split vertex, must use 1/z. Keep caller vertices and the z buffer in camera z. */
	gre_vertex4d projected[3];
	GRE_Index indices[3] = { 0, 1, 2 };
	gre_polygon4d localPolygon = *polygon;
	localPolygon.index = indices;
	for (int i = 0; i < 3; ++i)
	{
		projected[i] = vertexList[polygon->index[i]];
		float32 z = projected[i].pos.z;
		if (!(z > 0.0f) || !isfinite(z)) return;
		projected[i].pos.z = 1.0f / z;
		projected[i].u *= projected[i].pos.z;
		projected[i].v *= projected[i].pos.z;
	}
	gre_triangle_split split;
	if (!YMGRE_TriangleRaster_Split(projected, &localPolygon, &split))
		return;
	if (split.type == 1)
		Fill_Top_Trangle(split.top->pos.x, split.top->pos.y, split.top->pos.z, split.top->u, split.top->v,
			split.left->pos.x, split.left->pos.y, split.left->pos.z, split.left->u, split.left->v,
			split.right->pos.x, split.right->pos.y, split.right->pos.z, split.right->u, split.right->v,
			planecolor, material, colorMip, camera);
	else if (split.type == 2)
		Fill_Botton_Trangle(split.right->pos.x, split.right->pos.y, split.right->pos.z, split.right->u, split.right->v,
			split.left->pos.x, split.left->pos.y, split.left->pos.z, split.left->u, split.left->v,
			split.bottom->pos.x, split.bottom->pos.y, split.bottom->pos.z, split.bottom->u, split.bottom->v,
			planecolor, material, colorMip, camera);
	else
	{
		Fill_Top_Trangle(split.top->pos.x, split.top->pos.y, split.top->pos.z, split.top->u, split.top->v,
			split.left->pos.x, split.left->pos.y, split.left->pos.z, split.left->u, split.left->v,
			split.right->pos.x, split.right->pos.y, split.right->pos.z, split.right->u, split.right->v,
			planecolor, material, colorMip, camera);
		Fill_Botton_Trangle(split.right->pos.x, split.right->pos.y, split.right->pos.z, split.right->u, split.right->v,
			split.left->pos.x, split.left->pos.y, split.left->pos.z, split.left->u, split.left->v,
			split.bottom->pos.x, split.bottom->pos.y, split.bottom->pos.z, split.bottom->u, split.bottom->v,
			planecolor, material, colorMip, camera);
	}
}
