#include "scene_ray_renderer.h"
#include "YMGRE_MathBase.h"
#include "YMGRE_Light.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Rendering_Pipeline.h"
#include <float.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    GRE_Object4d object;
    GRE_Material material;
    int polygon;
    const SceneRayPrimitive* primitive;
    gre_fvector4d p[3];
    float center[3];
} RayTriangle;
typedef struct {
    float low[3], high[3];
    int start, count, left, right;
} RayNode;
struct SceneRayRenderer {
    RayTriangle* triangles;
    RayNode* nodes;
    int count, triangleCount, nodeCount, row, valid;
    SceneRayPrimitive primitives[32];int primitiveCount;
    uint64_t fingerprint;
    size_t capacity;
    GRE_FramePixel* colors;
    float* depths;
};
static uint64_t hashBytes(uint64_t hash, const void* data, size_t size)
{
    const unsigned char* p=data;
    for(size_t i=0;i<size;i++){hash^=p[i];hash*=UINT64_C(1099511628211);}
    return hash;
}
#define HASH(v) h=hashBytes(h,&(v),sizeof(v))
static uint64_t sceneHash(GRE_Camera4d c, GRE_List objects, GRE_List lights, GRE_List materials)
{
    uint64_t h=UINT64_C(14695981039346656037);
    HASH(c->pos);HASH(c->move.cu);HASH(c->move.cv);HASH(c->move.cn);
    HASH(c->perspectPlane);HASH(c->frustum);HASH(c->img.width);HASH(c->img.height);
    for(GRE_ListNode n=objects->listhead;n;n=n->next) {
        for(GRE_Object4d o=n->data;o;o=o->nextObject) {
            /* Pointer identities invalidate retained references even if replacement geometry matches. */
            HASH(o);HASH(o->isVisible);HASH(o->renderMode);HASH(o->mirrorKs);
            HASH(o->pointNum);HASH(o->polygonNum);
            h=hashBytes(h,o->pointList,(size_t)o->pointNum*sizeof(*o->pointList));
            if(o->materiaName)h=hashBytes(h,o->materiaName,strlen(o->materiaName)+1);
            for(int i=0;i<o->pointNum && o->pointList_wN;i++) {
                HASH(o->pointList_wN[i].normal);HASH(o->pointList_wN[i].tangent);HASH(o->pointList_wN[i].tangentW);
            }
            for(int i=0;i<o->polygonNum;i++) {
                GRE_Polygon4d p=o->polygonList+i;
                HASH(p->num);HASH(p->pN);HASH(p->planeColor);
                h=hashBytes(h,p->index,(size_t)p->num*sizeof(*p->index));
            }
            GRE_Lightmap m=o->lightmap;HASH(m);
            if(m) {
                HASH(m->enabled);HASH(m->colorsBaked);HASH(m->width);HASH(m->height);HASH(m->triangleCount);
                if(m->uv1)h=hashBytes(h,m->uv1,(size_t)m->triangleCount*6*sizeof(float));
                if(m->pixels)h=hashBytes(h,m->pixels,(size_t)m->width*m->height*sizeof(GRErgb24));
                if(m->specularPixels)h=hashBytes(h,m->specularPixels,(size_t)m->width*m->height*sizeof(GRErgb24));
            }
        }
    }
    for(GRE_ListNode n=lights->listhead;n;n=n->next) {
        GRE_Light4d l=n->data;
        HASH(l->ishide);HASH(l->type);HASH(l->pos);HASH(l->proper.strength);HASH(l->proper.shadowK);
        HASH(l->proper.kc0);HASH(l->proper.kc1);HASH(l->proper.kc2);HASH(l->proper.lightcolor);
        if(l->type==GRE_SpotLight){HASH(l->proper.spot);}
    }
    for(GRE_ListNode n=materials->listhead;n;n=n->next) {
        GRE_Material m=n->data;HASH(m);HASH(m->ambient);HASH(m->diffuse);HASH(m->specular);HASH(m->unlit);
        HASH(m->width);HASH(m->height);
        if(m->pixel)h=hashBytes(h,m->pixel,(size_t)m->width*m->height*sizeof(GRErgb24));
        if(m->advanced) {
            HASH(m->advanced->rayType);HASH(m->advanced->reflectivity);HASH(m->advanced->ior);HASH(m->advanced->transmissionColor);
            HASH(m->advanced->specularPower);HASH(m->advanced->normalWidth);HASH(m->advanced->normalHeight);
            if(m->advanced->normalPixel)h=hashBytes(h,m->advanced->normalPixel,
                (size_t)m->advanced->normalWidth*m->advanced->normalHeight*sizeof(GRErgb24));
        }
    }
    return h;
}
#undef HASH
static float component(gre_fvector4d p, int axis){return axis==0?p.x:axis==1?p.y:p.z;}
static int compareX(const void* a,const void* b){float d=((const RayTriangle*)a)->center[0]-((const RayTriangle*)b)->center[0];return (d>0)-(d<0);}
static int compareY(const void* a,const void* b){float d=((const RayTriangle*)a)->center[1]-((const RayTriangle*)b)->center[1];return (d>0)-(d<0);}
static int compareZ(const void* a,const void* b){float d=((const RayTriangle*)a)->center[2]-((const RayTriangle*)b)->center[2];return (d>0)-(d<0);}
static int buildNode(SceneRayRenderer* r,int start,int count)
{
    int index=r->nodeCount++;RayNode* n=r->nodes+index;
    *n=(RayNode){.start=start,.count=count,.left=-1,.right=-1};
    for(int axis=0;axis<3;axis++) {
        n->low[axis]=FLT_MAX;n->high[axis]=-FLT_MAX;
        for(int i=start;i<start+count;i++)for(int j=0;j<3;j++) {
            float v=component(r->triangles[i].p[j],axis);
            n->low[axis]=fminf(n->low[axis],v);n->high[axis]=fmaxf(n->high[axis],v);
        }
    }
    if(count>6) {
        int axis=0;
        for(int a=1;a<3;a++)if(n->high[a]-n->low[a]>n->high[axis]-n->low[axis])axis=a;
        qsort(r->triangles+start,count,sizeof(RayTriangle),axis==0?compareX:axis==1?compareY:compareZ);
        n->left=buildNode(r,start,count/2);n->right=buildNode(r,start+count/2,count-count/2);n->count=0;
    }
    return index;
}
static int baked(GRE_Object4d o)
{return o->lightmap&&o->lightmap->enabled&&o->lightmap->pixels&&o->lightmap->uv1&&o->lightmap->triangleCount==(uint32)o->polygonNum;}
static const SceneRayPrimitive* analytic(SceneRayRenderer* r,GRE_Object4d o)
{
    if(baked(o))return NULL;
    for(int i=0;i<r->primitiveCount;i++)if(r->primitives[i].object==o)return r->primitives+i;
    return NULL;
}
static int rayType(GRE_Material m)
{return m&&!m->unlit&&m->advanced?m->advanced->rayType:0;}
static int rebuild(SceneRayRenderer* r,GRE_List objects,GRE_List materials)
{
    free(r->triangles);free(r->nodes);r->triangles=NULL;r->nodes=NULL;r->count=r->triangleCount=r->nodeCount=0;
    size_t count=r->primitiveCount;
    for(GRE_ListNode n=objects->listhead;n;n=n->next)
        for(GRE_Object4d o=n->data;o;o=o->nextObject)if(o->isVisible)count+=o->polygonNum;
    if(!count)return 1;
    if(count>INT32_MAX/2)return 0;
    r->triangles=calloc(count,sizeof(RayTriangle));r->nodes=calloc(count*2,sizeof(RayNode));
    if(!r->triangles||!r->nodes)return 0;
    for(GRE_ListNode n=objects->listhead;n;n=n->next)for(GRE_Object4d o=n->data;o;o=o->nextObject) {
        if(!o->isVisible || !o->pointList || !o->polygonList || analytic(r,o))continue;
        GRE_Material m=YMGRE_Material_Find(materials,o->materiaName);
        for(int i=0;i<o->polygonNum;i++) {
            GRE_Polygon4d p=o->polygonList+i;
            if(p->num!=3 || !p->index)continue;
            RayTriangle t={.object=o,.material=m,.polygon=i};int valid=1;
            for(int j=0;j<3;j++) {
                if(p->index[j]>=o->pointNum){valid=0;break;}
                t.p[j]=o->pointList[p->index[j]].pos;
                for(int axis=0;axis<3;axis++) {
                    float v=component(t.p[j],axis);if(!isfinite(v))valid=0;
                    t.center[axis]+=v/3;
                }
            }
            if(valid)r->triangles[r->count++]=t;
        }
    }
    r->triangleCount=r->count;
    if(r->triangleCount)buildNode(r,0,r->triangleCount);
    for(int i=0;i<r->primitiveCount;i++) {
        const SceneRayPrimitive* p=r->primitives+i;GRE_Object4d o=p->object;
        if(o->isVisible&&!baked(o))r->triangles[r->count++]=(RayTriangle){.object=o,
            .material=YMGRE_Material_Find(materials,o->materiaName),.primitive=p};
    }
    return 1;
}
static int boxHit(const RayNode* n,const gre_ray* ray,float low,float high)
{
    for(int a=0;a<3;a++) {
        float d=component(ray->direction,a),o=component(ray->origin,a);
        if(fabsf(d)<1e-12f){if(o<n->low[a] || o>n->high[a])return 0;continue;}
        float t0=(n->low[a]-o)/d,t1=(n->high[a]-o)/d;
        low=fmaxf(low,fminf(t0,t1));high=fminf(high,fmaxf(t0,t1));
        if(high<low)return 0;
    }
    return 1;
}
static gre_fvector4d localVector(const SceneRayPrimitive* p,gre_fvector4d v)
{return (gre_fvector4d){YMGRE_Fvector4d_Dot(&v,(GRE_Fvector4d)&p->axis[0]),YMGRE_Fvector4d_Dot(&v,(GRE_Fvector4d)&p->axis[1]),YMGRE_Fvector4d_Dot(&v,(GRE_Fvector4d)&p->axis[2]),0};}
static gre_fvector4d worldVector(const SceneRayPrimitive* p,gre_fvector4d v)
{return (gre_fvector4d){p->axis[0].x*v.x+p->axis[1].x*v.y+p->axis[2].x*v.z,p->axis[0].y*v.x+p->axis[1].y*v.y+p->axis[2].y*v.z,p->axis[0].z*v.x+p->axis[1].z*v.y+p->axis[2].z*v.z,0};}
static int intersectPrimitive(const SceneRayPrimitive* p,gre_ray* ray,float low,float high,gre_ray_hit* hit)
{
    gre_ray local=*ray;local.origin.x-=p->center.x;local.origin.y-=p->center.y;local.origin.z-=p->center.z;local.origin.w=0;
    local.origin=localVector(p,local.origin);local.direction=localVector(p,ray->direction);
    gre_fvector4d zero={0};gre_ray_hit h;
    int found=p->type==1?YMGRE_Ray_IntersectSphere(&local,&zero,p->radius,low,high,&h):
        YMGRE_Ray_IntersectCylinder(&local,&zero,p->radius,p->halfHeight,low,high,&h);
    if(!found)return 0;
    if(p->type==1){h.u=atan2f(h.position.z,h.position.x)/(2*3.14159265359f);h.v=acosf(fmaxf(-1,fminf(1,h.position.y/p->radius)))/3.14159265359f;}
    else if(fabsf(h.normal.y)>.5f){h.u=.5f+h.position.x/(2*p->radius);h.v=.5f+h.position.z/(2*p->radius);}
    else {h.u=atan2f(h.position.z,h.position.x)/(2*3.14159265359f);h.v=.5f-h.position.y/(2*p->halfHeight);}
    if(h.u<0)h.u+=1;
    h.normal=worldVector(p,h.normal);h.position=(gre_fvector4d){ray->origin.x+ray->direction.x*h.distance,ray->origin.y+ray->direction.y*h.distance,ray->origin.z+ray->direction.z*h.distance,1};*hit=h;return 1;
}
static int intersect(SceneRayRenderer* r,gre_ray* ray,float low,float high,int shadow,gre_ray_hit* hit)
{
    if(!r->count)return -1;
    int stack[64],top=0,found=-1;if(r->triangleCount)stack[top++]=0;
    while(top) {
        RayNode* n=r->nodes+stack[--top];
        if(!boxHit(n,ray,low,high))continue;
        if(!n->count){stack[top++]=n->left;stack[top++]=n->right;continue;}
        for(int i=n->start;i<n->start+n->count;i++) {
            RayTriangle* t=r->triangles+i;gre_ray_hit candidate;
            /* Primary rays follow the same outward-face convention as rasterization.
               Shadow rays are double-sided so thin meshes also block light. */
            if(!shadow && rayType(t->material)!=2 && YMGRE_Fvector4d_Dot(&t->object->polygonList[t->polygon].pN,&ray->direction)>=0)continue;
            if(YMGRE_Ray_IntersectTriangle(ray,t->p,t->p+1,t->p+2,low,high,&candidate)) {
                if(shadow)return i;
                candidate.normal=t->object->polygonList[t->polygon].pN;candidate.normal.w=0;
                YMGRE_Fvector4d_Normalize(&candidate.normal);candidate.position.w=1;
                *hit=candidate;high=candidate.distance;found=i;
            }
        }
    }
    for(int i=r->triangleCount;i<r->count;i++) {
        gre_ray_hit candidate;
        if(intersectPrimitive(r->triangles[i].primitive,ray,low,high,&candidate)) {
            if(shadow)return i;
            *hit=candidate;high=candidate.distance;found=i;
        }
    }
    return found;
}
static GRErgb24 sample(GRErgb24* pixels,int w,int h,float u,float v,int clamp)
{
    if(!pixels||w<=0||h<=0||!isfinite(u)||!isfinite(v))return (GRErgb24){255,255,255};
    if(!clamp){u=fabsf(u-truncf(u));v=fabsf(v-truncf(v));}
    int x=GREMin(w-1,(int)(fminf(fmaxf(u,0),1)*w+0.00001f));
    int y=GREMin(h-1,(int)(fminf(fmaxf(v,0),1)*h+0.00001f));
    return pixels[y*w+x];
}
static GRErgb24 multiply(GRErgb24 a,GRErgb24 b)
{return (GRErgb24){a.R*b.R/255,a.G*b.G/255,a.B*b.B/255};}
static GRErgb24 shade(SceneRayRenderer* r,RayTriangle* t,gre_ray_hit* hit,GRE_Camera4d camera,GRE_List lights,gre_fvector4d* outNormal,GRErgb24* outSpecular)
{
    *outSpecular=(GRErgb24){0};
    GRE_Object4d o=t->object;GRE_Polygon4d p=o->polygonList+t->polygon;GRE_Material m=t->material;
    float w[3]={1-hit->u-hit->v,hit->u,hit->v},u=0,v=0;
    gre_fvector4d normal=p->pN,tangent={0},bitangent;
    if(o->renderMode!=GRE_RenderMode_Face && o->pointList_wN)normal=(gre_fvector4d){0};
    GRE_Vertex4d a=o->pointList+p->index[0],b=o->pointList+p->index[1],c=o->pointList+p->index[2];
    float handedness=((b->u-a->u)*(c->v-a->v)-(c->u-a->u)*(b->v-a->v)<-1e-8f)?-1:1;
    for(int j=0;j<3;j++) {
        GRE_Vertex4d vertex=o->pointList+p->index[j];u+=w[j]*vertex->u;v+=w[j]*vertex->v;
        if(o->pointList_wN) {
            GRE_Vertex4d_wN a=o->pointList_wN+p->index[j];
            if(o->renderMode!=GRE_RenderMode_Face) {
                normal.x+=w[j]*a->normal.x;normal.y+=w[j]*a->normal.y;normal.z+=w[j]*a->normal.z;
            }
            tangent.x+=w[j]*a->tangent.x;tangent.y+=w[j]*a->tangent.y;tangent.z+=w[j]*a->tangent.z;
        }
    }
    if(t->primitive) {
        u=hit->u;v=hit->v;normal=hit->normal;
        gre_fvector4d ln=localVector(t->primitive,normal);
        gre_fvector4d lt=fabsf(ln.y)>.999f?(gre_fvector4d){1,0,0,0}:(gre_fvector4d){-ln.z,0,ln.x,0};
        tangent=worldVector(t->primitive,lt);handedness=1;
    }
    normal.w=0;YMGRE_Fvector4d_Normalize(&normal);*outNormal=normal;
    GRErgb24 tex=sample(m?m->pixel:NULL,m?m->width:0,m?m->height:0,u,v,0);
    GRErgb24 diffuse=m?m->diffuse:p->planeColor,base={0},spec={0};
    if(m&&m->unlit)return multiply(tex,diffuse);
    GRE_Lightmap map=o->lightmap;
    if(map&&map->enabled&&map->pixels&&map->uv1&&map->triangleCount==(uint32)o->polygonNum) {
        float lu=0,lv=0;for(int j=0;j<3;j++){lu+=w[j]*map->uv1[t->polygon*6+j*2];lv+=w[j]*map->uv1[t->polygon*6+j*2+1];}
        base=sample(map->pixels,map->width,map->height,lu,lv,1);
        if(!map->colorsBaked)base=multiply(base,diffuse);
        if(map->specularPixels)spec=sample(map->specularPixels,map->width,map->height,lu,lv,1);
    } else {
        if(m&&m->advanced&&m->advanced->normalPixel&&o->pointList_wN) {
            GRErgb24 nm=sample(m->advanced->normalPixel,m->advanced->normalWidth,m->advanced->normalHeight,u,v,0);
            float dot=YMGRE_Fvector4d_Dot(&tangent,&normal);
            tangent.x-=dot*normal.x;tangent.y-=dot*normal.y;tangent.z-=dot*normal.z;
            if(YMGRE_Fvector4d_Len2(&tangent)>1e-12f) {
                YMGRE_Fvector4d_Normalize(&tangent);YMGRE_Fvector4d_CrossToResult(&normal,&tangent,&bitangent);
                float x=nm.R/127.5f-1,y=(nm.G/127.5f-1)*(handedness<0?-1:1),z=nm.B/127.5f-1;
                normal=(gre_fvector4d){tangent.x*x+bitangent.x*y+normal.x*z,tangent.y*x+bitangent.y*y+normal.y*z,tangent.z*x+bitangent.z*y+normal.z*z,0};
                YMGRE_Fvector4d_Normalize(&normal);*outNormal=normal;
            }
        }
        gre_fvector4d relative={hit->position.x-camera->pos.x,hit->position.y-camera->pos.y,hit->position.z-camera->pos.z,1};
        for(GRE_ListNode n=lights->listhead;n;n=n->next) {
            gre_light4d l=*(GRE_Light4d)n->data;if(l.ishide)continue;
            if(l.type!=GRE_GlobalLight && l.proper.shadowK>0) {
                gre_fvector4d d={l.pos.x-hit->position.x,l.pos.y-hit->position.y,l.pos.z-hit->position.z,0};
                float distance=YMGRE_Fvector4d_Len1(&d);
                if(distance>1e-5f && YMGRE_Fvector4d_Dot(&normal,&d)>0) {
                    YMGRE_Fvector4d_Normalize(&d);gre_ray shadow;
                    float bias=fmaxf(0.0001f,0.000001f*hit->distance);
                    gre_fvector4d geometricNormal=hit->normal;geometricNormal.w=0;
                    YMGRE_Fvector4d_Normalize(&geometricNormal);
                    YMGRE_Ray_SpawnFromSurface(&hit->position,&geometricNormal,&d,bias,&shadow);
                    if(intersect(r,&shadow,bias,distance-bias,1,NULL)>=0)continue;
                }
            }
            l.proper.pos_=(gre_fvector4d){l.pos.x-camera->pos.x,l.pos.y-camera->pos.y,l.pos.z-camera->pos.z,1};
            l.proper.shadowK=0;gre_polygon4d surface=*p;
            surface.planeColor=(m&&l.type==GRE_GlobalLight)?m->ambient:diffuse;
            uint8 power=m&&m->advanced&&m->advanced->specularPower?m->advanced->specularPower:30;
            YMGRE_PolygonLighting_ComponentsAdvanced(&surface,&relative,&normal,&l,&base,&spec,o->mirrorKs,power,m?m->specular:(GRErgb24){255,255,255});
        }
    }
    *outSpecular=spec;
    base=multiply(tex,base);
    return (GRErgb24){GREMin(255,base.R+spec.R),GREMin(255,base.G+spec.G),GREMin(255,base.B+spec.B)};
}
typedef struct {GRE_Object4d object[8];float ior[8];int count;} RayMedia;
static GRErgb24 trace(SceneRayRenderer* r,gre_ray* ray,float low,float high,GRE_List lights,int depth,float weight,RayMedia media);
static GRErgb24 mix(GRErgb24 a,GRErgb24 b,float t)
{return (GRErgb24){a.R*(1-t)+b.R*t,a.G*(1-t)+b.G*t,a.B*(1-t)+b.B*t};}
static GRErgb24 surfaceColor(SceneRayRenderer* r,RayTriangle* t,gre_ray_hit* hit,gre_ray* ray,GRE_List lights,int depth,float weight,RayMedia media)
{
    gre_camera4d view={.pos=ray->origin};gre_fvector4d normal;
    GRErgb24 specular;GRErgb24 local=shade(r,t,hit,&view,lights,&normal,&specular);int type=rayType(t->material);
    if(!type||baked(t->object))return local;
    if(depth>=8||weight<.005f)return (GRErgb24){50,50,50};
    int entering=YMGRE_Fvector4d_Dot(&ray->direction,&hit->normal)<0;
    if(YMGRE_Fvector4d_Dot(&ray->direction,&normal)>0){normal.x=-normal.x;normal.y=-normal.y;normal.z=-normal.z;}
    float bias=fmaxf(.0001f,hit->distance*1e-6f);
    gre_fvector4d direction;gre_ray bounce;
    YMGRE_Ray_Reflect(&ray->direction,&normal,&direction);
    YMGRE_Ray_SpawnFromSurface(&hit->position,&hit->normal,&direction,bias,&bounce);
    GRE_MaterialAdvanced material=t->material->advanced;
    if(type==1) {
        float amount=fmaxf(0,fminf(1,material->reflectivity));
        GRErgb24 result=amount<=0?local:mix(local,trace(r,&bounce,bias,FLT_MAX,lights,depth+1,weight*amount,media),amount);
        return (GRErgb24){GREMin(255,result.R+specular.R*amount),GREMin(255,result.G+specular.G*amount),GREMin(255,result.B+specular.B*amount)};
    }
    float ior=material->ior>=1?material->ior:1.5f;
    float etaIn=media.count?media.ior[media.count-1]:1,etaOut=ior;RayMedia transmitted=media;
    if(entering) {
        if(transmitted.count<8){transmitted.object[transmitted.count]=t->object;transmitted.ior[transmitted.count++]=ior;}
    } else {
        int index=-1;for(int i=media.count-1;i>=0;i--)if(media.object[i]==t->object){index=i;break;}
        if(index<0)etaIn=ior; /* Camera/ray starts inside a glass volume. */
        else {for(int i=index;i<transmitted.count-1;i++){transmitted.object[i]=transmitted.object[i+1];transmitted.ior[i]=transmitted.ior[i+1];}transmitted.count--;}
        etaOut=transmitted.count?transmitted.ior[transmitted.count-1]:1;
    }
    float cosine=fmaxf(0,-YMGRE_Fvector4d_Dot(&ray->direction,&normal));
    float reflect=YMGRE_Ray_FresnelSchlick(cosine,etaIn,etaOut);
    if(!YMGRE_Ray_Refract(&ray->direction,&normal,etaIn,etaOut,&direction))return trace(r,&bounce,bias,FLT_MAX,lights,depth+1,weight,media);
    GRErgb24 reflected=trace(r,&bounce,bias,FLT_MAX,lights,depth+1,weight*reflect,media);
    YMGRE_Ray_SpawnFromSurface(&hit->position,&hit->normal,&direction,bias,&bounce);
    GRErgb24 through=trace(r,&bounce,bias,FLT_MAX,lights,depth+1,weight*(1-reflect),transmitted);
    if(entering)through=multiply(through,material->transmissionColor);
    GRErgb24 result=mix(through,reflected,reflect);
    return (GRErgb24){GREMin(255,result.R+specular.R*reflect),GREMin(255,result.G+specular.G*reflect),GREMin(255,result.B+specular.B*reflect)};
}
static GRErgb24 trace(SceneRayRenderer* r,gre_ray* ray,float low,float high,GRE_List lights,int depth,float weight,RayMedia media)
{
    if(depth>8||weight<.005f)return (GRErgb24){50,50,50};
    gre_ray_hit hit;int i=intersect(r,ray,low,high,0,&hit);
    return i<0?(GRErgb24){50,50,50}:surfaceColor(r,r->triangles+i,&hit,ray,lights,depth,weight,media);
}
static void renderPixel(SceneRayRenderer* r,GRE_Camera4d camera,GRE_List lights,int x,int y,int step)
{
    gre_ray ray;gre_ray_hit hit;GRErgb24 color={50,50,50};float depth=camera->frustum.Zfar;
    int sx=GREMin(x+step/2,camera->img.width-1),sy=GREMin(y+step/2,camera->img.height-1);
    if(YMGRE_Ray_FromCameraPixel(camera,sx,sy,&ray)) {
        float forward=YMGRE_Fvector4d_Dot(&ray.direction,&camera->move.cn);
        int index=forward>1e-8f?intersect(r,&ray,camera->frustum.Znear/forward,camera->frustum.Zfar/forward,0,&hit):-1;
        if(index>=0){color=surfaceColor(r,r->triangles+index,&hit,&ray,lights,0,1,(RayMedia){0});depth=hit.distance*forward;}
    }
    for(int yy=y;yy<y+step && yy<camera->img.height;yy++)for(int xx=x;xx<x+step&&xx<camera->img.width;xx++) {
        size_t i=(size_t)yy*camera->img.width+xx;r->colors[i]=GRE_FramePixel_From_RGB24(color);r->depths[i]=depth;
    }
}
SceneRayRenderer* SceneRay_Create(void){return calloc(1,sizeof(SceneRayRenderer));}
void SceneRay_Destroy(SceneRayRenderer* r)
{if(r){free(r->triangles);free(r->nodes);free(r->colors);free(r->depths);free(r);}}
void SceneRay_Invalidate(SceneRayRenderer* r){if(r)r->valid=0;}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int SceneRay_Render(SceneRayRenderer* r,GRE_Camera4d camera,GRE_List objects,GRE_List lights,GRE_List materials)
{
    if(!r||!camera||!objects||!lights||!materials)return -1;
    GRE_RenderTarget target=camera->target?camera->target:&camera->img;
    if(!target->width||!target->height||!target->data||!target->zbuff)return -1;
    gre_camera4d cam=*camera;cam.img=*target;
    uint64_t hash=sceneHash(&cam,objects,lights,materials);
    hash=hashBytes(hash,r->primitives,(size_t)r->primitiveCount*sizeof(SceneRayPrimitive));
    size_t size=(size_t)target->width*target->height;
    if(size>r->capacity) {
        GRE_FramePixel* colors=malloc(size*sizeof(*colors));float* depths=malloc(size*sizeof(*depths));
        if(!colors||!depths){free(colors);free(depths);return -1;}
        free(r->colors);free(r->depths);r->colors=colors;r->depths=depths;r->capacity=size;r->valid=0;
    }
    if(!r->valid||hash!=r->fingerprint) {
        r->valid=0;if(!rebuild(r,objects,materials))return -1;
        r->fingerprint=hash;r->row=0;r->valid=1;
        /* A coarse complete frame keeps navigation responsive, then refine within a time budget. */
        for(int y=0;y<target->height;y+=8)for(int x=0;x<target->width;x+=8)renderPixel(r,&cam,lights,x,y,8);
    } else {
        double deadline=now()+0.012;
        while(r->row<target->height) {
            for(int x=0;x<target->width;x++)renderPixel(r,&cam,lights,x,r->row,1);
            r->row++;if(now()>=deadline)break;
        }
    }
    /* Editor overlays may change the target every frame; never accumulate them into the cached image. */
    memcpy(target->data,r->colors,size*sizeof(*r->colors));memcpy(target->zbuff,r->depths,size*sizeof(*r->depths));
    return r->row*100/target->height;
}

