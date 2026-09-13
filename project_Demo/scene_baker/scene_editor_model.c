#include "scene_editor_model.h"
#include "scene_editor_place.h"
#include "scene_uv.h"

#include "YMGRE_Camera.h"
#include "YMGRE_MathBase.h"
#include "YMGRE_Free.h"
#include <stdio.h>
#include <math.h>

static void refreshMeshNormals(GRE_Object4d mesh)
{
	for (GRE_Object4d part = mesh; part != NULL; part = part->nextObject) {
		for (int i = 0; i < part->polygonNum; ++i) {
			GRE_Polygon4d polygon = &part->polygonList[i];
			if (polygon->num < 3) continue;
			gre_fvector4d a = part->pointList[polygon->index[0]].pos;
			gre_fvector4d b = part->pointList[polygon->index[1]].pos;
			gre_fvector4d c = part->pointList[polygon->index[2]].pos;
			gre_fvector4d u = { b.x - a.x, b.y - a.y, b.z - a.z, 0 };
			gre_fvector4d v = { c.x - a.x, c.y - a.y, c.z - a.z, 0 };
			YMGRE_Fvector4d_CrossToResult(&u, &v, &polygon->pN);
			YMGRE_Fvector4d_Normalize(&polygon->pN);
		}
	}
}

/* Keep the advanced vertex cache in lockstep with the editable mesh. */
static void syncAdvancedMesh(GRE_Object4d mesh)
{
	for (GRE_Object4d part = mesh; part != NULL; part = part->nextObject) {
		YMGRE_Object_GenerateVertexAttributes(part);
	}
}

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
	refreshMeshNormals(object->mesh);
	syncAdvancedMesh(object->mesh);
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
	refreshMeshNormals(object->mesh);
	syncAdvancedMesh(object->mesh);
}

static void rotateLocal(gre_fvector4d* p, int axis, float32 c, float32 s)
{
	float32 x=p->x,y=p->y,z=p->z;
	if(axis==0){p->y=y*c-z*s;p->z=y*s+z*c;}
	else if(axis==1){p->x=x*c+z*s;p->z=-x*s+z*c;}
	else {p->x=x*c-y*s;p->y=x*s+y*c;}
}

void SceneEditorObject_SetRotationY(SceneEditorObject* object, float32 rotationY)
{
	SceneEditorObject_SetRotationAxis(object,1,rotationY);
}

