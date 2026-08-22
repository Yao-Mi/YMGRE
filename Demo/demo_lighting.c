#include "YMGRE_YMGUI_LCD.h"
#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"
#include "YMGRE_Coordinates_Transform.h"
#include "YMGRE_Rasterization.h"
#include "YMGRE_MathBase.h"
#include "YMGRE_Light.h"
#include <math.h>

#define EQUAL_PointLightStrength(baseStrength, halfConeAngleDeg) \
	((baseStrength) * GREMin(YMGRE_Spot_EquivalentPointLightGain(halfConeAngleDeg), 6.0f))

static GRE_Camera4d createCamera(int16 id, int16 width, int16 height)
{
	GRE_Camera4d camera = YMGRE_Creat_Camera(id, width, height, 38, 38, 38, 38);
	YMGRE_Camera_Frustum_Init(camera, 1, 500);
	gre_fvector4d position = { 0, 55, -70, 1 };
	gre_fvector4d target = { 0, 0, 120, 1 };
	YMGRE_UVNCamera_PositionInit(camera, &position, &target, NULL, 0);
	return camera;
}

static GRE_Object4d createObject(void)
{
	GRE_Object4d object = YMGRE_MeshGener_Sphere(70, 50, 50,
		(GRErgb24){ 190, 205, 220 }, "lighting_sphere", "");
	object->WorldCoordinate = (gre_fvector4d){ 0, 0, 120, 1 };
	object->mirrorKs = 1.0f;
	YMGRE_Object_LocalToWorld(object);
	return object;
}

static int projectWorld(GRE_Camera4d camera, gre_fvector4d world, int* x, int* y)
{
	gre_fvector4d screen;
	YMGRE_Point_WorldToCamera(&world, &screen, &camera->move.TMat);
	if (screen.z <= camera->frustum.Znear)
		return 0;
	YMGRE_Point_CameraToViewPlane(&screen, camera->perspectPlane.Dis);
	YMGRE_Point_ViewPlaneToWindows(&screen, camera);
	*x = (int)screen.x;
	*y = (int)screen.y;
	return (*x >= 0 && *x < camera->img.width && *y >= 0 && *y < camera->img.height);
}

static void drawLightMarker(GRE_Camera4d camera, GRE_Light4d light, GRErgb24 color)
{
	int x, y;
	if (!projectWorld(camera, light->pos, &x, &y))
		return;
	int size = 9;
	if (x < size || y < size || x >= camera->img.width - size || y >= camera->img.height - size)
		return;
	YMGRE_Img_SetBrushColor(color);
	YMGRE_Img_Line(camera->img.data, camera->img.width, camera->img.height,
		x - size, y, x + size, y);
	YMGRE_Img_Line(camera->img.data, camera->img.width, camera->img.height,
		x, y - size, x, y + size);
}