void SceneRay_SetPrimitives(SceneRayRenderer* r,const SceneRayPrimitive* primitives,int count)
{
    if(!r)return;if(count<0)count=0;if(count>32)count=32;
    if(count!=r->primitiveCount||(count&&memcmp(r->primitives,primitives,count*sizeof(*primitives)))) {
        if(count)memcpy(r->primitives,primitives,count*sizeof(*primitives));r->primitiveCount=count;r->valid=0;
    }
}
int SceneRay_BakeSurface(SceneRayRenderer* r,GRE_Object4d o,GRE_Lightmap map,GRE_Camera4d camera,GRE_List objects,GRE_List lights,GRE_List materials)
{
    if(!r||!o||!map||!map->specularPixels||!camera)return 0;
    /* Capture the tessellated surface that will be exported, preserving atlas correspondence. */
    GRE_Lightmap old=o->lightmap;o->lightmap=NULL;
    int primitiveCount=r->primitiveCount;SceneRayPrimitive primitives[32];memcpy(primitives,r->primitives,sizeof(primitives));
    for(int i=0;i<r->primitiveCount;i++)if(r->primitives[i].object==o){r->primitives[i]=r->primitives[--r->primitiveCount];break;}
    int ok=rebuild(r,objects,materials);
    if(ok) {
        int grid=(int)ceil(sqrt((double)o->polygonNum)),tile=map->width/grid;
        GRE_Material m=YMGRE_Material_Find(materials,o->materiaName);
        for(int i=0;i<o->polygonNum;i++)for(int y=0;y<tile;y++)for(int x=0;x<tile;x++) {
            float b=fmaxf(0,(x+.5f-2.5f)/(tile-5)),c=fmaxf(0,(y+.5f-2.5f)/(tile-5));
            if(b+c>1){float shift=(b+c-1)*.5f;b-=shift;c-=shift;if(b<0){b=0;c=1;}if(c<0){c=0;b=1;}}
            float w[3]={1-b-c,b,c};gre_ray_hit hit={.u=b,.v=c,.normal=o->polygonList[i].pN,.position={0,0,0,1}};
            for(int j=0;j<3;j++){gre_fvector4d p=o->pointList[o->polygonList[i].index[j]].pos;hit.position.x+=w[j]*p.x;hit.position.y+=w[j]*p.y;hit.position.z+=w[j]*p.z;}
            gre_ray ray={.origin=camera->pos,.direction={hit.position.x-camera->pos.x,hit.position.y-camera->pos.y,hit.position.z-camera->pos.z,0}};
            hit.distance=YMGRE_Fvector4d_Len1(&ray.direction);YMGRE_Fvector4d_Normalize(&ray.direction);
            RayTriangle t={.object=o,.material=m,.polygon=i};
            GRErgb24 color=surfaceColor(r,&t,&hit,&ray,lights,0,1,(RayMedia){0});
            int index=((i/grid)*tile+y)*map->width+(i%grid)*tile+x;
            map->pixels[index]=(GRErgb24){0};map->specularPixels[index]=color;
        }
        map->colorsBaked=1;
    }
    o->lightmap=old;r->primitiveCount=primitiveCount;memcpy(r->primitives,primitives,sizeof(primitives));r->valid=0;return ok;
}
