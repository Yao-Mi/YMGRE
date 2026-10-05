#include "YMGRE_LOD_Simplify.h"
#include "../OPOBJ/YMGRE_Creat.h"
#include "../OPOBJ/YMGRE_Free.h"
#include "../CONFIG/YMGRE_Mem.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

typedef struct {
    gre_vertex4d vertex;
    gre_fvector4d normal;
    GRErgb24 color;
} LodVertex;
typedef struct {
    uint32 v[3], source;
    uint8 active;
} LodFace;
typedef struct {
    uint32 a,b;
    uint32 count,face;
    double cost;
    float32 t;
} LodEdge;
typedef struct { double v[10]; } LodQuadric;
typedef struct { uint32 root, triangles; double area; } LodComponent;

static int edgeCompare(const void *pa,const void *pb)
{
    const LodEdge *a=pa,*b=pb;
    if(a->a!=b->a)return a->a<b->a?-1:1;
    return a->b<b->b?-1:a->b>b->b?1:0;
}
static int costCompare(const void *pa,const void *pb)
{
    const LodEdge *a=pa,*b=pb;
    if(a->cost!=b->cost)return a->cost<b->cost?-1:1;
    return edgeCompare(pa,pb);
}
static int componentCompare(const void *pa,const void *pb)
{
    const LodComponent *a=pa,*b=pb;
    if(a->area!=b->area)return a->area<b->area?-1:1;
    return a->root<b->root?-1:a->root>b->root?1:0;
}
static uint32 rootOf(uint32 *parent,uint32 x)
{
    while(parent[x]!=x){parent[x]=parent[parent[x]];x=parent[x];}
    return x;
}
static void addPlane(LodQuadric *q,double x,double y,double z,double w)
{
    q->v[0]+=x*x;q->v[1]+=x*y;q->v[2]+=x*z;q->v[3]+=x*w;
    q->v[4]+=y*y;q->v[5]+=y*z;q->v[6]+=y*w;
    q->v[7]+=z*z;q->v[8]+=z*w;q->v[9]+=w*w;
}
static double quadricError(const LodQuadric *a,const LodQuadric *b,
                           double x,double y,double z)
{
    double q[10];for(int i=0;i<10;i++)q[i]=a->v[i]+b->v[i];
    return q[0]*x*x+2*q[1]*x*y+2*q[2]*x*z+2*q[3]*x+
           q[4]*y*y+2*q[5]*y*z+2*q[6]*y+q[7]*z*z+2*q[8]*z+q[9];
}
static double faceCross(const gre_fvector4d *a,const gre_fvector4d *b,
                        const gre_fvector4d *c,double out[3])
{
    double ux=b->x-a->x,uy=b->y-a->y,uz=b->z-a->z;
    double vx=c->x-a->x,vy=c->y-a->y,vz=c->z-a->z;
    out[0]=uy*vz-uz*vy;out[1]=uz*vx-ux*vz;out[2]=ux*vy-uy*vx;
    return sqrt(out[0]*out[0]+out[1]*out[1]+out[2]*out[2]);
}
static uint32 activeCount(const LodFace *faces,uint32 count)
{
    uint32 n=0;for(uint32 i=0;i<count;i++)n+=faces[i].active!=0;return n;
}
static void buildQuadrics(const LodVertex *vertices,const LodFace *faces,
                          uint32 faceCount,LodQuadric *quadrics,uint32 vertexCount)
{
    memset(quadrics,0,(size_t)vertexCount*sizeof(*quadrics));
    for(uint32 i=0;i<faceCount;i++)if(faces[i].active){
        const LodFace *f=&faces[i];
        const gre_fvector4d *a=&vertices[f->v[0]].vertex.pos;
        const gre_fvector4d *b=&vertices[f->v[1]].vertex.pos;
        const gre_fvector4d *c=&vertices[f->v[2]].vertex.pos;
        double n[3],length=faceCross(a,b,c,n);if(length<1e-12)continue;
        double x=n[0]/length,y=n[1]/length,z=n[2]/length;
        double w=-(x*a->x+y*a->y+z*a->z);
        for(int j=0;j<3;j++)addPlane(&quadrics[f->v[j]],x,y,z,w);
    }
}
static uint32 buildEdges(const LodFace *faces,uint32 faceCount,LodEdge *edges,
                         uint8 *boundary,uint32 vertexCount)
{
    uint32 n=0;
    for(uint32 i=0;i<faceCount;i++)if(faces[i].active)for(int j=0;j<3;j++){
        uint32 a=faces[i].v[j],b=faces[i].v[(j+1)%3];
        if(a==b)continue;
        if(a>b){uint32 t=a;a=b;b=t;}
        edges[n++]=(LodEdge){.a=a,.b=b,.count=1,.face=i};
    }
    qsort(edges,n,sizeof(*edges),edgeCompare);
    memset(boundary,0,vertexCount);
    uint32 unique=0;
    for(uint32 i=0;i<n;){
        uint32 j=i+1;while(j<n&&edges[j].a==edges[i].a&&edges[j].b==edges[i].b)j++;
        edges[unique]=edges[i];edges[unique].count=j-i;
        if(j-i==1){boundary[edges[i].a]=1;boundary[edges[i].b]=1;}
        unique++;i=j;
    }
    return unique;
}
static void addBoundaryQuadrics(const LodVertex *vertices,const LodFace *faces,
                                const LodEdge *edges,uint32 edgeCount,LodQuadric *quadrics)
{
    for(uint32 i=0;i<edgeCount;i++)if(edges[i].count==1){
        const LodEdge *edge=&edges[i];
        const LodFace *face=&faces[edge->face];
        const gre_fvector4d *a=&vertices[edge->a].vertex.pos;
        const gre_fvector4d *b=&vertices[edge->b].vertex.pos;
        double e[3]={b->x-a->x,b->y-a->y,b->z-a->z};
        double n[3],length=faceCross(&vertices[face->v[0]].vertex.pos,
                                     &vertices[face->v[1]].vertex.pos,
                                     &vertices[face->v[2]].vertex.pos,n);
        double edgeLength=sqrt(e[0]*e[0]+e[1]*e[1]+e[2]*e[2]);
        if(length<1e-12||edgeLength<1e-12)continue;
        for(int j=0;j<3;j++){n[j]/=length;e[j]/=edgeLength;}
        double p[3]={e[1]*n[2]-e[2]*n[1],e[2]*n[0]-e[0]*n[2],
                     e[0]*n[1]-e[1]*n[0]};
        double normalLength=sqrt(p[0]*p[0]+p[1]*p[1]+p[2]*p[2]);
        if(normalLength<1e-12)continue;
        const double strength=4;
        double x=p[0]*strength/normalLength,y=p[1]*strength/normalLength;
        double z=p[2]*strength/normalLength;
        double w=-(x*a->x+y*a->y+z*a->z);
        addPlane(&quadrics[edge->a],x,y,z,w);
        addPlane(&quadrics[edge->b],x,y,z,w);
    }
}
static float32 chooseEdge(LodEdge *edge,const LodVertex *vertices,
                          const LodQuadric *quadrics,double diagonal2)
{
    const gre_fvector4d *a=&vertices[edge->a].vertex.pos,*b=&vertices[edge->b].vertex.pos;
    const float32 choices[3]={0,.5f,1};
    double best=HUGE_VAL;float32 bestT=0;
    for(int i=0;i<3;i++){
        float32 t=choices[i];double x=a->x+(b->x-a->x)*t;
        double y=a->y+(b->y-a->y)*t,z=a->z+(b->z-a->z)*t;
        double cost=quadricError(&quadrics[edge->a],&quadrics[edge->b],x,y,z);
        if(cost<best){best=cost;bestT=t;}
    }
    float32 du=vertices[edge->a].vertex.u-vertices[edge->b].vertex.u;
    float32 dv=vertices[edge->a].vertex.v-vertices[edge->b].vertex.v;
    edge->cost=fmax(0,best)+diagonal2*.0001*(du*du+dv*dv);
    edge->t=bestT;
    return bestT;
}
static int collapseValid(const LodFace *faces,uint32 faceCount,const LodVertex *vertices,
                         uint32 a,uint32 b,gre_fvector4d target,double minArea)
{
    for(uint32 i=0;i<faceCount;i++)if(faces[i].active){
        const LodFace *f=&faces[i];int hasA=0,hasB=0;
        for(int j=0;j<3;j++){hasA|=f->v[j]==a;hasB|=f->v[j]==b;}
        if(!(hasA||hasB)||(hasA&&hasB))continue;
        const gre_fvector4d *old[3]={&vertices[f->v[0]].vertex.pos,
                                      &vertices[f->v[1]].vertex.pos,
                                      &vertices[f->v[2]].vertex.pos};
        const gre_fvector4d *next[3]={old[0],old[1],old[2]};
        for(int j=0;j<3;j++)if(f->v[j]==a||f->v[j]==b)next[j]=&target;
        double before[3],after[3];double lenA=faceCross(old[0],old[1],old[2],before);
        double lenB=faceCross(next[0],next[1],next[2],after);
        if(lenB<minArea||lenA<minArea||
           (before[0]*after[0]+before[1]*after[1]+before[2]*after[2])<.25*lenA*lenB)return 0;
    }
    return 1;
}
static int linkValid(const LodFace *faces,uint32 faceCount,uint32 vertexCount,
                     uint32 a,uint32 b,uint32 expectedCommon,uint8 *neighbors)
{
    memset(neighbors,0,vertexCount);
    for(uint32 i=0;i<faceCount;i++)if(faces[i].active){
        const LodFace *f=&faces[i];
        if(f->v[0]!=a&&f->v[1]!=a&&f->v[2]!=a)continue;
        for(int j=0;j<3;j++)if(f->v[j]!=a&&f->v[j]!=b)neighbors[f->v[j]]=1;
    }
    int common=0;
    for(uint32 i=0;i<faceCount;i++)if(faces[i].active){
        const LodFace *f=&faces[i];
        if(f->v[0]!=b&&f->v[1]!=b&&f->v[2]!=b)continue;
        for(int j=0;j<3;j++){
            uint32 v=f->v[j];
            if(v!=a&&v!=b&&neighbors[v]==1){neighbors[v]=2;common++;}
        }
    }
    return common==(int)expectedCommon;
}
static void collapseEdge(LodFace *faces,uint32 faceCount,LodVertex *vertices,
                         float32 *uvs,uint16 uvChannels,uint32 a,uint32 b,float32 t,
                         uint8 *blocked)
{
    for(uint32 i=0;i<faceCount;i++)if(faces[i].active){
        int affected=0;
        for(int j=0;j<3;j++)affected|=faces[i].v[j]==a||faces[i].v[j]==b;
        if(affected)for(int j=0;j<3;j++)blocked[faces[i].v[j]]=1;
    }
    gre_vertex4d *va=&vertices[a].vertex,*vb=&vertices[b].vertex;
    va->pos.x+=(vb->pos.x-va->pos.x)*t;
    va->pos.y+=(vb->pos.y-va->pos.y)*t;
    va->pos.z+=(vb->pos.z-va->pos.z)*t;
    va->u+=(vb->u-va->u)*t;va->v+=(vb->v-va->v)*t;
    vertices[a].normal.x+=(vertices[b].normal.x-vertices[a].normal.x)*t;
    vertices[a].normal.y+=(vertices[b].normal.y-vertices[a].normal.y)*t;
    vertices[a].normal.z+=(vertices[b].normal.z-vertices[a].normal.z)*t;
    vertices[a].color.R=(uint8)(vertices[a].color.R+(vertices[b].color.R-vertices[a].color.R)*t);
    vertices[a].color.G=(uint8)(vertices[a].color.G+(vertices[b].color.G-vertices[a].color.G)*t);
    vertices[a].color.B=(uint8)(vertices[a].color.B+(vertices[b].color.B-vertices[a].color.B)*t);
    for(uint16 ch=0;ch<uvChannels;ch++)for(int xy=0;xy<2;xy++){
        size_t ia=((size_t)a*uvChannels+ch)*2+xy,ib=((size_t)b*uvChannels+ch)*2+xy;
        uvs[ia]+=(uvs[ib]-uvs[ia])*t;
    }
    if(uvChannels){uvs[(size_t)a*uvChannels*2]=va->u;uvs[(size_t)a*uvChannels*2+1]=va->v;}
    for(uint32 i=0;i<faceCount;i++)if(faces[i].active){
        LodFace *f=&faces[i];
        for(int j=0;j<3;j++)if(f->v[j]==b)f->v[j]=a;
        if(f->v[0]==f->v[1]||f->v[1]==f->v[2]||f->v[2]==f->v[0])f->active=0;
    }
}
static uint32 pruneComponents(const LodVertex *vertices,LodFace *faces,uint32 faceCount,
                              uint32 vertexCount,uint32 target)
{
    uint32 *parent=malloc((size_t)vertexCount*sizeof(*parent));
    LodComponent *components=calloc(vertexCount,sizeof(*components));
    if(!parent||!components){free(parent);free(components);return 0;}
    for(uint32 i=0;i<vertexCount;i++)parent[i]=i;
    for(uint32 i=0;i<faceCount;i++)if(faces[i].active){
        uint32 a=rootOf(parent,faces[i].v[0]);
        for(int j=1;j<3;j++)parent[rootOf(parent,faces[i].v[j])]=a;
    }
    for(uint32 i=0;i<faceCount;i++)if(faces[i].active){
        LodFace *f=&faces[i];uint32 r=rootOf(parent,f->v[0]);
        components[r].root=r;components[r].triangles++;
        double cross[3];components[r].area+=faceCross(&vertices[f->v[0]].vertex.pos,
            &vertices[f->v[1]].vertex.pos,&vertices[f->v[2]].vertex.pos,cross)*.5;
    }
    uint32 count=0,remaining=activeCount(faces,faceCount),removed=0;
    for(uint32 i=0;i<vertexCount;i++)if(components[i].triangles)components[count++]=components[i];
    qsort(components,count,sizeof(*components),componentCompare);
    /* Keep at least the largest component so each material submesh survives. */
    for(uint32 i=0;i+1<count&&remaining>target;i++){
        LodComponent *c=&components[i];
        /* Removing a whole large sheet/card creates visible LOD popping. */
        if(c->triangles>16||remaining-c->triangles<target)continue;
        for(uint32 j=0;j<faceCount;j++)if(faces[j].active&&rootOf(parent,faces[j].v[0])==c->root){
            faces[j].active=0;removed++;
        }
        remaining-=c->triangles;
    }
    free(parent);free(components);return removed;
}
static GRE_Object4d simplifyPart(GRE_Object4d source,YMGRE_LOD_SimplifyOptions options,
                                 YMGRE_LOD_SimplifyResult *result)
{
    if(!source||source->pointNum<=0||source->polygonNum<=0||source->lightmap||
       !source->pointList||!source->polygonList)return NULL;
    uint32 nv=(uint32)source->pointNum,nf=(uint32)source->polygonNum;
    size_t edgeBytes=(size_t)nf*3*sizeof(LodEdge);
    size_t vertexBytes=(size_t)nv*sizeof(LodVertex);
    if(edgeBytes/(3*sizeof(LodEdge))!=nf||vertexBytes/sizeof(LodVertex)!=nv)return NULL;
    for(uint32 i=0;i<nf;i++){
        GRE_Polygon4d f=&source->polygonList[i];
        if(f->num!=3||!f->index)return NULL;
        for(int j=0;j<3;j++)if(f->index[j]>=nv)return NULL;
    }
    LodVertex *vertices=calloc(nv,sizeof(*vertices));
    LodFace *faces=calloc(nf,sizeof(*faces));
    LodQuadric *quadrics=calloc(nv,sizeof(*quadrics));
    LodEdge *edges=malloc(edgeBytes);
    uint8 *boundary=calloc(nv,1),*blocked=calloc(nv,1),*neighbors=calloc(nv,1);
    uint32 *map=malloc((size_t)nv*sizeof(*map));
    uint16 channels=source->importedUvCount;
    if(channels&&nv>SIZE_MAX/((size_t)channels*2*sizeof(float32)))channels=0;
    float32 *uvs=channels?malloc((size_t)nv*channels*2*sizeof(*uvs)):NULL;
    if(!vertices||!faces||!quadrics||!edges||!boundary||!blocked||!neighbors||!map||
       (source->importedUvCount&&!uvs))goto fail;
    gre_fvector4d lo=source->pointList[0].pos,hi=lo;
    for(uint32 i=0;i<nv;i++){
        vertices[i].vertex=source->pointList[i];
        vertices[i].normal=source->importedNormals?source->importedNormals[i]:
            source->pointList_wN?source->pointList_wN[i].normal:(gre_fvector4d){0,0,0,0};
        vertices[i].color=source->pointList_wN?source->pointList_wN[i].color:(GRErgb24){255,255,255};
        if(channels){
            if(source->importedUvs)memcpy(uvs+(size_t)i*channels*2,
                source->importedUvs+(size_t)i*channels*2,(size_t)channels*2*sizeof(float32));
            else {
                memset(uvs+(size_t)i*channels*2,0,(size_t)channels*2*sizeof(float32));
                uvs[(size_t)i*channels*2]=vertices[i].vertex.u;
                uvs[(size_t)i*channels*2+1]=vertices[i].vertex.v;
            }
        }
        gre_fvector4d p=vertices[i].vertex.pos;
        if(p.x<lo.x)lo.x=p.x;
        if(p.y<lo.y)lo.y=p.y;
        if(p.z<lo.z)lo.z=p.z;
        if(p.x>hi.x)hi.x=p.x;
        if(p.y>hi.y)hi.y=p.y;
        if(p.z>hi.z)hi.z=p.z;
    }
    double diagonal2=(hi.x-lo.x)*(hi.x-lo.x)+(hi.y-lo.y)*(hi.y-lo.y)+
                     (hi.z-lo.z)*(hi.z-lo.z);
    if(diagonal2<1e-12)diagonal2=1;
    for(uint32 i=0;i<nf;i++){
        faces[i]=(LodFace){.v={source->polygonList[i].index[0],
                                   source->polygonList[i].index[1],
                                   source->polygonList[i].index[2]},.source=i,.active=1};
    }
    uint32 target=(uint32)ceilf(nf*options.target_ratio);
    if(target<1)target=1;
    if(target>nf)target=nf;
    double limit=options.max_relative_error>0?
        (double)options.max_relative_error*options.max_relative_error*diagonal2:HUGE_VAL;
    for(uint32 round=0;round<64&&activeCount(faces,nf)>target;round++){
        buildQuadrics(vertices,faces,nf,quadrics,nv);
        uint32 ne=buildEdges(faces,nf,edges,boundary,nv);
        addBoundaryQuadrics(vertices,faces,edges,ne,quadrics);
        uint32 candidateCount=0;
        for(uint32 i=0;i<ne;i++)if(edges[i].count==1||
                (edges[i].count==2&&!boundary[edges[i].a]&&!boundary[edges[i].b])){
            chooseEdge(&edges[i],vertices,quadrics,diagonal2);
            if(edges[i].cost<=limit)edges[candidateCount++]=edges[i];
        }
        qsort(edges,candidateCount,sizeof(*edges),costCompare);
        memset(blocked,0,nv);uint32 before=activeCount(faces,nf);
        for(uint32 i=0;i<candidateCount&&activeCount(faces,nf)>target;i++){
            LodEdge *edge=&edges[i];uint32 a=edge->a,b=edge->b;
            if(blocked[a]||blocked[b])continue;
            gre_fvector4d pa=vertices[a].vertex.pos,pb=vertices[b].vertex.pos;
            gre_fvector4d point={pa.x+(pb.x-pa.x)*edge->t,
                pa.y+(pb.y-pa.y)*edge->t,pa.z+(pb.z-pa.z)*edge->t,1};
            if(!linkValid(faces,nf,nv,a,b,edge->count,neighbors)||
               !collapseValid(faces,nf,vertices,a,b,point,diagonal2*1e-12))continue;
            collapseEdge(faces,nf,vertices,uvs,channels,a,b,edge->t,blocked);
        }
        if(activeCount(faces,nf)==before)break;
    }
    uint32 afterCollapse=activeCount(faces,nf);
    uint32 pruned=options.prune_components?
        pruneComponents(vertices,faces,nf,nv,target):0;
    uint32 outputFaces=afterCollapse-pruned;
    for(uint32 i=0;i<nv;i++)map[i]=UINT32_MAX;
    uint32 outputVertices=0;
    for(uint32 i=0;i<nf;i++)if(faces[i].active)for(int j=0;j<3;j++)
        if(map[faces[i].v[j]]==UINT32_MAX)map[faces[i].v[j]]=outputVertices++;
    if(!outputFaces||!outputVertices||outputVertices>YMGRE_MAX_VERTICES)goto fail;
    GRE_Object4d out=YMGRE_Creat_Object((int)outputVertices,(int)outputFaces,
                                        source->objName,source->materiaName);
    if(!out)goto fail;
    out->polygonNum=0; /* Keep partial-construction cleanup safe. */
    out->WorldCoordinate=source->WorldCoordinate;out->direct=source->direct;
    out->scale=source->scale;out->isVisible=source->isVisible;
    out->boundType=source->boundType;out->BoundingSphereR=source->BoundingSphereR;
    out->BoundingBoxMin=source->BoundingBoxMin;out->BoundingBoxMax=source->BoundingBoxMax;
    out->renderMode=source->renderMode;out->wireFrame=source->wireFrame;
    out->mirrorKs=source->mirrorKs;
    for(uint32 i=0;i<nv;i++)if(map[i]!=UINT32_MAX){
        out->pointList[map[i]]=vertices[i].vertex;
        out->pointList_[map[i]]=vertices[i].vertex;
    }
    if(channels){
        out->importedUvs=GRE_GeometryBuff_Malloc((size_t)outputVertices*channels*2*sizeof(float32));
        if(!out->importedUvs){YMGRE_Free_Object(out);goto fail;}
        out->importedUvCount=channels;
        for(uint32 i=0;i<nv;i++)if(map[i]!=UINT32_MAX)
            memcpy(out->importedUvs+(size_t)map[i]*channels*2,
                   uvs+(size_t)i*channels*2,(size_t)channels*2*sizeof(float32));
    }
    if(source->importedNormals){
        out->importedNormals=GRE_GeometryBuff_Malloc((size_t)outputVertices*sizeof(gre_fvector4d));
        if(!out->importedNormals){YMGRE_Free_Object(out);goto fail;}
        for(uint32 i=0;i<nv;i++)if(map[i]!=UINT32_MAX)out->importedNormals[map[i]]=vertices[i].normal;
    }
    uint32 next=0;
    for(uint32 i=0;i<nf;i++)if(faces[i].active){
        GRE_Polygon4d dst=&out->polygonList[next++];
        *dst=source->polygonList[faces[i].source];
        dst->num=3;dst->index=GRE_PolyIndex_Malloc(3*sizeof(GRE_Index));
        if(!dst->index){YMGRE_Free_Object(out);goto fail;}
        for(int j=0;j<3;j++)dst->index[j]=(GRE_Index)map[faces[i].v[j]];
        out->polygonNum=(int)next;
        double n[3];const gre_fvector4d *a=&out->pointList[dst->index[0]].pos;
        const gre_fvector4d *b=&out->pointList[dst->index[1]].pos;
        const gre_fvector4d *c=&out->pointList[dst->index[2]].pos;
        double length=faceCross(a,b,c,n);
        dst->pN=(gre_fvector4d){n[0]/length,n[1]/length,n[2]/length,0};
    }
    if(source->pointList_wN){
        if(!YMGRE_Object_GenerateVertexAttributes(out)){YMGRE_Free_Object(out);goto fail;}
        for(uint32 i=0;i<nv;i++)if(map[i]!=UINT32_MAX){
            out->pointList_wN[map[i]].color=vertices[i].color;
            out->pointList_wN_[map[i]].color=vertices[i].color;
        }
    }
    result->input_triangles+=nf;
    result->output_triangles+=outputFaces;
    result->collapsed_triangles+=nf-afterCollapse;
    result->pruned_triangles+=pruned;
    free(vertices);free(faces);free(quadrics);free(edges);
    free(boundary);free(blocked);free(neighbors);free(map);free(uvs);
    return out;
fail:
    free(vertices);free(faces);free(quadrics);free(edges);
    free(boundary);free(blocked);free(neighbors);free(map);free(uvs);
    return NULL;
}

GRE_Object4d YMGRE_LOD_SimplifyMesh(GRE_Object4d source,
                                    YMGRE_LOD_SimplifyOptions options,
                                    YMGRE_LOD_SimplifyResult *result)
{
    if(!source||!isfinite(options.target_ratio)||options.target_ratio<=0||
       options.target_ratio>1||!isfinite(options.max_relative_error)||
       options.max_relative_error<0)return NULL;
    YMGRE_LOD_SimplifyResult local={0};
    GRE_Object4d head=NULL,tail=NULL;
    for(GRE_Object4d part=source;part;part=part->nextObject){
        GRE_Object4d created=simplifyPart(part,options,&local);
        if(!created){YMGRE_Free_Object(head);return NULL;}
        if(tail)tail->nextObject=created;else head=created;
        tail=created;
    }
    if(result)*result=local;
    return head;
}