static void drawSpotCone(GRE_Camera4d camera, GRE_Light4d light, GRErgb24 color,
	float32 halfConeAngleDeg)
{
	const float32 coneLength = 100.0f;
	// Demo 中锥体指向场景球心；渲染管线里的 direct 已转换为相机空间，
	// 这里重新用世界空间位置计算，避免线框方向被相机变换污染。
	gre_fvector4d direction = { -light->pos.x, -light->pos.y, 120.0f - light->pos.z, 0.0f };
	direction.w = 0.0f;
	YMGRE_Fvector4d_Normalize(&direction);
	// 构造与聚光方向正交的圆锥底面基。
	gre_fvector4d reference = { 0, 1, 0, 0 };
	if (YMGRE_Fvector4d_Len1(&direction) > 0.999f &&
		GREMax(direction.y, -direction.y) > 0.99f)
		reference = (gre_fvector4d){ 1, 0, 0, 0 };
	gre_fvector4d right;
	gre_fvector4d up;
	YMGRE_Fvector4d_CrossToResult(&direction, &reference, &right);
	YMGRE_Fvector4d_Normalize(&right);
	YMGRE_Fvector4d_CrossToResult(&right, &direction, &up);
	YMGRE_Fvector4d_Normalize(&up);
	float32 angleRad = halfConeAngleDeg * YMGRE_Deg2Rad;
	float32 sinAngle = YMGRE_Sin(angleRad);
	float32 cosAngle = YMGRE_Cos(angleRad);
	if (cosAngle < 1e-4f) cosAngle = 1e-4f;
	float32 radius = coneLength * sinAngle / cosAngle;
	gre_fvector4d center = light->pos;
	center.x += direction.x * coneLength;
	center.y += direction.y * coneLength;
	center.z += direction.z * coneLength;
	center.w = 1.0f;
	gre_fvector4d ring[8];
	for (uint16 i = 0; i < 8; i++)
	{
		float32 a = (float32)i * (2.0f * YMGRE_Pai / 8.0f);
		float32 c = YMGRE_Cos(a);
		float32 s = YMGRE_Sin(a);
		ring[i] = center;
		ring[i].x += radius * (right.x * c + up.x * s);
		ring[i].y += radius * (right.y * c + up.y * s);
		ring[i].z += radius * (right.z * c + up.z * s);
	}
	YMGRE_Img_SetBrushColor(color);
	for (uint16 i = 0; i < 8; i++)
	{
		int x0, y0, x1, y1, xa, ya;
		gre_fvector4d* next = &ring[(i + 1) % 8];
		if (projectWorld(camera, ring[i], &x0, &y0) &&
			projectWorld(camera, *next, &x1, &y1))
			YMGRE_Img_Line(camera->img.data, camera->img.width, camera->img.height,
				x0, y0, x1, y1);
		if (projectWorld(camera, light->pos, &xa, &ya) &&
			projectWorld(camera, ring[i], &x0, &y0))
			YMGRE_Img_Line(camera->img.data, camera->img.width, camera->img.height,
				xa, ya, x0, y0);
	}
}

static GRErgb24 scaleLightColor(GRErgb24 color, float32 strength)
{
	if (strength > 1.0f) strength = 1.0f;
	if (strength < 0.0f) strength = 0.0f;
	color.R = (uint8)(color.R * strength);
	color.G = (uint8)(color.G * strength);
	color.B = (uint8)(color.B * strength);
	return color;
}

static void renderPanel(GRE_Camera4d camera, GRE_List objects, GRE_List lights,
	GRE_RenderWorkspace workspace, GRE_Light4d marker, GRErgb24 markerColor,
	uint8 showSpotCone, int x, int y)
{
	gre_list materials = { 0 };
	YMGRE_Camera_TanglePipline_RenderingWithWorkspace(camera, lights, objects,
		&materials, workspace);
	if (marker != NULL)
	{
		GRErgb24 displayColor = scaleLightColor(markerColor, marker->proper.strength);
		// 内置灯光绘制函数使用相机空间位置，这里按当前相机转换后再调用。
		GRE_Light4d cameraLight = YMGRE_Creat_Light(marker->ID, marker->type,
			displayColor, marker->proper.strength);
		YMGRE_Point_WorldToCamera(&marker->pos, &cameraLight->proper.pos_, &camera->move.TMat);
		YMGRE_Point_CameraToViewPlane(&cameraLight->proper.pos_, camera->perspectPlane.Dis);
		YMGRE_Point_ViewPlaneToWindows(&cameraLight->proper.pos_, camera);
		YMGRE_Light_Primitive_Rasterization(cameraLight, camera, 5);
		YMGRE_Free_Light(cameraLight);
		drawLightMarker(camera, marker, displayColor);
		if (showSpotCone)
		{
			// 线框圆锥半角与灯光实际内锥角保持一致。
			float32 innerAngleDeg = acosf(marker->proper.spot.cs_inner_angle) /
				YMGRE_Deg2Rad;
			drawSpotCone(camera, marker, displayColor, innerAngleDeg);
		}
	}
	LCD_Fill_RgbRect(x, y, camera->img.width, camera->img.height, camera->img.data);
}

