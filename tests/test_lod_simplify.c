#include "YMGRE_LOD_Simplify.h"
#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_Mem.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#define CHECK(x) do { if(!(x)) { fprintf(stderr,"LOD simplify failed at line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static int valid(GRE_Object4d object)
{
    for(GRE_Object4d p=object;p;p=p->nextObject){
        if(!p->pointNum||!p->polygonNum)return 0;
        for(int i=0;i<p->polygonNum;i++){
            GRE_Polygon4d f=&p->polygonList[i];
            if(f->num!=3||!f->index)return 0;
            for(int j=0;j<3;j++)if(f->index[j]>=p->pointNum)return 0;
            if(!isfinite(f->pN.x)||!isfinite(f->pN.y)||!isfinite(f->pN.z))return 0;
        }
    }
    return 1;
}
int main(void)
{
    GRE_Object4d sphere=YMGRE_MeshGener_Sphere(1,20,24,(GRErgb24){255,255,255},"sphere","white");
    CHECK(sphere);
    int original=sphere->polygonNum;
    YMGRE_LOD_SimplifyResult stats={0};
    GRE_Object4d simpler=YMGRE_LOD_SimplifyMesh(sphere,
        (YMGRE_LOD_SimplifyOptions){.target_ratio=.5f,.max_relative_error=.2f},&stats);
    CHECK(simpler&&valid(simpler));
    CHECK(sphere->polygonNum==original&&stats.input_triangles==(uint32)original);
    CHECK(simpler->polygonNum<original&&stats.output_triangles==(uint32)simpler->polygonNum);
    CHECK(stats.collapsed_triangles>0);
    YMGRE_Free_Object(simpler);YMGRE_Free_Object(sphere);

    GRE_Object4d cards=YMGRE_Creat_Object(16,8,"cards","leaf");
    for(int c=0;c<4;c++){
        int v=c*4;
        cards->pointList[v+0]=(gre_vertex4d){{c*2,0,0,1},0,0};
        cards->pointList[v+1]=(gre_vertex4d){{c*2+1,0,0,1},1,0};
        cards->pointList[v+2]=(gre_vertex4d){{c*2,1,0,1},0,1};
        cards->pointList[v+3]=(gre_vertex4d){{c*2+1,1,0,1},1,1};
        for(int t=0;t<2;t++){
            GRE_Polygon4d f=&cards->polygonList[c*2+t];
            f->num=3;f->index=GRE_PolyIndex_Malloc(3*sizeof(GRE_Index));
            GRE_Index a[3]={v,v+1,v+2},b[3]={v+1,v+3,v+2};
            for(int j=0;j<3;j++)f->index[j]=t?b[j]:a[j];
        }
    }
    stats=(YMGRE_LOD_SimplifyResult){0};
    simpler=YMGRE_LOD_SimplifyMesh(cards,
        (YMGRE_LOD_SimplifyOptions){.target_ratio=.5f,.prune_components=1},&stats);
    CHECK(simpler&&valid(simpler));
    CHECK(simpler->polygonNum==4&&stats.collapsed_triangles+stats.pruned_triangles==4);
    CHECK(cards->polygonNum==8);
    YMGRE_Free_Object(simpler);YMGRE_Free_Object(cards);

    /* A double-sided thin sheet must be reduced on both sides, not removed as
       one large component. This mirrors the topology of the imported grass. */
    enum { ROWS=4, COLS=5, SIDE_VERTS=(ROWS+1)*(COLS+1), SIDE_FACES=2*ROWS*COLS };
    GRE_Object4d sheet=YMGRE_Creat_Object(2*SIDE_VERTS,2*SIDE_FACES,"paired sheet","grass");
    CHECK(sheet);
    for(int side=0;side<2;side++){
        for(int y=0;y<=ROWS;y++)for(int x=0;x<=COLS;x++){
            int vi=side*SIDE_VERTS+y*(COLS+1)+x;
            sheet->pointList[vi]=(gre_vertex4d){{x*.2f,y*.2f,0,1},x/(float)COLS,y/(float)ROWS};
        }
        for(int y=0;y<ROWS;y++)for(int x=0;x<COLS;x++){
            int a=side*SIDE_VERTS+y*(COLS+1)+x,b=a+1,c=a+COLS+1,d=c+1;
            GRE_Index front[6]={(GRE_Index)a,(GRE_Index)b,(GRE_Index)c,
                                (GRE_Index)b,(GRE_Index)d,(GRE_Index)c};
            GRE_Index back[6]={(GRE_Index)a,(GRE_Index)c,(GRE_Index)b,
                               (GRE_Index)b,(GRE_Index)c,(GRE_Index)d};
            GRE_Index *indices=side?back:front;
            for(int t=0;t<2;t++){
                GRE_Polygon4d f=&sheet->polygonList[side*SIDE_FACES+2*(y*COLS+x)+t];
                f->num=3;f->index=GRE_PolyIndex_Malloc(3*sizeof(GRE_Index));
                CHECK(f->index);
                for(int j=0;j<3;j++)f->index[j]=indices[3*t+j];
            }
        }
    }
    stats=(YMGRE_LOD_SimplifyResult){0};
    simpler=YMGRE_LOD_SimplifyMesh(sheet,
        (YMGRE_LOD_SimplifyOptions){.target_ratio=.5f,.max_relative_error=.2f,.prune_components=1},&stats);
    CHECK(simpler&&valid(simpler));
    CHECK(stats.pruned_triangles==0&&stats.collapsed_triangles>0);
    int positive=0,negative=0;
    for(int i=0;i<simpler->polygonNum;i++){
        if(simpler->polygonList[i].pN.z>0)positive++;
        if(simpler->polygonList[i].pN.z<0)negative++;
    }
    CHECK(positive>0&&negative>0&&abs(positive-negative)<=2);
    YMGRE_Free_Object(simpler);YMGRE_Free_Object(sheet);
    puts("PASS: QEM continuous mesh, paired thin sheets, and small foliage pieces");
    return 0;
}
