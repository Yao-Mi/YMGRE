#include "scene_uv_atlas.h"
#include "xatlas.h"
#include <cstdio>
#include <cmath>
int SceneUv_AtlasUnwrap(SceneUvAtlasMesh* meshes,unsigned count,int preserveAspect,int* charts,char* error,size_t capacity)
{
    xatlas::Atlas* atlas=xatlas::Create();if(!atlas)return 0;
    bool ok=true;
    for(unsigned i=0;i<count&&ok;i++) {
        xatlas::MeshDecl input;
        input.vertexPositionData=meshes[i].positions;input.vertexPositionStride=sizeof(float)*3;
        input.vertexCount=meshes[i].vertexCount;input.indexData=meshes[i].indices;
        input.indexCount=meshes[i].indexCount;input.indexFormat=xatlas::IndexFormat::UInt32;
        ok=xatlas::AddMesh(atlas,input,count)==xatlas::AddMeshError::Success;
    }
    if(ok) {
        xatlas::ChartOptions chart;
        chart.normalSeamWeight=0;chart.textureSeamWeight=0;
        chart.normalDeviationWeight=1;chart.maxIterations=4;chart.maxCost=8;
        xatlas::PackOptions pack;pack.padding=4;
        xatlas::Generate(atlas,chart,pack);
        ok=atlas->meshCount==count&&atlas->width>0&&atlas->height>0&&atlas->atlasCount<=1;
    }
    for(unsigned i=0;i<count&&ok;i++) {
        const auto& output=atlas->meshes[i];ok=output.indexCount==meshes[i].indexCount;
        for(unsigned k=0;k<output.indexCount&&ok;k++) {
            const auto& v=output.vertexArray[output.indexArray[k]];
            ok=v.xref==meshes[i].indices[k]&&v.chartIndex>=0&&v.atlasIndex==0&&std::isfinite(v.uv[0])&&std::isfinite(v.uv[1]);
            // Use a square UV domain without stretching a rectangular packed atlas.
            // The legacy normalization is retained only for loading autoUv=3 scenes.
            const unsigned squareSize=atlas->width>atlas->height?atlas->width:atlas->height;
            meshes[i].cornerUvs[k*2]=(v.uv[0]+4)/((preserveAspect?squareSize:atlas->width)+8);
            meshes[i].cornerUvs[k*2+1]=(v.uv[1]+4)/((preserveAspect?squareSize:atlas->height)+8);
            meshes[i].cornerCharts[k]=v.chartIndex;
        }
    }
    if(ok&&charts)*charts=atlas->chartCount;
    if(!ok&&error&&capacity)std::snprintf(error,capacity,"展开失败：请检查退化面或无效网格");
    xatlas::Destroy(atlas);return ok;
}
