#include "YMGRE_PBR.h"
#include "YMGRE_MaterialRaster.h"
#include "YMGRE_MathBase.h"
#if YMGRE_ENABLE_PBR
static inline float pbr_max(float a,float b){return a>b?a:b;}
static inline float pbr_min(float a,float b){return a<b?a:b;}
typedef struct { float x,y,z; } GRE_PBRVec;
static GRE_PBRVec pbr_add(GRE_PBRVec a, GRE_PBRVec b) { return (GRE_PBRVec){a.x+b.x,a.y+b.y,a.z+b.z}; }
static GRE_PBRVec pbr_sub(GRE_PBRVec a, GRE_PBRVec b) { return (GRE_PBRVec){a.x-b.x,a.y-b.y,a.z-b.z}; }
static GRE_PBRVec pbr_scale(GRE_PBRVec a,float s) { return (GRE_PBRVec){a.x*s,a.y*s,a.z*s}; }
static GRE_PBRVec pbr_mul(GRE_PBRVec a,GRE_PBRVec b) { return (GRE_PBRVec){a.x*b.x,a.y*b.y,a.z*b.z}; }
static float pbr_dot(GRE_PBRVec a,GRE_PBRVec b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
static GRE_PBRVec pbr_cross(GRE_PBRVec a,GRE_PBRVec b) { return (GRE_PBRVec){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
static GRE_PBRVec pbr_norm(GRE_PBRVec a) { return pbr_scale(a,1.f/sqrtf(pbr_max(pbr_dot(a,a),1e-16f))); }
static float pbr_clamp(float a,float lo,float hi) { return pbr_min(hi,pbr_max(lo,a)); }
static inline GRE_PBRVec pbr_sample(GRErgb24* pixels,int w,int h,float u,float v,const float32* decode)
{
 GRErgb24 p=YMGRE_Material_Texel(pixels,w,h,u,v);
#if YMGRE_ENABLE_LINEAR_COLOR
 if(decode)return (GRE_PBRVec){decode[p.R],decode[p.G],decode[p.B]};
#else
 (void)decode;
#endif
 return (GRE_PBRVec){p.R/255.f,p.G/255.f,p.B/255.f};
}
static GRE_PBRVec pbr_camera_point(GRE_Camera4d cam,gre_vertex4d_wN* v)
{
    float vw=cam->perspectPlane.pR-cam->perspectPlane.pL;
    float vh=cam->perspectPlane.pU-cam->perspectPlane.pD;
    float z=v->base.pos.z;
    return (GRE_PBRVec){(v->base.pos.x-cam->img.width*.5f)*vw*z/(cam->img.width*cam->perspectPlane.Dis),
        (cam->img.height*.5f-v->base.pos.y)*vh*z/(cam->img.height*cam->perspectPlane.Dis),z};
}

static GRE_PBRVec pbr_shade(GRE_PBRVec base,GRE_PBRVec mrs,float ior,GRE_PBRVec normal,GRE_PBRVec pos,
    GRE_List lights,gre_fvector4d* lightPositions,GRE_FMat4x4 matrix)
{
    const float pi=3.14159265358979323846f;
    float metal=pbr_clamp(mrs.x,0,1),rough=pbr_clamp(mrs.y,.065f,1);
    float f=(ior-1)/(ior+1); f=pbr_clamp(2*mrs.z*f*f,0,1);
    GRE_PBRVec f0=pbr_add(pbr_scale((GRE_PBRVec){f,f,f},1-metal),pbr_scale(base,metal));
    GRE_PBRVec view=pbr_norm(pbr_scale(pos,-1)),out={0,0,0};
    float nv=pbr_max(pbr_dot(normal,view),.001f),a=rough*rough,a2=a*a;
    unsigned li=0;
    for(GRE_ListNode node=lights->listhead;node;node=node->next,li++) {
        GRE_Light4d light=node->data;
        GRE_PBRVec color={light->proper.lightcolor.R/255.f,light->proper.lightcolor.G/255.f,light->proper.lightcolor.B/255.f};
        if(light->ishide)continue;
        float strength=light->proper.strength;
        if(light->type==GRE_GlobalLight) {
            /* Constant environment approximation, not an environment-map reflection. */
            GRE_PBRVec ambient=pbr_add(pbr_scale(base,1-metal),pbr_scale(f0,.4f+(.4f*metal)));
            out=pbr_add(out,pbr_scale(pbr_mul(ambient,color),strength));
            continue;
        }
        if(light->type!=GRE_PointLight && light->type!=GRE_SpotLight) continue;
        GRE_PBRVec l=pbr_sub((GRE_PBRVec){lightPositions[li].x,lightPositions[li].y,lightPositions[li].z},pos);
        float dist=sqrtf(pbr_max(pbr_dot(l,l),1e-12f));l=pbr_scale(l,1/dist);
        if(light->type==GRE_SpotLight){
            gre_fvector4d direction=light->proper.spot.direct;
            direction.w=0;if(matrix)YMGRE_Fvector4d_MatMultTo(matrix,&direction,&direction);
            GRE_PBRVec d=pbr_norm((GRE_PBRVec){direction.x,direction.y,direction.z});
            float cone=pbr_dot(d,pbr_scale(l,-1));
            float range=light->proper.spot.cs_inner_angle-light->proper.spot.cs_outer_angle;
            float weight=range>0?pbr_clamp((cone-light->proper.spot.cs_outer_angle)/range,0,1):(cone>=light->proper.spot.cs_outer_angle?1:0);
            strength*=weight*weight*(3-2*weight);
        }
        float nl=pbr_max(pbr_dot(normal,l),0); if(nl<=0)continue;
        GRE_PBRVec half=pbr_norm(pbr_add(view,l));
        float nh=pbr_max(pbr_dot(normal,half),0),vh=pbr_clamp(pbr_dot(view,half),0,1);
        float denom=nh*nh*(a2-1)+1,D=a2/(pi*denom*denom);
        float k=(rough+1)*(rough+1)/8;
        float G=(nv/(nv*(1-k)+k))*(nl/(nl*(1-k)+k));
        float s=1-vh,schlick=s*s*s*s*s;
        GRE_PBRVec F=pbr_add(f0,pbr_scale(pbr_sub((GRE_PBRVec){1,1,1},f0),schlick));
        GRE_PBRVec diffuse=pbr_scale(pbr_mul(pbr_sub((GRE_PBRVec){1,1,1},F),base),(1-metal)/pi);
        GRE_PBRVec specular=pbr_scale(F,D*G/pbr_max(4*nv*nl,.00001f));
        float atten=light->proper.kc0+light->proper.kc1*dist+light->proper.kc2*dist*dist;
        out=pbr_add(out,pbr_scale(pbr_mul(pbr_add(diffuse,specular),color),pi*strength*nl/pbr_max(atten,.0001f)));
    }
    return out;
}

void YMGRE_PBR_FillRows(GRE_Vertex4d_wN vertices,GRE_Polygon4d poly,GRE_Material material,
    GRE_List lights,gre_fvector4d* lightPositions,GRE_FMat4x4 matrix,GRE_Camera4d cam,int yBegin,int yEnd)
{
    if(!vertices||!poly||poly->num!=3||!poly->index||!material||!material->advanced||!lights||!cam||!cam->img.data||!cam->img.zbuff)return;
    GRE_MaterialAdvanced adv=material->advanced;
    int linear=0;
    const float32* decode=NULL;
#if YMGRE_ENABLE_LINEAR_COLOR
    linear=cam->linearColorEnabled&&cam->linearColor;
    if(linear)decode=YMGRE_Color_DecodeTable();
#endif
    float ior=adv->pbrIOR>=1?adv->pbrIOR:1.45f;
    GRE_Vertex4d_wN a=&vertices[poly->index[0]],b=&vertices[poly->index[1]],c=&vertices[poly->index[2]];
    int tinted=material->diffuse.R!=255||material->diffuse.G!=255||material->diffuse.B!=255||
        a->color.R!=255||a->color.G!=255||a->color.B!=255||b->color.R!=255||b->color.G!=255||b->color.B!=255||c->color.R!=255||c->color.G!=255||c->color.B!=255;
    float den=(b->base.pos.y-c->base.pos.y)*(a->base.pos.x-c->base.pos.x)+(c->base.pos.x-b->base.pos.x)*(a->base.pos.y-c->base.pos.y);
    if(fabsf(den)<1e-6f)return;
    unsigned lod=YMGRE_Material_OpacityLOD(material,a,b,c,den);
    YMGRE_ColorMipView colorMip=YMGRE_Material_ColorMipForTriangle(material,a,b,c);
    GRE_PBRVec pa=pbr_camera_point(cam,a),pb=pbr_camera_point(cam,b),pc=pbr_camera_point(cam,c);
    GRE_PBRVec e1=pbr_sub(pb,pa),e2=pbr_sub(pc,pa);
    float du1=b->base.u-a->base.u,dv1=b->base.v-a->base.v,du2=c->base.u-a->base.u,dv2=c->base.v-a->base.v;
    float det=du1*dv2-du2*dv1;
    GRE_PBRVec tangent={1,0,0},bitangent={0,1,0};
    if(fabsf(det)>1e-10f) {
        tangent=pbr_scale(pbr_sub(pbr_scale(e1,dv2),pbr_scale(e2,dv1)),1/det);
        bitangent=pbr_scale(pbr_sub(pbr_scale(e2,du1),pbr_scale(e1,du2)),1/det);
    }
    int minX=GREMax((int)floorf(pbr_min(a->base.pos.x,pbr_min(b->base.pos.x,c->base.pos.x))),0);
    int maxX=GREMin((int)ceilf(pbr_max(a->base.pos.x,pbr_max(b->base.pos.x,c->base.pos.x))),cam->img.width-1);
    int minY=GREMax((int)floorf(pbr_min(a->base.pos.y,pbr_min(b->base.pos.y,c->base.pos.y))),GREMax(0,yBegin));
    int maxY=GREMin((int)ceilf(pbr_max(a->base.pos.y,pbr_max(b->base.pos.y,c->base.pos.y))),GREMin(cam->img.height,yEnd)-1);
    GRE_Vertex4d_wN corners[3]={a,b,c};
    for(int y=minY;y<=maxY;y++){
        float left=(float)maxX+1,right=(float)minX-1;
        for(int edge=0;edge<3;edge++){
            gre_fvector4d p=corners[edge]->base.pos,q=corners[(edge+1)%3]->base.pos;
            if(y<pbr_min(p.y,q.y)||y>pbr_max(p.y,q.y))continue;
            if(p.y==q.y){left=pbr_min(left,pbr_min(p.x,q.x));right=pbr_max(right,pbr_max(p.x,q.x));}
            else {float x=p.x+(y-p.y)*(q.x-p.x)/(q.y-p.y);left=pbr_min(left,x);right=pbr_max(right,x);}
        }
        if(right<left)continue;
        int begin=GREMax(minX,(int)floorf(left)-1),end=GREMin(maxX,(int)ceilf(right)+1);
        for(int x=begin;x<=end;x++) {
        float w0=((b->base.pos.y-c->base.pos.y)*(x-c->base.pos.x)+(c->base.pos.x-b->base.pos.x)*(y-c->base.pos.y))/den;
        float w1=((c->base.pos.y-a->base.pos.y)*(x-c->base.pos.x)+(a->base.pos.x-c->base.pos.x)*(y-c->base.pos.y))/den,w2=1-w0-w1;
        if(w0<0||w1<0||w2<0)continue;
        float ia=w0/a->base.pos.z,ib=w1/b->base.pos.z,ic=w2/c->base.pos.z,sum=ia+ib+ic;
        if(sum<=0)continue;
        float z=1/sum,wa=ia*z,wb=ib*z,wc=ic*z;
        int index=y*cam->img.width+x;
        if(cam->img.zbuff[index]<=z||z<=cam->frustum.Znear)continue;
        float u=wa*a->base.u+wb*b->base.u+wc*c->base.u,v=wa*a->base.v+wb*b->base.v+wc*c->base.v;
        uint8 alpha=YMGRE_Material_Opacity(material,u,v,lod);
        if(!YMGRE_Material_AcceptAlpha(cam,alpha))continue;
        GRE_PBRVec normal=pbr_norm((GRE_PBRVec){wa*a->normal.x+wb*b->normal.x+wc*c->normal.x,wa*a->normal.y+wb*b->normal.y+wc*c->normal.y,wa*a->normal.z+wb*b->normal.z+wc*c->normal.z});
        if(cam->pbrNormalEnabled && adv->normalPixel && fabsf(det)>1e-10f) {
            GRE_PBRVec nm=pbr_sample(adv->normalPixel,adv->normalWidth,adv->normalHeight,u,v,0);
            nm=pbr_sub(pbr_scale(nm,2),(GRE_PBRVec){1,1,1});
            nm.x*=adv->pbrNormalStrength;nm.y*=adv->pbrNormalStrength;
            nm.z=1+(nm.z-1)*adv->pbrNormalStrength;
            GRE_PBRVec t=pbr_norm(pbr_sub(tangent,pbr_scale(normal,pbr_dot(normal,tangent))));
            GRE_PBRVec bt=pbr_cross(normal,t);
            if(pbr_dot(bt,bitangent)<0)bt=pbr_scale(bt,-1);
            normal=pbr_norm(pbr_add(pbr_add(pbr_scale(t,nm.x),pbr_scale(bt,nm.y)),pbr_scale(normal,nm.z)));
        }
        GRE_PBRVec pos=pbr_add(pbr_add(pbr_scale(pa,wa),pbr_scale(pb,wb)),pbr_scale(pc,wc));
        GRE_PBRVec base=colorMip.pixels?pbr_sample(colorMip.pixels,colorMip.width,colorMip.height,u,v,decode):(GRE_PBRVec){1,1,1};
        if(tinted){
        base.x*=material->diffuse.R/255.f*(wa*a->color.R+wb*b->color.R+wc*c->color.R)/255.f;
        base.y*=material->diffuse.G/255.f*(wa*a->color.G+wb*b->color.G+wc*c->color.G)/255.f;
        base.z*=material->diffuse.B/255.f*(wa*a->color.B+wb*b->color.B+wc*c->color.B)/255.f;
        }
        GRE_PBRVec mrs=adv->pbrParameters?pbr_sample(adv->pbrParameters,adv->pbrWidth,adv->pbrHeight,u,v,0):(GRE_PBRVec){0,.5f,.5f};
        GRE_PBRVec color=pbr_shade(base,mrs,ior,normal,pos,lights,lightPositions,matrix);
#if YMGRE_ENABLE_LINEAR_COLOR
        if(linear){
            float coverage=alpha/255.f,*dst=cam->linearColor+index*3;
            dst[0]=color.x*coverage+dst[0]*(1-coverage);
            dst[1]=color.y*coverage+dst[1]*(1-coverage);
            dst[2]=color.z*coverage+dst[2]*(1-coverage);
            if(alpha==255)cam->img.zbuff[index]=z;
        }else
#endif
        YMGRE_Material_WriteRGB(cam,index,z,(GRErgb24){(uint8)(255*pbr_clamp(color.x,0,1)),(uint8)(255*pbr_clamp(color.y,0,1)),(uint8)(255*pbr_clamp(color.z,0,1))},alpha);
    }
    }
}

void YMGRE_PBR_Fill(GRE_Vertex4d_wN vertices,GRE_Polygon4d poly,GRE_Material material,
 GRE_List lights,gre_fvector4d* lightPositions,GRE_FMat4x4 matrix,GRE_Camera4d cam)
{
 YMGRE_PBR_FillRows(vertices,poly,material,lights,lightPositions,matrix,cam,0,cam?cam->img.height:0);
}

#endif