void SceneEditorObject_SetRotationAxis(SceneEditorObject* object, uint8 axis, float32 angle)
{
	if(!object || !object->mesh || axis>2 || !isfinite(angle))return;
	/* Use a fixed X/Y/Z composition so serialized Euler values reconstruct
	   exactly the same geometry regardless of the order of inspector edits. */
	float32 previous[3]={object->rotX,object->rotY,object->rotZ};
	float32 next[3]={object->rotX,object->rotY,object->rotZ};next[axis]=angle;
	float32 oldC[3],oldS[3],newC[3],newS[3];
	for(int i=0;i<3;i++) {
		oldC[i]=YMGRE_Cos(previous[i]*YMGRE_Deg2Rad);oldS[i]=-YMGRE_Sin(previous[i]*YMGRE_Deg2Rad);
		newC[i]=YMGRE_Cos(next[i]*YMGRE_Deg2Rad);newS[i]=YMGRE_Sin(next[i]*YMGRE_Deg2Rad);
	}
	for(GRE_Object4d part=object->mesh;part;part=part->nextObject)for(int i=0;i<part->pointNum;i++) {
		gre_fvector4d p=part->pointList[i].pos;
		p.x-=object->x;p.y-=object->y;p.z-=object->z;
		for(int k=2;k>=0;k--)rotateLocal(&p,k,oldC[k],oldS[k]);
		for(int k=0;k<3;k++)rotateLocal(&p,k,newC[k],newS[k]);
		p.x+=object->x;p.y+=object->y;p.z+=object->z;part->pointList[i].pos=p;
		if(part->importedNormals) {
			for(int k=2;k>=0;k--)rotateLocal(&part->importedNormals[i],k,oldC[k],oldS[k]);
			for(int k=0;k<3;k++)rotateLocal(&part->importedNormals[i],k,newC[k],newS[k]);
		}
	}
	object->rotX=next[0];object->rotY=next[1];object->rotZ=next[2];
	refreshMeshNormals(object->mesh);syncAdvancedMesh(object->mesh);
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

int SceneEditorObject_GenerateUv(SceneEditorObject* object, uint16 mode, int* islands, char* error, size_t capacity)
{
    if(!object||!object->mesh)return 0;
    gre_fvector4d axes[3]={{1,0,0,0},{0,1,0,0},{0,0,1,0}},center={object->x,object->y,object->z,1};
    float angles[3]={object->rotX,object->rotY,object->rotZ};
    for(int i=0;i<3;i++)for(int k=0;k<3;k++)rotateLocal(axes+i,k,
        YMGRE_Cos(angles[k]*YMGRE_Deg2Rad),YMGRE_Sin(angles[k]*YMGRE_Deg2Rad));
    /* Upgrade old capsule rings only on an explicit parameter unwrap. A temporary
       mesh keeps the live scene and any bake intact if construction/UV generation fails. */
    if(mode==5&&object->primitiveKind==SCENE_PLACE_CAPSULE&&object->autoUv!=5) {
        if(object->mesh->importedUvCount>1){if(error&&capacity)snprintf(error,capacity,"旧胶囊含额外 UV 通道，请保留原展开");return 0;}
        SceneEditorPlaceResult result={.kind=SCENE_PLACE_CAPSULE,.detailA=object->detailA,.detailB=object->detailB,.color=object->color};
        GRE_Object4d replacement=SceneEditorPlace_CreateMesh(&result);if(!replacement)return 0;
        SceneEditorPlace_TransformMesh(replacement,object->x,object->y,object->z,0,object->scale,object->wireframe);
        SceneEditorObject staged=*object;staged.mesh=replacement;staged.rotX=staged.rotY=staged.rotZ=0;staged.autoUv=5;
        for(int k=0;k<3;k++)SceneEditorObject_SetRotationAxis(&staged,k,angles[k]);
        int ok=SceneUv_Primitive(replacement,7,&center,axes,object->scale,islands,error,capacity);
        if(ok) {
            GRE_Object4d src=object->mesh;
#define CAPSULE_SWAP(field,type) do{type old=src->field;src->field=replacement->field;replacement->field=old;}while(0)
            CAPSULE_SWAP(pointNum,int);CAPSULE_SWAP(pointList,GRE_Vertex4d);CAPSULE_SWAP(pointList_,GRE_Vertex4d);
            CAPSULE_SWAP(pointList_wN,GRE_Vertex4d_wN);CAPSULE_SWAP(pointList_wN_,GRE_Vertex4d_wN);
            CAPSULE_SWAP(polygonNum,int);CAPSULE_SWAP(polygonList,GRE_Polygon4d);
            CAPSULE_SWAP(importedUvs,float32*);CAPSULE_SWAP(importedUvCount,uint16);CAPSULE_SWAP(importedNormals,gre_fvector4d*);
#undef CAPSULE_SWAP
            YMGRE_Free_Lightmap(src->lightmap);src->lightmap=NULL;object->autoUv=5;object->uvEdited=0;
        }
        YMGRE_Free_Object(replacement);return ok;
    }
    int ok=mode==5?SceneUv_Primitive(object->mesh,object->primitiveKind,&center,axes,object->scale,islands,error,capacity):mode==4?SceneUv_GenerateAtlas(object->mesh,&center,axes,islands,error,capacity):mode==3?
        SceneUv_GenerateAtlasLegacy(object->mesh,&center,axes,islands,error,capacity):mode==2&&object->primitiveKind==SCENE_PLACE_SPHERE?
        SceneUv_Sphere(object->mesh,object->detailA,object->detailB,error,capacity):
        mode==1?SceneUv_Generate(object->mesh,islands,error,capacity):0;
    if(ok){object->autoUv=mode;object->uvEdited=0;if(mode==2&&islands)*islands=1;}
    return ok;
}
