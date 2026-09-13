#include "scene_uv.h"
#include "scene_uv_data.h"
#include "scene_uv_atlas.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_Mem.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define UV_MAX_FACES 20000
#define UV_CHART_FACES 256
#define UV_SIZE 1024
#define UV_PAD 4

typedef struct { double x,y,z; } V3;
typedef struct { double x,y; } V2;
typedef struct { V3 p; int vertex; } Weld;
typedef struct { int a,b,face,side; } Edge;
typedef struct { V3 n; int neighbor[3],chart; V2 uv[3]; } Face;
typedef struct { int id; double w,h,x,y; int heightOrder; } Chart;
typedef struct { GRE_Object4d source, staged; Face* faces; } Part;
static V3 sub(V3 a,V3 b){return (V3){a.x-b.x,a.y-b.y,a.z-b.z};}
static double dot(V3 a,V3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static V3 cross(V3 a,V3 b){return (V3){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static V3 unit(V3 a){double d=sqrt(dot(a,a));return (V3){a.x/d,a.y/d,a.z/d};}
static V3 pos(GRE_Object4d m,int i){gre_fvector4d p=m->pointList[i].pos;return (V3){p.x,p.y,p.z};}
static int weldCompare(const void* ap,const void* bp)
{
    const Weld *a=ap,*b=bp;
    if(a->p.x!=b->p.x)return a->p.x<b->p.x?-1:1;
    if(a->p.y!=b->p.y)return a->p.y<b->p.y?-1:1;
    if(a->p.z!=b->p.z)return a->p.z<b->p.z?-1:1;
    return 0;
}
static int edgeCompare(const void* ap,const void* bp)
{
    const Edge *a=ap,*b=bp;
    if(a->a!=b->a)return a->a<b->a?-1:1;
    if(a->b!=b->b)return a->b<b->b?-1:1;
    return a->face-b->face;
}
/* Separating axes: touching chart edges/vertices are allowed; overlapping interiors are not. */
static int overlap(const V2 a[3],const V2 b[3])
{
    for(int t=0;t<2;t++)for(int k=0;k<3;k++) {
        const V2* tri=t?b:a;V2 p=tri[k],q=tri[(k+1)%3];
        double nx=-(q.y-p.y),ny=q.x-p.x,amin=1e100,amax=-1e100,bmin=1e100,bmax=-1e100;
        for(int i=0;i<3;i++) {
            double av=a[i].x*nx+a[i].y*ny,bv=b[i].x*nx+b[i].y*ny;
            amin=fmin(amin,av);amax=fmax(amax,av);bmin=fmin(bmin,bv);bmax=fmax(bmax,bv);
        }
        if(fmin(amax,bmax)-fmax(amin,bmin)<=1e-10)return 0;
    }
    return 1;
}
static int buildCharts(Part* part,Chart* charts,int* chartCount)
{
    GRE_Object4d m=part->source;int n=m->polygonNum;
    Weld* weld=malloc(m->pointNum*sizeof(*weld));int* ids=malloc(m->pointNum*sizeof(*ids));
    Edge* edges=malloc((size_t)n*3*sizeof(*edges));Face* f=calloc(n,sizeof(*f));
    part->faces=f;int ok=0;
    if(!weld||!ids||!edges||!f)goto done;
    V3 lo=pos(m,0),hi=lo;
    for(int i=0;i<m->pointNum;i++) {
        V3 p=pos(m,i);if(!isfinite(p.x)||!isfinite(p.y)||!isfinite(p.z))goto done;
        weld[i]=(Weld){p,i};lo=(V3){fmin(lo.x,p.x),fmin(lo.y,p.y),fmin(lo.z,p.z)};
        hi=(V3){fmax(hi.x,p.x),fmax(hi.y,p.y),fmax(hi.z,p.z)};
    }
    double extent=fmax(hi.x-lo.x,fmax(hi.y-lo.y,hi.z-lo.z));if(extent<=1e-12)goto done;
    qsort(weld,m->pointNum,sizeof(*weld),weldCompare);
    int id=0;for(int i=0;i<m->pointNum;i++){if(i&&weldCompare(weld+i-1,weld+i))id++;ids[weld[i].vertex]=id;}
    for(int i=0;i<n;i++) {
        GRE_Polygon4d p=m->polygonList+i;f[i].chart=-1;
        if(p->num!=3||!p->index)goto done;
        for(int j=0;j<3;j++)if(p->index[j]>=m->pointNum)goto done;
        V3 normal=cross(sub(pos(m,p->index[1]),pos(m,p->index[0])),sub(pos(m,p->index[2]),pos(m,p->index[0])));
        if(sqrt(dot(normal,normal))<=extent*extent*1e-12)goto done;
        f[i].n=unit(normal);
        for(int j=0;j<3;j++) {
            int a=ids[p->index[j]],b=ids[p->index[(j+1)%3]];
            edges[i*3+j]=(Edge){a<b?a:b,a<b?b:a,i,j};f[i].neighbor[j]=-1;
        }
    }
    qsort(edges,n*3,sizeof(*edges),edgeCompare);
    for(int i=0;i<n*3;) {
        int end=i+1;while(end<n*3&&edges[end].a==edges[i].a&&edges[end].b==edges[i].b)end++;
        if(end-i==2) {
            Edge a=edges[i],b=edges[i+1];
            f[a.face].neighbor[a.side]=b.face;f[b.face].neighbor[b.side]=a.face;
        }
        i=end; /* Non-manifold edges remain seams. */
    }
    for(int seed=0;seed<n;seed++)if(f[seed].chart<0) {
        int chart=(*chartCount)++,queue[UV_CHART_FACES],count=1;queue[0]=seed;f[seed].chart=chart;
        V3 origin=pos(m,m->polygonList[seed].index[0]);
        V3 u=unit(sub(pos(m,m->polygonList[seed].index[1]),origin)),v=cross(f[seed].n,u);
        for(int head=0;head<count;head++) {
            int face=queue[head];
            for(int j=0;j<3;j++) {
                V3 p=sub(pos(m,m->polygonList[face].index[j]),origin);
                f[face].uv[j]=(V2){dot(p,u)/extent,dot(p,v)/extent};
            }
            for(int side=0;side<3&&count<UV_CHART_FACES;side++) {
                int next=f[face].neighbor[side];if(next<0||f[next].chart>=0||dot(f[seed].n,f[next].n)<.8660254)continue;
                V2 projected[3];for(int j=0;j<3;j++) {
                    V3 p=sub(pos(m,m->polygonList[next].index[j]),origin);
                    projected[j]=(V2){dot(p,u)/extent,dot(p,v)/extent};
                }
                int intersects=0;
                for(int k=0;k<count&&!intersects;k++)intersects=overlap(projected,f[queue[k]].uv);
                if(intersects)continue;
                memcpy(f[next].uv,projected,sizeof(projected));f[next].chart=chart;queue[count++]=next;
            }
        }
        double minx=1e100,miny=1e100,maxx=-1e100,maxy=-1e100;
        for(int k=0;k<count;k++)for(int j=0;j<3;j++) {
            V2 p=f[queue[k]].uv[j];minx=fmin(minx,p.x);miny=fmin(miny,p.y);maxx=fmax(maxx,p.x);maxy=fmax(maxy,p.y);
        }
        charts[chart]=(Chart){chart,fmax(maxx-minx,1e-8)*extent,fmax(maxy-miny,1e-8)*extent,0,0};
        for(int k=0;k<count;k++)for(int j=0;j<3;j++) {
            V2* p=&f[queue[k]].uv[j];p->x=(p->x-minx)*extent;p->y=(p->y-miny)*extent;
        }
    }
    ok=1;
done:
    free(weld);free(ids);free(edges);return ok;
}
static int chartCompare(const void* ap,const void* bp)
{
    const Chart *a=ap,*b=bp;
    if(a->heightOrder!=b->heightOrder)return a->heightOrder>b->heightOrder?-1:1;
    return a->id-b->id;
}
static int pack(Chart* order,int n,double scale,Chart* result)
{
    int x=0,y=0,row=0;
    for(int i=0;i<n;i++) {
        int w=(int)ceil(order[i].w*scale)+2*UV_PAD,h=(int)ceil(order[i].h*scale)+2*UV_PAD;
        if(w>UV_SIZE||h>UV_SIZE)return 0;
        if(x+w>UV_SIZE){x=0;y+=row;row=0;}
        if(y+h>UV_SIZE)return 0;
        if(result){result[order[i].id].x=x+UV_PAD;result[order[i].id].y=y+UV_PAD;}
        x+=w;if(h>row)row=h;
    }
    return 1;
}
static int stageMesh(Part* part,const Chart* charts,double scale)
{
    GRE_Object4d src=part->source;
    /* A source vertex is split only when its corners belong to different islands. */
    int slots=1;while(slots<src->polygonNum*6)slots*=2;
    int* table=malloc(slots*sizeof(*table));int* originals=malloc(src->polygonNum*3*sizeof(*originals));
    int* groups=malloc(src->polygonNum*3*sizeof(*groups));
    GRE_Object4d dst=calloc(1,sizeof(*dst));
    part->staged=dst;int ok=0;
    if(!table||!originals||!groups||!dst)goto done;
    dst->pointNum=src->polygonNum*3;
    dst->pointList=calloc(dst->pointNum,sizeof(*dst->pointList));
    dst->pointList_=calloc(dst->pointNum,sizeof(*dst->pointList_));
    dst->polygonList=calloc(src->polygonNum,sizeof(*dst->polygonList));
    if(!dst->pointList||!dst->pointList_||!dst->polygonList)goto done;
    dst->polygonNum=src->polygonNum;
    for(int i=0;i<src->polygonNum;i++) {
        dst->polygonList[i]=src->polygonList[i];
        dst->polygonList[i].index=GRE_PolyIndex_Malloc(3*sizeof(uint16));
        if(!dst->polygonList[i].index)goto done;
    }
    for(int i=0;i<slots;i++)table[i]=-1;
    /* Compute original smooth normals before splitting, without changing the live mesh. */
    gre_object4d reference=*src;reference.pointList_wN=NULL;reference.pointList_wN_=NULL;
    if(!YMGRE_Object_GenerateVertexAttributes(&reference))goto done;
    dst->importedNormals=malloc(src->polygonNum*3*sizeof(*dst->importedNormals));
    dst->importedUvCount=src->importedUvs?src->importedUvCount:1;
    dst->importedUvs=malloc((size_t)src->polygonNum*3*dst->importedUvCount*2*sizeof(float));
    if(!dst->importedNormals||!dst->importedUvs){free(reference.pointList_wN);free(reference.pointList_wN_);goto done;}
    int vertices=0;
    for(int i=0;i<src->polygonNum;i++) {
        int chart=part->faces[i].chart;
        for(int j=0;j<3;j++) {
            float u=(part->faces[i].uv[j].x*scale+charts[chart].x)/UV_SIZE;
            float v=(part->faces[i].uv[j].y*scale+charts[chart].y)/UV_SIZE;
            int original=src->polygonList[i].index[j];unsigned h=((unsigned)original*73856093u^(unsigned)chart*19349663u)&(slots-1);
            while(table[h]>=0&&(originals[table[h]]!=original||groups[table[h]]!=chart||dst->pointList[table[h]].u!=u||dst->pointList[table[h]].v!=v))h=(h+1)&(slots-1);
            if(table[h]<0) {
                int k=vertices++;table[h]=k;originals[k]=original;groups[k]=chart;
                dst->pointList[k]=src->pointList[original];
                dst->pointList[k].u=u;dst->pointList[k].v=v;
                dst->importedNormals[k]=reference.pointList_wN[original].normal;
                float* uv=dst->importedUvs+(size_t)k*dst->importedUvCount*2;
                if(src->importedUvs)memcpy(uv,src->importedUvs+(size_t)original*dst->importedUvCount*2,dst->importedUvCount*2*sizeof(float));
                uv[0]=dst->pointList[k].u;uv[1]=dst->pointList[k].v;
            }
            dst->polygonList[i].index[j]=table[h];
        }
    }
    free(reference.pointList_wN);free(reference.pointList_wN_);
    dst->pointNum=vertices;ok=YMGRE_Object_GenerateVertexAttributes(dst);
done:
    free(table);free(originals);free(groups);return ok;
}
int SceneUv_Generate(GRE_Object4d mesh,int* islands,char* error,size_t capacity)
{
    Part parts[64]={0};int count=0,total=0,chartCount=0,ok=0;Chart *charts=NULL,*order=NULL;
    const char* reason="无法展开：内存不足或存在无效、退化三角形";
    for(GRE_Object4d p=mesh;p;p=p->nextObject) {
        if(count==64||p->polygonNum<1||p->polygonNum>UV_MAX_FACES-total||p->pointNum<1||p->pointNum>65535||
           !p->pointList||!p->polygonList||p->importedUvCount>8||(p->importedUvs&&!p->importedUvCount)) {
            reason="自动展开支持最多 20000 个三角形、64 个子网格及 8 套 UV";goto done;
        }
        parts[count++].source=p;total+=p->polygonNum;
    }
    if(!count)goto done;
    charts=calloc(total,sizeof(*charts));order=malloc(total*sizeof(*order));if(!charts||!order)goto done;
    for(int i=0;i<count;i++)if(!buildCharts(parts+i,charts,&chartCount))goto done;
    double maximum=0;for(int i=0;i<chartCount;i++)maximum=fmax(maximum,fmax(charts[i].w,charts[i].h));
    /* Equivalent faces must not reorder after a rotation/scale round trip due to float noise. */
    for(int i=0;i<chartCount;i++) {
        charts[i].heightOrder=(int)llround(charts[i].h/maximum*100000);
        /* Stabilize integer packing at exact pixel boundaries as well. The rounding error
           is below 0.006 atlas pixels, well inside the four-pixel island padding. */
        charts[i].w=fmax(1,round(charts[i].w/maximum*100000))*maximum/100000;
        charts[i].h=fmax(1,round(charts[i].h/maximum*100000))*maximum/100000;
    }
    memcpy(order,charts,chartCount*sizeof(*charts));qsort(order,chartCount,sizeof(*order),chartCompare);
    if(!pack(order,chartCount,0,NULL)){reason="UV 碎片过多，无法保留足够的岛间边距";goto done;}
    double low=0,high=UV_SIZE/maximum;
    for(int i=0;i<40;i++){double middle=(low+high)*.5;if(pack(order,chartCount,middle,NULL))low=middle;else high=middle;}
    if(low<=0||!pack(order,chartCount,low,charts))goto done;
    for(int i=0;i<count;i++)if(!stageMesh(parts+i,charts,low))goto done;
    for(int i=0;i<count;i++) {
        GRE_Object4d src=parts[i].source,dst=parts[i].staged;
        /* Swap owned geometry buffers only; live object pointers, material and visibility stay valid. */
#define SWAP(field,type) do{type old=src->field;src->field=dst->field;dst->field=old;}while(0)
        SWAP(pointNum,int);SWAP(pointList,GRE_Vertex4d);SWAP(pointList_,GRE_Vertex4d);
        SWAP(pointList_wN,GRE_Vertex4d_wN);SWAP(pointList_wN_,GRE_Vertex4d_wN);
        SWAP(polygonList,GRE_Polygon4d);SWAP(importedUvs,float32*);SWAP(importedUvCount,uint16);
        SWAP(importedNormals,gre_fvector4d*);
#undef SWAP
        YMGRE_Free_Lightmap(src->lightmap);src->lightmap=NULL;
    }
    if(islands)*islands=chartCount;ok=1;
done:
    for(int i=0;i<count;i++){free(parts[i].faces);YMGRE_Free_Object(parts[i].staged);}
    free(charts);free(order);if(!ok&&error&&capacity)snprintf(error,capacity,"%s",reason);return ok;
}

/* The editor's built-in sphere has a known latitude/longitude topology. Reconstruct
   the same rectangular grid used by demo_advanced_earth_normal_map.c, including
   separate vertices at U=0/1 and a full row of pole UVs. Pole-row triangles with
   coincident positions are intentional: they preserve the rectangular UV domain. */
int SceneUv_Sphere(GRE_Object4d src,unsigned latitude,unsigned longitude,char* error,size_t capacity)
{
    GRE_Object4d dst=NULL;int ok=0;
    if(!src||src->nextObject||latitude<2||longitude<3||latitude>100||longitude>100||
       !src->pointList||!src->polygonList||src->importedUvCount>8||
       (src->importedUvs&&!src->importedUvCount))goto done;
    int grid=src->polygonNum==(int)(latitude*longitude*2);
    if(!grid&&src->polygonNum!=(int)(2*longitude*(latitude-1)))goto done;
    for(int i=0;i<src->polygonNum;i++) {
        GRE_Polygon4d p=src->polygonList+i;if(p->num!=3||!p->index)goto done;
        for(int j=0;j<3;j++)if(p->index[j]>=src->pointNum)goto done;
    }
    dst=calloc(1,sizeof(*dst));if(!dst)goto done;
    dst->pointNum=(latitude+1)*(longitude+1);
    dst->pointList=calloc(dst->pointNum,sizeof(*dst->pointList));
    dst->pointList_=calloc(dst->pointNum,sizeof(*dst->pointList_));
    dst->polygonList=calloc(latitude*longitude*2,sizeof(*dst->polygonList));
    dst->importedNormals=calloc(dst->pointNum,sizeof(*dst->importedNormals));
    dst->importedUvCount=src->importedUvs?src->importedUvCount:1;
    dst->importedUvs=calloc((size_t)dst->pointNum*dst->importedUvCount*2,sizeof(float));
    if(!dst->pointList||!dst->pointList_||!dst->polygonList||!dst->importedNormals||!dst->importedUvs)goto done;
    dst->polygonNum=latitude*longitude*2;
    for(unsigned y=0;y<latitude;y++)for(unsigned x=0;x<longitude;x++) {
        unsigned a=y*(longitude+1)+x,b=a+1,c=a+longitude+1,d=c+1;
        unsigned indices[6]={a,b,d,a,d,c};
        for(unsigned t=0;t<2;t++) {
            unsigned face=(y*longitude+x)*2+t;
            unsigned original=grid?face:y==0?x:y==latitude-1?src->polygonNum-longitude+x:
                longitude+((y-1)*longitude+x)*2+t;
            dst->polygonList[face]=src->polygonList[original];
            dst->polygonList[face].index=GRE_PolyIndex_Malloc(3*sizeof(uint16));
            if(!dst->polygonList[face].index)goto done;
            for(int j=0;j<3;j++)dst->polygonList[face].index[j]=indices[t*3+j];
        }
    }
    gre_object4d reference=*src;reference.pointList_wN=NULL;reference.pointList_wN_=NULL;
    if(!YMGRE_Object_GenerateVertexAttributes(&reference))goto done;
    for(unsigned y=0;y<=latitude;y++)for(unsigned x=0;x<=longitude;x++) {
        unsigned col=x%longitude,original;
        if(grid)original=y<latitude?src->polygonList[(y*longitude+col)*2].index[0]:
            src->polygonList[((y-1)*longitude+col)*2+1].index[2];
        else if(y==0)original=src->polygonList[col].index[0];
        else if(y==1)original=src->polygonList[col].index[2];
        else if(y==latitude)original=src->polygonList[src->polygonNum-longitude+col].index[0];
        else original=src->polygonList[longitude+((y-2)*longitude+col)*2+1].index[2];
        unsigned i=y*(longitude+1)+x;dst->pointList[i]=src->pointList[original];
        dst->pointList[i].u=(float)x/longitude;dst->pointList[i].v=(float)y/latitude;
        dst->importedNormals[i]=reference.pointList_wN[original].normal;
        float* uv=dst->importedUvs+(size_t)i*dst->importedUvCount*2;
        if(src->importedUvs)memcpy(uv,src->importedUvs+(size_t)original*dst->importedUvCount*2,dst->importedUvCount*2*sizeof(float));
        uv[0]=dst->pointList[i].u;uv[1]=dst->pointList[i].v;
    }
    free(reference.pointList_wN);free(reference.pointList_wN_);
    if(!YMGRE_Object_GenerateVertexAttributes(dst))goto done;
#define SWAP(field,type) do{type old=src->field;src->field=dst->field;dst->field=old;}while(0)
    SWAP(pointNum,int);SWAP(pointList,GRE_Vertex4d);SWAP(pointList_,GRE_Vertex4d);
    SWAP(pointList_wN,GRE_Vertex4d_wN);SWAP(pointList_wN_,GRE_Vertex4d_wN);
    SWAP(polygonNum,int);SWAP(polygonList,GRE_Polygon4d);
    SWAP(importedUvs,float32*);SWAP(importedUvCount,uint16);SWAP(importedNormals,gre_fvector4d*);
#undef SWAP
    YMGRE_Free_Lightmap(src->lightmap);src->lightmap=NULL;ok=1;
done:
    YMGRE_Free_Object(dst);
    if(!ok&&error&&capacity)snprintf(error,capacity,"球体展开失败：网格拓扑无效、细分过大或内存不足");
    return ok;
}

static int generateAtlas(GRE_Object4d mesh,const gre_fvector4d* center,const gre_fvector4d axes[3],int preserveAspect,int* islands,char* error,size_t capacity)
{
    Part parts[64]={0};SceneUvAtlasMesh inputs[64]={0};int count=0,total=0,ok=0,chartCount=0;
    Chart* charts=NULL;double extent=0;
    if(error&&capacity)snprintf(error,capacity,"展开失败：无效网格、超过 2 万三角形或内存不足");
    for(GRE_Object4d p=mesh;p;p=p->nextObject) {
        if(count==64||p->pointNum<1||p->pointNum>65535||p->polygonNum<1||p->polygonNum>UV_MAX_FACES-total||
           !p->pointList||!p->polygonList||p->importedUvCount>8||(p->importedUvs&&!p->importedUvCount))goto done;
        int i=count++;parts[i].source=p;total+=p->polygonNum;
        float* positions=malloc((size_t)p->pointNum*3*sizeof(float));
        unsigned* indices=malloc((size_t)p->polygonNum*3*sizeof(unsigned));
        inputs[i]=(SceneUvAtlasMesh){positions,indices,p->pointNum,p->polygonNum*3,
            malloc((size_t)p->polygonNum*6*sizeof(float)),malloc((size_t)p->polygonNum*3*sizeof(int))};
        parts[i].faces=calloc(p->polygonNum,sizeof(Face));
        if(!positions||!indices||!inputs[i].cornerUvs||!inputs[i].cornerCharts||!parts[i].faces)goto done;
        for(int v=0;v<p->pointNum;v++) {
            V3 q=pos(p,v);if(center)q=sub(q,(V3){center->x,center->y,center->z});
            for(int k=0;k<3;k++) {
                double value=axes?dot(q,(V3){axes[k].x,axes[k].y,axes[k].z}):k==0?q.x:k==1?q.y:q.z;
                if(!isfinite(value))goto done;positions[v*3+k]=value;extent=fmax(extent,fabs(value));
            }
        }
        for(int f=0;f<p->polygonNum;f++) {
            GRE_Polygon4d face=p->polygonList+f;if(face->num!=3||!face->index)goto done;
            for(int j=0;j<3;j++){if(face->index[j]>=p->pointNum)goto done;indices[f*3+j]=face->index[j];}
        }
    }
    if(!count||extent<=1e-12)goto done;
    /* Solve in a canonical object frame; quantization suppresses transform-roundtrip noise.
       Only solver input is normalized. The actual geometry buffers retain original positions. */
    for(int i=0;i<count;i++)for(unsigned j=0;j<inputs[i].vertexCount*3;j++) {
        float* p=(float*)inputs[i].positions;p[j]=round(p[j]/extent*100000)/100000;
    }
    if(!SceneUv_AtlasUnwrap(inputs,count,preserveAspect,&chartCount,error,capacity))goto done;
    charts=calloc(chartCount,sizeof(*charts));if(!charts)goto done;
    for(int i=0;i<count;i++) {
        for(int f=0;f<parts[i].source->polygonNum;f++) {
            Face* face=parts[i].faces+f;face->chart=inputs[i].cornerCharts[f*3];
            if(face->chart<0||face->chart>=chartCount)goto done;
            for(int j=0;j<3;j++)face->uv[j]=(V2){inputs[i].cornerUvs[f*6+j*2],inputs[i].cornerUvs[f*6+j*2+1]};
        }
        if(!stageMesh(parts+i,charts,UV_SIZE))goto done;
    }
    for(int i=0;i<count;i++) {
        GRE_Object4d src=parts[i].source,dst=parts[i].staged;
#define SWAP(field,type) do{type old=src->field;src->field=dst->field;dst->field=old;}while(0)
        SWAP(pointNum,int);SWAP(pointList,GRE_Vertex4d);SWAP(pointList_,GRE_Vertex4d);
        SWAP(pointList_wN,GRE_Vertex4d_wN);SWAP(pointList_wN_,GRE_Vertex4d_wN);
        SWAP(polygonList,GRE_Polygon4d);SWAP(importedUvs,float32*);SWAP(importedUvCount,uint16);
        SWAP(importedNormals,gre_fvector4d*);
#undef SWAP
        YMGRE_Free_Lightmap(src->lightmap);src->lightmap=NULL;
    }
    if(islands)*islands=chartCount;ok=1;
done:
    for(int i=0;i<count;i++) {
        free((void*)inputs[i].positions);free((void*)inputs[i].indices);free(inputs[i].cornerUvs);free(inputs[i].cornerCharts);
        free(parts[i].faces);YMGRE_Free_Object(parts[i].staged);
    }
    free(charts);return ok;
}

int SceneUv_GenerateAtlas(GRE_Object4d mesh,const gre_fvector4d* center,const gre_fvector4d axes[3],int* islands,char* error,size_t capacity)
{return generateAtlas(mesh,center,axes,1,islands,error,capacity);}
int SceneUv_GenerateAtlasLegacy(GRE_Object4d mesh,const gre_fvector4d* center,const gre_fvector4d axes[3],int* islands,char* error,size_t capacity)
{return generateAtlas(mesh,center,axes,0,islands,error,capacity);}

int SceneUvData_Apply(GRE_Object4d mesh,const SceneUvData* data)
{
    if(!data||!data->parts||data->parts>64||data->triangles>20000)return 0;
    Part parts[64]={0};float* direct[64]={0};unsigned count=0,corner=0;int ok=0;Chart chart={0};
    for(GRE_Object4d p=mesh;p;p=p->nextObject) {
        if(count>=data->parts||p->polygonNum<1||p->polygonNum!=(int)data->counts[count]||p->pointNum<1||p->pointNum>200000||
           !p->pointList||!p->polygonList||p->importedUvCount>8||(p->importedUvs&&!p->importedUvCount))goto done;
        Part* part=parts+count++;part->source=p;part->faces=calloc(p->polygonNum,sizeof(Face));if(!part->faces)goto done;
        for(int f=0;f<p->polygonNum;f++) {
            GRE_Polygon4d face=p->polygonList+f;if(face->num!=3||!face->index)goto done;
            for(int j=0;j<3;j++) {
                if(face->index[j]>=p->pointNum||corner+2>data->triangles*6)goto done;
                float u=data->corners[corner++],v=data->corners[corner++];
                if(!isfinite(u)||!isfinite(v)||fabsf(u)>1024||fabsf(v)>1024)goto done;
                part->faces[f].uv[j]=(V2){u,v};
            }
        }
        if(!stageMesh(part,&chart,UV_SIZE))goto done;
    }
    if(count!=data->parts||corner!=data->triangles*6)goto done;
    /* Preserve original vertex/index ordering whenever the saved corners fit it.
       Reindexing a merely edited mesh would invalidate its saved bake fingerprint. */
    for(unsigned i=0;i<count;i++) {
        GRE_Object4d src=parts[i].source;
        direct[i]=malloc((size_t)src->pointNum*2*sizeof(float));if(!direct[i])goto done;
        for(int k=0;k<src->pointNum*2;k++)direct[i][k]=NAN;
        int split=0;
        for(int f=0;f<src->polygonNum&&!split;f++)for(int j=0;j<3;j++) {
            unsigned k=src->polygonList[f].index[j]*2;V2 uv=parts[i].faces[f].uv[j];
            if(isfinite(direct[i][k])&&(direct[i][k]!=uv.x||direct[i][k+1]!=uv.y)){split=1;break;}
            direct[i][k]=uv.x;direct[i][k+1]=uv.y;
        }
        if(split){free(direct[i]);direct[i]=NULL;}
        else if(!YMGRE_Object_GenerateVertexAttributes(src))goto done;
    }
    for(unsigned i=0;i<count;i++) {
        GRE_Object4d src=parts[i].source,dst=parts[i].staged;
        if(direct[i]) {
            for(int k=0;k<src->pointNum;k++)if(isfinite(direct[i][k*2])) {
                src->pointList[k].u=direct[i][k*2];src->pointList[k].v=direct[i][k*2+1];
                if(src->importedUvs){size_t at=(size_t)k*src->importedUvCount*2;src->importedUvs[at]=direct[i][k*2];src->importedUvs[at+1]=direct[i][k*2+1];}
            }
            YMGRE_Object_GenerateVertexAttributes(src);continue;
        }
#define SWAP(field,type) do{type old=src->field;src->field=dst->field;dst->field=old;}while(0)
        SWAP(pointNum,int);SWAP(pointList,GRE_Vertex4d);SWAP(pointList_,GRE_Vertex4d);
        SWAP(pointList_wN,GRE_Vertex4d_wN);SWAP(pointList_wN_,GRE_Vertex4d_wN);
        SWAP(polygonList,GRE_Polygon4d);SWAP(importedUvs,float32*);SWAP(importedUvCount,uint16);
        SWAP(importedNormals,gre_fvector4d*);
#undef SWAP
    }
    ok=1;
done:
    for(unsigned i=0;i<count;i++){free(direct[i]);free(parts[i].faces);YMGRE_Free_Object(parts[i].staged);}return ok;
}

/* Physical lengths share one scale so caps stay circular and islands have matching density. */
static double uvAngle(double z,double x)
{
    double u=atan2(z,x)/(2*M_PI);if(u<0)u+=1;
    if(u<1e-6||u>1-1e-6)u=0;return u;
}
static void uvUnwrap(double u[3],const int pole[3])
{
    double lo=2,hi=-1;for(int j=0;j<3;j++)if(!pole[j]){lo=fmin(lo,u[j]);hi=fmax(hi,u[j]);}
    if(hi-lo>.5)for(int j=0;j<3;j++)if(!pole[j]&&u[j]<.5)u[j]+=1;
    double sum=0;int n=0;for(int j=0;j<3;j++)if(!pole[j]){sum+=u[j];n++;}
    for(int j=0;j<3;j++)if(pole[j])u[j]=n?sum/n:0;
}
int SceneUv_Primitive(GRE_Object4d mesh,unsigned shape,const gre_fvector4d* center,
    const gre_fvector4d axes[3],float scale,int* islands,char* error,size_t capacity)
{
    Part part={.source=mesh};int ok=0,charts=1;V3* local=NULL;
    if(error&&capacity)snprintf(error,capacity,"基本形状展开失败：无效网格或内存不足");
    if(!mesh||mesh->nextObject||shape>7||shape==3||!center||!axes||!isfinite(scale)||scale<=0||
       mesh->pointNum<1||mesh->pointNum>65535||mesh->polygonNum<1||mesh->polygonNum>UV_MAX_FACES||
       !mesh->pointList||!mesh->polygonList||mesh->importedUvCount>8||(mesh->importedUvs&&!mesh->importedUvCount))return 0;
    local=malloc(mesh->pointNum*sizeof(*local));part.faces=calloc(mesh->polygonNum,sizeof(*part.faces));if(!local||!part.faces)goto done;
    for(int i=0;i<mesh->pointNum;i++) {
        V3 p=sub(pos(mesh,i),(V3){center->x,center->y,center->z});double q[3];
        for(int k=0;k<3;k++){q[k]=dot(p,(V3){axes[k].x,axes[k].y,axes[k].z})/scale;if(!isfinite(q[k]))goto done;}
        local[i]=(V3){q[0],q[1],q[2]};
    }
    double width=48,height=48,pad=2;
    if(shape==1||shape==2){double w=shape==1?30:42,h=shape==1?30:24,d=30;width=w+d+w+4*pad;height=h+fmax(h,d)+3*pad;charts=6;}
    if(shape==4){width=2*M_PI*15+2*pad;height=32+30+3*pad;charts=3;}
    double slant=hypot(17,34),sector=2*M_PI*17/slant;
    if(shape==5){width=2*slant+2*pad;height=slant+34+3*pad;charts=2;}
    if(shape==6){width=2*M_PI*20+2*pad;height=2*M_PI*6+2*pad;}
    if(shape==7){width=2*M_PI*12+2*pad;height=26+M_PI*12+2*pad;}
    double size=fmax(width,height);Chart layout[6]={0};
    for(int i=0;i<6;i++){layout[i].x=(UV_SIZE-width/size*(UV_SIZE-16))*.5;layout[i].y=(UV_SIZE-height/size*(UV_SIZE-16))*.5;}
    for(int f=0;f<mesh->polygonNum;f++) {
        GRE_Polygon4d face=mesh->polygonList+f;V3 p[3];double u[3],v[3];int pole[3]={0};
        if(face->num!=3||!face->index)goto done;
        for(int j=0;j<3;j++){if(face->index[j]>=mesh->pointNum)goto done;p[j]=local[face->index[j]];u[j]=uvAngle(p[j].z,p[j].x);pole[j]=hypot(p[j].x,p[j].z)<1e-4;}
        uvUnwrap(u,pole);int chart=0;
        if(shape==0)for(int j=0;j<3;j++){u[j]=p[j].x+24;v[j]=p[j].z+24;}
        if(shape==1||shape==2) {
            double w=shape==1?30:42,h=shape==1?30:24,d=30;
            V3 n=cross(sub(p[1],p[0]),sub(p[2],p[0]));double a=fabs(n.x),b=fabs(n.y),c=fabs(n.z);
            chart=a>b&&a>c?(n.x>0?3:2):b>c?(n.y>0?5:4):(n.z>0?1:0);
            for(int j=0;j<3;j++) {
                if(chart<2){u[j]=pad+(chart? w+d+2*pad:0)+(chart?-p[j].x:p[j].x)+w/2;v[j]=pad+p[j].y+h/2;}
                else if(chart<4){u[j]=2*pad+w+p[j].z+d/2;v[j]=pad+(chart==3?h+pad:0)+p[j].y+h/2;}
                else {u[j]=pad+(chart==5?w+d+2*pad:0)+p[j].x+w/2;v[j]=2*pad+h+p[j].z+d/2;}
            }
        }
        if(shape==4) {
            int top=1,bottom=1;for(int j=0;j<3;j++){top&=fabs(p[j].y-16)<1e-3;bottom&=fabs(p[j].y+16)<1e-3;}
            chart=top?1:bottom?2:0;
            for(int j=0;j<3;j++)if(chart){u[j]=pad+15+(chart==2?30+pad:0)+p[j].x;v[j]=2*pad+32+15+(chart==2?-p[j].z:p[j].z);}
                else {u[j]=pad+u[j]*2*M_PI*15;v[j]=pad+16-p[j].y;}
        }
        if(shape==5) {
            int bottom=1;for(int j=0;j<3;j++)bottom&=fabs(p[j].y+17)<1e-3;chart=bottom?1:0;
            for(int j=0;j<3;j++)if(bottom){u[j]=pad+17+p[j].x;v[j]=2*pad+slant+17+p[j].z;}
                else {double a=(u[j]-.5)*sector,r=slant*(17-p[j].y)/34;u[j]=pad+slant+r*sin(a);v[j]=pad+r*cos(a);}
        }
        if(shape==6) {
            double t[3];int noPole[3]={0};for(int j=0;j<3;j++)t[j]=uvAngle(p[j].y,hypot(p[j].x,p[j].z)-20);uvUnwrap(t,noPole);
            for(int j=0;j<3;j++){u[j]=pad+u[j]*2*M_PI*20;v[j]=pad+t[j]*2*M_PI*6;}
        }
        if(shape==7)for(int j=0;j<3;j++) {
            double y=p[j].y,arc;
            if(y>13)arc=12*acos(fmax(-1,fmin(1,(y-13)/12)));
            else if(y< -13)arc=M_PI*6+26+12*asin(fmax(-1,fmin(1,(-y-13)/12)));
            else arc=M_PI*6+13-y;
            u[j]=pad+u[j]*2*M_PI*12;v[j]=pad+arc;
        }
        part.faces[f].chart=chart;
        for(int j=0;j<3;j++)part.faces[f].uv[j]=(V2){u[j],v[j]};
    }
    if(!stageMesh(&part,layout,(UV_SIZE-16)/size))goto done;
#define PRIM_SWAP(field,type) do{type old=mesh->field;mesh->field=part.staged->field;part.staged->field=old;}while(0)
    PRIM_SWAP(pointNum,int);PRIM_SWAP(pointList,GRE_Vertex4d);PRIM_SWAP(pointList_,GRE_Vertex4d);
    PRIM_SWAP(pointList_wN,GRE_Vertex4d_wN);PRIM_SWAP(pointList_wN_,GRE_Vertex4d_wN);
    PRIM_SWAP(polygonNum,int);PRIM_SWAP(polygonList,GRE_Polygon4d);
    PRIM_SWAP(importedUvs,float32*);PRIM_SWAP(importedUvCount,uint16);PRIM_SWAP(importedNormals,gre_fvector4d*);
#undef PRIM_SWAP
    YMGRE_Free_Lightmap(mesh->lightmap);mesh->lightmap=NULL;if(islands)*islands=charts;ok=1;
done:
    free(local);free(part.faces);YMGRE_Free_Object(part.staged);return ok;
}
