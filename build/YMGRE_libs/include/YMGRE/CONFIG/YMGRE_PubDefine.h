#ifndef _YMGRE_Pubdef_H
#define _YMGRE_Pubdef_H

#define _VS201X_USE 1 //使用VS平台

#ifdef _VS201X_USE
#pragma warning(disable:4996)
#endif

#include <stdio.h>

#include"./YMGRE_PubType.h"

#ifndef NULL
#define NULL (void*)0
#endif
#define DefaultPolygonClv  128
#define DefaultPolygonColor  (GRErgb24){ DefaultPolygonClv,DefaultPolygonClv,DefaultPolygonClv } //默认平面颜色为 128 = 2^7

#define YMGRE_VECTOR_USE     0//向量创建开启
#define YMGRE_F32MAT_USE     0//矩阵创建开启

//文件系统定义
#define YMGRE_FILE FILE
#define YMGRE_fopen fopen
#define YMGRE_fread fread
#define YMGRE_fgets fgets
#define YMGRE_fclose fclose
#define YMGRE_feof feof
#define YMGRE_fwrite fwrite
#define YMGRE_FILE_END EOF

#define YMGRE_V2_0 20210313

//常用量
#define GREUint8ySize sizeof(uint8)
#define GREUint16Size sizeof(uint16)
#define GREFloat32Size sizeof(float32)
#define GREInt32Size sizeof(int32)


#define  LimitMaxMin(xMin,x,xMax)    (((x) > xMax) ? xMax : ((x) < xMin) ? xMin : x)
#define  GREMax(x,y)    (((x) > (y)) ? (x) : (y))
#define  GREMin(x,y)    (((x) < (y)) ? (x) : (y))

#include <math.h>
#define YMGRE_Abs(x)   abs((int)(x))
#define YMGRE_Fabs(x)  fabsf((float)(x))
#define YMGRE_Sin(x)   sinf((float)(x))
#define YMGRE_Cos(x)   cosf((float)(x))
#define YMGRE_Tan(x)   tanf((float)(x))
#define YMGRE_Atan(x)  atanf((float)(x))
#define YMGRE_Atan2(y,x)  atan2f((float)(y),(float)(x))
#define YMGRE_Log(x)   logf((float)(x))
#define YMGRE_Pow(x,y) powf((float)(x),(float)(y))
#define YMGRE_Exp(x)   expf((float)(x))
#define YMGRE_Fabs(x)  fabsf((float)(x))
#define YMGRE_Sqrt(x)  sqrtf((float)(x))
#define YMGRE_Ceil(x) ceilf((float)(x))
#define YMGRE_Floor(x) floorf((float)(x))
#define YMGRE_Round(x) roundf((float)(x))

#define YMGRE_2Pai 6.2831853f
#define YMGRE_Pai 3.1415926f
#define YMGRE_Pai2 1.57079632f
#define YMGRE_Pai4 0.78539816f
#define YMGRE_Deg2Rad 0.0174532922f//(pai/180)
#define YMGRE_Rad2Deg 57.2957805f//(180/pai)



#include <stdlib.h>
#define YMGRE_Srand(seed) srand(seed)
#define YMGRE_Randint() rand()
#define YMGRE_Float32Max 3.402823466e+38f
#define YMGRE_Strtof(str,endpos) strtof(str,endpos) //float返回

#include <string.h>
#define YMGRE_Memcmp memcmp


#include <ctype.h>
#define YMGRE_Isspace(a) isspace(a) //空白字符判断，如空格，换行等等 


#endif // __Pubdef_H


