#include "./YMGRE_TriangleRaster.h"
#include "./YMGRE_MathBase.h"
#include "./YMGRE_Light.h"

extern GRErgb24 GRE_brush;

#define YMGRE_RASTER_AREA_EPSILON 1e-5f

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
	if (vertices == NULL || polygon == NULL || lights == NULL || worldToCamera == NULL) return;
	uint8 specularPower = EMaterial_GetSpecularPower(material);
	for (uint16 i = 0; i < vertexCount; i++)
	{
		GRErgb24 lighting = { 0, 0, 0 };
		GRErgb24 specular = { 0, 0, 0 };
		gre_polygon4d litPolygon = *polygon;
		uint32 lightIndex = 0;
		for (GRE_ListNode node = lights->listhead; node != NULL; node = node->next)
		{
			gre_light4d light = *(GRE_Light4d)node->data;
			litPolygon.planeColor = (material && light.type == GRE_GlobalLight) ?
				material->ambient : (material ? material->diffuse : (GRErgb24){ 255, 255, 255 });
			light.proper.pos_ = lightPos[lightIndex++];
			if (light.type == GRE_SpotLight)
			{
				YMGRE_Fvector4d_MatMultTo(worldToCamera, &light.proper.spot.direct,
					&light.proper.spot.direct);
				light.proper.spot.direct.w = 0;
			}
			YMGRE_PolygonLighting_ComponentsAdvanced(&litPolygon,
				&vertices[i].base.pos, &vertices[i].normal, &light,
				&lighting, &specular, mirrorKs, specularPower,
				material ? material->specular : (GRErgb24){ 255, 255, 255 });
		}
		vertices[i].vertexLighting = lighting;
		vertices[i].vertexSpecular = specular;
	}
}

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
	int minX = GREMax(YMGRE_Raster_Floor(GREMin(a->base.pos.x,
		GREMin(b->base.pos.x,c->base.pos.x))), 0);
	int maxX = GREMin(YMGRE_Raster_Ceil(GREMax(a->base.pos.x,
		GREMax(b->base.pos.x,c->base.pos.x))), camera->img.width-1);
	int minY = GREMax(YMGRE_Raster_Floor(GREMin(a->base.pos.y,
		GREMin(b->base.pos.y,c->base.pos.y))), 0);
	int maxY = GREMin(YMGRE_Raster_Ceil(GREMax(a->base.pos.y,
		GREMax(b->base.pos.y,c->base.pos.y))), camera->img.height-1);
	for (int y = minY; y <= maxY; y++) for (int x = minX; x <= maxX; x++)
	{
		float32 w0=((b->base.pos.y-c->base.pos.y)*(x-c->base.pos.x)+
			(c->base.pos.x-b->base.pos.x)*(y-c->base.pos.y))/den;
		float32 w1=((c->base.pos.y-a->base.pos.y)*(x-c->base.pos.x)+
			(a->base.pos.x-c->base.pos.x)*(y-c->base.pos.y))/den;
		float32 w2=1.0f-w0-w1;
		if (w0 < 0 || w1 < 0 || w2 < 0) continue;
		float32 iw0=w0/a->base.pos.z, iw1=w1/b->base.pos.z, iw2=w2/c->base.pos.z;
		float32 invz=iw0+iw1+iw2;
		if (invz <= 0) continue;
		float32 wa=iw0/invz, wb=iw1/invz, wc=iw2/invz, z=1.0f/invz;
		int index=y*camera->img.width+x;
		if (camera->img.zbuff[index] <= z || z <= camera->frustum.Znear) continue;
		camera->img.zbuff[index]=z;
		float32 u=wa*a->base.u+wb*b->base.u+wc*c->base.u;
		float32 v=wa*a->base.v+wb*b->base.v+wc*c->base.v;
		GRErgb24 texel=EMaterial_GetPixel(material ? material->pixel : NULL,
			material ? material->width : 0, material ? material->height : 0, u, v);
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
		camera->img.data[index]=GRE_FramePixel_From_RGB24((GRErgb24){
			(uint8)GREMin(GREMax(cr,0),255), (uint8)GREMin(GREMax(cg,0),255),
			(uint8)GREMin(GREMax(cb,0),255) });
	}
}