int main(void)
{
	gre_list objects = { 0 };
	GRE_Object4d object = createObject();
	YMGRE_List_Append(&objects, sizeof(gre_object4d), object);
	GRE_RenderWorkspace workspace = YMGRE_Creat_RenderWorkspace();
	GRE_Camera4d globalCamera = createCamera(0, 300, 300);
	GRE_Camera4d pointCamera = createCamera(1, 300, 300);
	GRE_Camera4d spotCamera = createCamera(2, 300, 300);
	GRE_Light4d global = YMGRE_Creat_Light(0, GRE_GlobalLight,
		(GRErgb24){ 255, 255, 255 }, 1.0f);
	GRE_Light4d point = YMGRE_Creat_Light(1, GRE_PointLight,
		(GRErgb24){ 255, 180, 120 }, 1.0f);
	const float32 spotInnerAngleDeg = 60.0f;
	GRE_Light4d spot = YMGRE_Creat_Light(2, GRE_SpotLight,
		(GRErgb24){ 255, 190, 130 },
		EQUAL_PointLightStrength(0.2f, spotInnerAngleDeg));
	point->pos = (gre_fvector4d){ -80, 40, 70, 1 };
	point->proper.kc1 = 0.004f;
	// 与点光源共址，方向指向球体中心，便于直接比较点光与聚光的差异。
	spot->pos = point->pos;
	// 光源仍位于球体左侧，但聚光方向明确指向球心，避免整个模型落在外锥之外。
	spot->proper.spot.direct = (gre_fvector4d){ 80.0f, -40.0f, 50.0f, 0 };
	spot->proper.kc1 = 0.001f;
	spot->proper.spot.cs_inner_angle = YMGRE_Cos(spotInnerAngleDeg * YMGRE_Deg2Rad);
	spot->proper.spot.cs_outer_angle = YMGRE_Cos((spotInnerAngleDeg+15.0f) * YMGRE_Deg2Rad);
	{
		float32 d = spot->proper.spot.cs_inner_angle - spot->proper.spot.cs_outer_angle;
		spot->proper.spot.cs_div_ = 1.0f / d;
	}
	// The pipeline evaluates normals, positions and directions in camera space.
	{
		gre_fvector4d worldDirection = spot->proper.spot.direct;
		YMGRE_Fvector4d_MatMultTo(&spotCamera->move.TMat, &worldDirection,
			&spot->proper.spot.direct);
		spot->proper.spot.direct.w = 0;
	}
	gre_list globalLights = { 0 };
	gre_list pointLights = { 0 };
	gre_list spotLights = { 0 };
	YMGRE_List_Append(&globalLights, sizeof(gre_light4d), global);
	YMGRE_List_Append(&pointLights, sizeof(gre_light4d), point);
	YMGRE_List_Append(&spotLights, sizeof(gre_light4d), spot);

	LCD_Init(900, 300);
	renderPanel(globalCamera, &objects, &globalLights, workspace, NULL,
		(GRErgb24){ 255, 255, 255 }, 0, 0, 0);
	renderPanel(pointCamera, &objects, &pointLights, workspace, point,
		(GRErgb24){ 255, 210, 80 }, 0, 300, 0);
	renderPanel(spotCamera, &objects, &spotLights, workspace, spot,
		(GRErgb24){ 80, 220, 255 }, 1, 600, 0);
	while (LCD_Update(60)) { }
	LCD_Destory();

	YMGRE_Free_RenderWorkspace(workspace);
	YMGRE_Free_Camera(globalCamera);
	YMGRE_Free_Camera(pointCamera);
	YMGRE_Free_Camera(spotCamera);
	YMGRE_List_Clear(&globalLights, YMGRE_Free_Light);
	YMGRE_List_Clear(&pointLights, YMGRE_Free_Light);
	YMGRE_List_Clear(&spotLights, YMGRE_Free_Light);
	YMGRE_List_Clear(&objects, YMGRE_Free_Object);
	return 0;
}
