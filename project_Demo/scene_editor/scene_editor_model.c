#include "scene_editor_model.h"

#include "YMGRE_Camera.h"
#include "YMGRE_MathBase.h"
#include <math.h>

void SceneEditorObject_Translate(SceneEditorObject* object, float32 dx, float32 dy, float32 dz)
{
	if (object == NULL || !isfinite(dx) || !isfinite(dy) || !isfinite(dz)) return;
	float32 nextX = object->x + dx, nextY = object->y + dy, nextZ = object->z + dz;
	if (!isfinite(nextX) || !isfinite(nextY) || !isfinite(nextZ)) return;
	object->x = nextX; object->y = nextY; object->z = nextZ;
	if (object->mesh != NULL) {
		for (GRE_Object4d part = object->mesh; part != NULL; part = part->nextObject) {
			part->WorldCoordinate.x += dx;
			part->WorldCoordinate.y += dy;
			part->WorldCoordinate.z += dz;
			for (int i = 0; i < part->pointNum; ++i) {
				part->pointList[i].pos.x += dx;
				part->pointList[i].pos.y += dy;
				part->pointList[i].pos.z += dz;
			}
		}
	}
	SceneEditorObject_SyncHandle(object);
}

void SceneEditorObject_SetScale(SceneEditorObject* object, float32 scale)
{
	if (object == NULL || object->mesh == NULL || scale <= 0.001f || object->scale <= 0.001f) return;
	float32 ratio = scale / object->scale;
	for (GRE_Object4d part = object->mesh; part != NULL; part = part->nextObject) {
		for (int i = 0; i < part->pointNum; ++i) {
			gre_fvector4d* point = &part->pointList[i].pos;
			point->x = part->WorldCoordinate.x + (point->x - part->WorldCoordinate.x) * ratio;
			point->y = part->WorldCoordinate.y + (point->y - part->WorldCoordinate.y) * ratio;
			point->z = part->WorldCoordinate.z + (point->z - part->WorldCoordinate.z) * ratio;
		}
		part->scale = scale;
	}
	object->scale = scale;
}

void SceneEditorObject_SetRotationY(SceneEditorObject* object, float32 rotationY)
{
	if (object == NULL || object->mesh == NULL || !isfinite(rotationY)) return;
	float32 delta = (rotationY - object->rotY) * YMGRE_Deg2Rad;
	float32 cosine = YMGRE_Cos(delta), sine = YMGRE_Sin(delta);
	for (GRE_Object4d part = object->mesh; part != NULL; part = part->nextObject) {
		for (int i = 0; i < part->pointNum; ++i) {
			gre_fvector4d* point = &part->pointList[i].pos;
			float32 x = point->x - object->x;
			float32 z = point->z - object->z;
			point->x = object->x + x * cosine + z * sine;
			point->z = object->z - x * sine + z * cosine;
		}
	}
	object->rotY = rotationY;
}

void SceneEditorObject_SetRotationAxis(SceneEditorObject* object, uint8 axis, float32 angle)
{
	if(object==NULL||object->mesh==NULL||axis>2||!isfinite(angle))return;
	float32* current=axis==0?&object->rotX:(axis==1?&object->rotY:&object->rotZ);
	float32 delta=(angle-*current)*YMGRE_Deg2Rad,c=YMGRE_Cos(delta),s=YMGRE_Sin(delta);
	for(GRE_Object4d part=object->mesh;part!=NULL;part=part->nextObject)for(int i=0;i<part->pointNum;++i){
		gre_fvector4d* p=&part->pointList[i].pos;float32 x=p->x-object->x,y=p->y-object->y,z=p->z-object->z;
		if(axis==0){p->y=object->y+y*c-z*s;p->z=object->z+y*s+z*c;}
		else if(axis==1){p->x=object->x+x*c+z*s;p->z=object->z-x*s+z*c;}
		else {p->x=object->x+x*c-y*s;p->y=object->y+x*s+y*c;}
	}
	*current=angle;
}

void SceneEditorObject_SetColor(SceneEditorObject* object, GYcolor color)
{
	if (object == NULL) return;
	object->color = color;
	GRErgb24 rgb = { (uint8)(color >> 16), (uint8)(color >> 8), (uint8)color };
	for (GRE_Object4d part = object->mesh; part != NULL; part = part->nextObject) {
		for (int i = 0; i < part->polygonNum; ++i) {
			part->polygonList[i].planeColor = rgb;
			part->polygonList[i].planeColor_ = rgb;
		}
	}
	if (object->light != NULL)
		object->light->proper.lightcolor = rgb;
}

void SceneEditorObject_SyncHandle(SceneEditorObject* object)
{
	if (object == NULL) return;
	if (object->light != NULL) {
		object->light->pos = (gre_fvector4d){ object->x, object->y, object->z, 1 };
		object->light->proper.strength = object->strength;
		object->light->proper.shadowK = object->shadowsEnabled ? object->strength * 0.1f : 0.0f;
	}
	if (object->camera != NULL) {
		gre_fvector4d position = { object->x, object->y, object->z, 1 };
		gre_fvector4d target = { object->targetX, object->targetY, object->targetZ, 1 };
		YMGRE_UVNCamera_PositionInit(object->camera, &position, &target, NULL, 0);
	}
}