static void FillAdvanced(GRE_Vertex4d_wN vertexList, GRE_Polygon4d polygon,
	GRE_Material material, GRE_List lights, gre_fvector4d* lightPos, GRE_FMat4x4 worldToCamera,
	float32 mirrorKs, GRE_Camera4d camera, GRE_Lightmap lightmap)
{
	if(vertexList==NULL||polygon==NULL||(lights==NULL && lightmap==NULL)||camera==NULL) return;
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
	int minX = GREMax(YMGRE_Raster_Floor(GREMin(a->base.pos.x,
		GREMin(b->base.pos.x,c->base.pos.x))),0);
	int maxX = GREMin(YMGRE_Raster_Ceil(GREMax(a->base.pos.x,
		GREMax(b->base.pos.x,c->base.pos.x))),camera->img.width-1);
	int minY = GREMax(YMGRE_Raster_Floor(GREMin(a->base.pos.y,
		GREMin(b->base.pos.y,c->base.pos.y))),0);
	int maxY = GREMin(YMGRE_Raster_Ceil(GREMax(a->base.pos.y,
		GREMax(b->base.pos.y,c->base.pos.y))),camera->img.height-1);
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
		camera->img.zbuff[index]=z;
		float32 u=wa*a->base.u+wb*b->base.u+wc*c->base.u, v=wa*a->base.v+wb*b->base.v+wc*c->base.v;
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
		GRErgb24 texel=EMaterial_GetPixel(material ? material->pixel : NULL,
			material ? material->width : 0,material ? material->height : 0,u,v);
		// Advanced lighting uses the standard 0..255 color range. The legacy
		// rasterizer's neutral value of 128 must not be reused here: white light
		// would otherwise double the color and create intersecting saturation bands.
		int cr=texel.R*lighting.R/255, cg=texel.G*lighting.G/255, cb=texel.B*lighting.B/255;
		cr=cr*(wa*a->color.R+wb*b->color.R+wc*c->color.R)/255;
		cg=cg*(wa*a->color.G+wb*b->color.G+wc*c->color.G)/255;
		cb=cb*(wa*a->color.B+wb*b->color.B+wc*c->color.B)/255;
		cr += specular.R; cg += specular.G; cb += specular.B;
		camera->img.data[index]=GRE_FramePixel_From_RGB24((GRErgb24){GREMin(cr,255),GREMin(cg,255),GREMin(cb,255)});
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
	float32 x2, float32 y2, float32 z2, float32 u2, float32 v2, GRErgb24 planecolor, GRE_Material mymater, GRE_Camera4d mycam)
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
							GRErgb24 texel = EMaterial_GetPixel(mymater->pixel, mymater->width, mymater->height, begU * depth, begV * depth);//getPixel(begU, begV)
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
	float32 x2, float32 y2, float32 z2, float32 u2, float32 v2, GRErgb24 planecolor, GRE_Material mymater, GRE_Camera4d mycam)
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
							GRErgb24 texel = EMaterial_GetPixel(mymater->pixel, mymater->width, mymater->height, begU * depth, begV * depth);//getPixel(begU, begV)
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
			planecolor, material, camera);
	else if (split.type == 2)
		Fill_Botton_Trangle(split.right->pos.x, split.right->pos.y, split.right->pos.z, split.right->u, split.right->v,
			split.left->pos.x, split.left->pos.y, split.left->pos.z, split.left->u, split.left->v,
			split.bottom->pos.x, split.bottom->pos.y, split.bottom->pos.z, split.bottom->u, split.bottom->v,
			planecolor, material, camera);
	else
	{
		Fill_Top_Trangle(split.top->pos.x, split.top->pos.y, split.top->pos.z, split.top->u, split.top->v,
			split.left->pos.x, split.left->pos.y, split.left->pos.z, split.left->u, split.left->v,
			split.right->pos.x, split.right->pos.y, split.right->pos.z, split.right->u, split.right->v,
			planecolor, material, camera);
		Fill_Botton_Trangle(split.right->pos.x, split.right->pos.y, split.right->pos.z, split.right->u, split.right->v,
			split.left->pos.x, split.left->pos.y, split.left->pos.z, split.left->u, split.left->v,
			split.bottom->pos.x, split.bottom->pos.y, split.bottom->pos.z, split.bottom->u, split.bottom->v,
			planecolor, material, camera);
	}
}
