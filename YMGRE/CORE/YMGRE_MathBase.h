#ifndef YMGRE_MATHBASE_H
#define YMGRE_MATHBASE_H
#include "../OPOBJ/YMGRE_OBJ.h"
#include "../OPOBJ/YMGRE_Creat.h"
#include "../DEBUG/YMGRE_Debug.h"
#include "../CONFIG/YMGRE_PubDefine.h"

/***********************************************************  F2D 向量运算  *****************************************************************************/

//向量长度
static inline float32 YMGRE_Fvector2d_Len1(GRE_Fvector2d vec)//L1范数
{
	return YMGRE_Sqrt(vec->x * vec->x + vec->y * vec->y);
}
static inline float32 YMGRE_Fvector2d_Len2(GRE_Fvector2d vec)//L2范数
{
	return vec->x * vec->x + vec->y * vec->y;
}
//向量归一化
static inline void YMGRE_Fvector2d_Normalize(GRE_Fvector2d vec)
{
	float32 len = YMGRE_Fvector2d_Len1(vec);
	if (len < 1e-8f) len = 1e-8f;
	//归一化
	len = 1.0f / len;
	vec->x *= len;
	vec->y *= len;
}
//向量点乘
static inline float32 YMGRE_Fvector2d_Dot(GRE_Fvector2d vec0, GRE_Fvector2d vec1)
{
	return vec0->x * vec1->x + vec0->y * vec1->y;
}
//夹角
static inline float32 YMGRE_Fvector2d_CosA(GRE_Fvector2d vec0, GRE_Fvector2d vec1)
{
	float32 len0 = YMGRE_Fvector2d_Len1(vec0);
	float32 len1 = YMGRE_Fvector2d_Len1(vec1);

	float32 lallbl = len0 * len1;
	if (lallbl < 1e-8f) lallbl = 1e-8f;//避免计算产生nan

	float32 cosval = YMGRE_Fvector2d_Dot(vec0, vec1) / lallbl;
	return cosval;
}
//向量取反
static inline void YMGRE_Fvector2d_NegTo(GRE_Fvector2d vec)
{
	vec->x = -vec->x;
	vec->y = -vec->y;
}
//向量赋值
static inline void YMGRE_Fvector2d_AssTo(GRE_Fvector2d vec, GRE_Fvector2d vec1)
{
	vec->x = vec1->x;
	vec->y = vec1->y;
}
//向量加法
static inline void YMGRE_Fvector2d_AddTo(GRE_Fvector2d vec, GRE_Fvector2d vec1)
{
	vec->x += vec1->x;
	vec->y += vec1->y;
}
//向量减法
static inline void YMGRE_Fvector2d_SubTo(GRE_Fvector2d vec, GRE_Fvector2d vec1)
{
	vec->x -= vec1->x;
	vec->y -= vec1->y;
}
//向量相等
static inline int8 YMGRE_Fvector2d_isEqual(GRE_Fvector2d vec, GRE_Fvector2d vec1)
{
	if ((vec->x == vec1->x) && (vec->y == vec1->y))
	{
		return 1;
	}
	return 0;
}

//向量数乘
static inline GRE_Fvector2d YMGRE_Fvector2d_Scale(GRE_Fvector2d vec, float32 lambda)
{
	GRE_Fvector2d vout = YMGRE_Creat_VectorF2d();
	vout->x = vec->x * lambda;
	vout->y = vec->y * lambda;
	return vout;
}
//向量加法
static inline GRE_Fvector2d YMGRE_Fvector2d_Add(GRE_Fvector2d vec0, GRE_Fvector2d vec1)
{
	GRE_Fvector2d vout = YMGRE_Creat_VectorF2d();
	vout->x = vec0->x + vec1->x;
	vout->y = vec0->y + vec1->y;
	return vout;
}
//向量减法 
static inline GRE_Fvector2d YMGRE_Fvector2d_Sub(GRE_Fvector2d vec0, GRE_Fvector2d vec1)
{
	GRE_Fvector2d vout = YMGRE_Creat_VectorF2d();
	vout->x = vec0->x - vec1->x;
	vout->y = vec0->y - vec1->y;
	return vout;
}


/***********************************************************  F3D 向量运算  *****************************************************************************/

//向量长度
static inline float32 YMGRE_Fvector3d_Len1(GRE_Fvector3d vec)//L1范数
{
	return YMGRE_Sqrt(vec->x * vec->x + vec->y * vec->y + vec->z * vec->z);
}
static inline float32 YMGRE_Fvector3d_Len2(GRE_Fvector3d vec)//L2范数
{
	return vec->x * vec->x + vec->y * vec->y + vec->z * vec->z;
}
//向量归一化
static inline void YMGRE_Fvector3d_Normalize(GRE_Fvector3d vec)
{
	float32 len = YMGRE_Fvector3d_Len1(vec);
	if (len < 1e-8f) len = 1e-8f;
	//归一化
	len = 1.0f / len;
	vec->x *= len;
	vec->y *= len;
	vec->z *= len;
}
//向量点乘
static inline float32 YMGRE_Fvector3d_Dot(GRE_Fvector3d vec0, GRE_Fvector3d vec1)
{
	return vec0->x * vec1->x + vec0->y * vec1->y + vec0->z * vec1->z;
}
//夹角
static inline float32 YMGRE_Fvector3d_CosA(GRE_Fvector3d vec0, GRE_Fvector3d vec1)
{
	float32 len0 = YMGRE_Fvector3d_Len1(vec0);
	float32 len1 = YMGRE_Fvector3d_Len1(vec1);

	float32 lallbl = len0 * len1;
	if (lallbl < 1e-8f) lallbl = 1e-8f;//避免计算产生nan

	float32 cosval = YMGRE_Fvector3d_Dot(vec0, vec1) / lallbl;
	return cosval;
}
//向量取反
static inline void YMGRE_Fvector3d_NegTo(GRE_Fvector3d vec)
{
	vec->x = -vec->x;
	vec->y = -vec->y;
	vec->z = -vec->z;
}
//向量赋值
static inline void YMGRE_Fvector3d_AssTo(GRE_Fvector3d vec, GRE_Fvector3d vec1)
{
	vec->x = vec1->x;
	vec->y = vec1->y;
	vec->z = vec1->z;
}
//向量加法
static inline void YMGRE_Fvector3d_AddTo(GRE_Fvector3d vec, GRE_Fvector3d vec1)
{
	vec->x += vec1->x;
	vec->y += vec1->y;
	vec->z += vec1->z;
}
//向量减法
static inline void YMGRE_Fvector3d_SubTo(GRE_Fvector3d vec, GRE_Fvector3d vec1)
{
	vec->x -= vec1->x;
	vec->y -= vec1->y;
	vec->z -= vec1->z;
}
//向量相等
static inline int8 YMGRE_Fvector3d_isEqual(GRE_Fvector3d vec, GRE_Fvector3d vec1)
{
	if ((vec->x == vec1->x) && (vec->y == vec1->y) && (vec->z == vec1->z))
	{
		return 1;
	}
	return 0;
}

//向量数乘
static inline GRE_Fvector3d YMGRE_Fvector3d_Scale(GRE_Fvector3d vec, float32 lambda)
{
	GRE_Fvector3d vout = YMGRE_Creat_VectorF3d();
	vout->x = vec->x * lambda;
	vout->y = vec->y * lambda;
	vout->z = vec->z * lambda;
	return vout;
}
//向量加法
static inline GRE_Fvector3d YMGRE_Fvector3d_Add(GRE_Fvector3d vec0, GRE_Fvector3d vec1)
{
	GRE_Fvector3d vout = YMGRE_Creat_VectorF3d();
	vout->x = vec0->x + vec1->x;
	vout->y = vec0->y + vec1->y;
	vout->z = vec0->z + vec1->z;
	return vout;
}
//向量减法 
static inline GRE_Fvector3d YMGRE_Fvector3d_Sub(GRE_Fvector3d vec0, GRE_Fvector3d vec1)
{
	GRE_Fvector3d vout = YMGRE_Creat_VectorF3d();
	vout->x = vec0->x - vec1->x;
	vout->y = vec0->y - vec1->y;
	vout->z = vec0->z - vec1->z;
	return vout;
}


/***********************************************************  F4D 向量运算  *****************************************************************************/

//向量长度
static inline float32 YMGRE_Fvector4d_Len1(GRE_Fvector4d vec)//L1范数
{
	gre_log_explain(vec == NULL, GRE_LOG_PtrI, "输入的向量不存在");
	return YMGRE_Sqrt(vec->x * vec->x + vec->y * vec->y + vec->z * vec->z + vec->w * vec->w);
}
static inline float32 YMGRE_Fvector4d_Len2(GRE_Fvector4d vec)//L2范数
{
	gre_log_explain(vec == NULL, GRE_LOG_PtrI, "输入的向量不存在");
	return vec->x * vec->x + vec->y * vec->y + vec->z * vec->z + vec->w * vec->w;
}
//向量归一化
static inline void YMGRE_Fvector4d_Normalize(GRE_Fvector4d vec)
{
	gre_log_explain(vec == NULL, GRE_LOG_PtrI, "输入的向量不存在");
	float32 len = YMGRE_Fvector4d_Len1(vec);
	if (len < 1e-8f) len = 1e-8f;
	//归一化
	len = 1.0f / len;
	vec->x *= len;
	vec->y *= len;
	vec->z *= len;
	vec->w *= len;
}
//向量点乘
static inline float32 YMGRE_Fvector4d_Dot(GRE_Fvector4d vec0, GRE_Fvector4d vec1)
{
	gre_log_explain((vec0 == NULL) || (vec1 == NULL), GRE_LOG_PtrI, "输入的向量不存在");
	return vec0->x * vec1->x + vec0->y * vec1->y + vec0->z * vec1->z + vec0->w * vec1->w;
}
//夹角
static inline float32 YMGRE_Fvector4d_CosA(GRE_Fvector4d vec0, GRE_Fvector4d vec1)
{
	gre_log_explain((vec0 == NULL) || (vec1 == NULL), GRE_LOG_PtrI, "输入的向量不存在");
	float32 len0 = YMGRE_Fvector4d_Len1(vec0);
	float32 len1 = YMGRE_Fvector4d_Len1(vec1);

	float32 lallbl = len0 * len1;
	if (lallbl < 1e-8f) lallbl = 1e-8f;//避免计算产生nan

	float32 cosval = YMGRE_Fvector4d_Dot(vec0, vec1) / lallbl;
	return cosval;
}
//向量取反
static inline void YMGRE_Fvector4d_NegTo(GRE_Fvector4d vec)
{
	gre_log_explain(vec == NULL, GRE_LOG_PtrI, "输入的向量不存在");
	vec->x = -vec->x;
	vec->y = -vec->y;
	vec->z = -vec->z;
	vec->w = -vec->w;
}
//向量赋值
static inline void YMGRE_Fvector4d_AssTo(GRE_Fvector4d vec, GRE_Fvector4d vec1)
{
	gre_log_explain((vec == NULL) || (vec1 == NULL), GRE_LOG_PtrI, "输入的向量不存在");
	vec->x = vec1->x;
	vec->y = vec1->y;
	vec->z = vec1->z;
	vec->w = vec1->w;
}
static inline void YMGRE_Fvector4d_AddTo(GRE_Fvector4d vec, GRE_Fvector4d vec1)
{
	gre_log_explain((vec == NULL) || (vec1 == NULL), GRE_LOG_PtrI, "输入的向量不存在");
	vec->x += vec1->x;
	vec->y += vec1->y;
	vec->z += vec1->z;
	vec->w += vec1->w;
}
static inline void YMGRE_Fvector4d_SubTo(GRE_Fvector4d vec, GRE_Fvector4d vec1)
{
	gre_log_explain((vec == NULL) || (vec1 == NULL), GRE_LOG_PtrI, "输入的向量不存在");
	vec->x -= vec1->x;
	vec->y -= vec1->y;
	vec->z -= vec1->z;
	vec->w -= vec1->w;
}
static inline void YMGRE_Fvector4d_ScaleTo(GRE_Fvector4d vec, float32 lambda)
{
	gre_log_explain(vec == NULL, GRE_LOG_PtrI, "输入的向量不存在");
	vec->x *= lambda;
	vec->y *= lambda;
	vec->z *= lambda;
	//vec->w *= 1; 参考放缩矩阵
}
//向量相等
static inline int8 YMGRE_Fvector4d_isEqual(GRE_Fvector4d vec, GRE_Fvector4d vec1)
{
	gre_log_explain((vec == NULL) || (vec1 == NULL), GRE_LOG_PtrI, "输入的向量不存在");
	if ((vec->x == vec1->x) && (vec->y == vec1->y) && (vec->z == vec1->z) && (vec->w == vec1->w))
	{
		return 1;
	}
	return 0;
}

//向量数乘
static inline GRE_Fvector4d YMGRE_Fvector4d_Scale(GRE_Fvector4d vec, float32 lambda)
{
	gre_log_explain(vec == NULL, GRE_LOG_PtrI, "输入的向量不存在");
	GRE_Fvector4d vout = YMGRE_Creat_VectorF4d();
	vout->x = vec->x * lambda;
	vout->y = vec->y * lambda;
	vout->z = vec->z * lambda;
	vout->w = vec->w;// 放缩矩阵该值是 * 1
	return vout;
}
//向量加法
static inline GRE_Fvector4d YMGRE_Fvector4d_Add(GRE_Fvector4d vec0, GRE_Fvector4d vec1)
{
	gre_log_explain((vec0 == NULL) || (vec1 == NULL), GRE_LOG_PtrI, "输入的向量不存在");
	GRE_Fvector4d vout = YMGRE_Creat_VectorF4d();
	vout->x = vec0->x + vec1->x;
	vout->y = vec0->y + vec1->y;
	vout->z = vec0->z + vec1->z;
	vout->w = vec0->w + vec1->w;
	return vout;
}
//向量减法 
static inline GRE_Fvector4d YMGRE_Fvector4d_Sub(GRE_Fvector4d vec0, GRE_Fvector4d vec1)
{
	gre_log_explain((vec0 == NULL) || (vec1 == NULL), GRE_LOG_PtrI, "输入的向量不存在");
	GRE_Fvector4d vout = YMGRE_Creat_VectorF4d();
	vout->x = vec0->x - vec1->x;
	vout->y = vec0->y - vec1->y;
	vout->z = vec0->z - vec1->z;
	vout->w = vec0->w - vec1->w;
	return vout;
}
//向量叉积
//参考：https://www.zhihu.com/question/536971503/answer/2522534794
static inline GRE_Fvector4d YMGRE_Fvector4d_Cross(GRE_Fvector4d va, GRE_Fvector4d vb)
{
	gre_log_explain((va == NULL) || (vb == NULL), GRE_LOG_PtrI, "输入的向量不存在");
	GRE_Fvector4d vout = YMGRE_Creat_VectorF4d();
	vout->x = va->y * vb->z - vb->y * va->z;
	vout->y = va->z * vb->x - vb->z * va->x;
	vout->z = va->x * vb->y - vb->x * va->y;

	vout->w = 0;
	return vout;
}


/***********************************************************   矩阵运算  *****************************************************************************/

/**
  * @brief	矩阵乘法 out = mat2 * mat1
  */
static inline GRE_FMat4x4 YMGRE_FMat_Mult_Cal(GRE_FMat4x4 mat2, GRE_FMat4x4 mat1)
{
	gre_log_explain((mat1 == NULL) || (mat2 == NULL), GRE_LOG_PtrI, "输入的待处理矩阵不存在");

	GRE_FMat4x4 mat_o = YMGRE_Creat_FMAT4x4();
	//  mat2  * mat1
	//  x y z   a   o
	//	      * b =
	//			c
	//
	//输出矩阵的列与mat1同，行与mat2同
	for (uint16 k = 0; k < 4; k++)
	{
		for (uint16 i = 0; i < 4; i++)
		{
			//输出数据
			mat_o->val[i][k] =	mat2->val[i][0] * mat1->val[0][k] + mat2->val[i][1] * mat1->val[1][k] + \
								mat2->val[i][2] * mat1->val[2][k] + mat2->val[i][3] * mat1->val[3][k];
		}
	}
	return mat_o;
}

/**
  * @brief	矩阵转置 out = mat1 ^T
  */
static inline GRE_FMat4x4 YMGRE_FMat4x4_Trans_Cal(GRE_FMat4x4 mat1)
{
	gre_log_explain(mat1 == NULL, GRE_LOG_PtrI, "输入的待处理矩阵不存在");

	GRE_FMat4x4 mat_o = YMGRE_Creat_FMAT4x4();
	//输出矩阵的列与mat1行同，行与mat1列同
	for (uint16 k = 0; k < 4; k++)
	{
		for (uint16 i = 0; i < 4; i++)
		{
			//行列数据交换
			mat_o->val[i][k] = mat1->val[k][i];
		}
	}
	return mat_o;
}


//平移
static inline GRE_FMat4x4 YMGRE_FMat4x4_Move_Cal(float32 x, float32 y, float32 z )
{
	GRE_FMat4x4 mat_o = YMGRE_Creat_FMAT4x4();
	//单位矩阵 + 平移向量
	mat_o->val[0][0] = 1; mat_o->val[0][1] = 0; mat_o->val[0][2] = 0; mat_o->val[0][3] = x;
	mat_o->val[1][0] = 0; mat_o->val[1][1] = 1; mat_o->val[1][2] = 0; mat_o->val[1][3] = y;
	mat_o->val[2][0] = 0; mat_o->val[2][1] = 0; mat_o->val[2][2] = 1; mat_o->val[2][3] = z;
	mat_o->val[3][0] = 0; mat_o->val[3][1] = 0; mat_o->val[3][2] = 0; mat_o->val[3][3] = 1;
	return mat_o;
}
//缩放
static inline GRE_FMat4x4 YMGRE_FMat4x4_Sacle_Cal(float32 x, float32 y, float32 z)
{
	GRE_FMat4x4 mat_o = YMGRE_Creat_FMAT4x4();
	//缩放矩阵
	mat_o->val[0][0] = x; mat_o->val[0][1] = 0; mat_o->val[0][2] = 0; mat_o->val[0][3] = 0;
	mat_o->val[1][0] = 0; mat_o->val[1][1] = y; mat_o->val[1][2] = 0; mat_o->val[1][3] = 0;
	mat_o->val[2][0] = 0; mat_o->val[2][1] = 0; mat_o->val[2][2] = z; mat_o->val[2][3] = 0;
	mat_o->val[3][0] = 0; mat_o->val[3][1] = 0; mat_o->val[3][2] = 0; mat_o->val[3][3] = 1;
	
	return mat_o;
}
//旋转
//     ^ y
//     |
//      ---> x
//   / z 
//  
//  右手坐标系，以顺时针为正方向，参考：https://blog.csdn.net/ahelloyou/article/details/108903506

static inline GRE_FMat4x4 YMGRE_FMat4x4_RotateX_Cal(float32 xAngle)
{
	GRE_FMat4x4 mat_o = YMGRE_Creat_FMAT4x4();
	float32 s = YMGRE_Sin(xAngle * YMGRE_Deg2Rad);
	float32 c = YMGRE_Cos(xAngle * YMGRE_Deg2Rad);
	float32 n = -s;
	//绕x轴旋转矩阵
	mat_o->val[0][0] = 1; mat_o->val[0][1] = 0; mat_o->val[0][2] = 0; mat_o->val[0][3] = 0;
	mat_o->val[1][0] = 0; mat_o->val[1][1] = c; mat_o->val[1][2] = n; mat_o->val[1][3] = 0;
	mat_o->val[2][0] = 0; mat_o->val[2][1] = s; mat_o->val[2][2] = c; mat_o->val[2][3] = 0;
	mat_o->val[3][0] = 0; mat_o->val[3][1] = 0; mat_o->val[3][2] = 0; mat_o->val[3][3] = 1;

	return mat_o;
}
static inline GRE_FMat4x4 YMGRE_FMat4x4_RotateY_Cal(float32 yAngle)
{
	GRE_FMat4x4 mat_o = YMGRE_Creat_FMAT4x4();
	float32 s = YMGRE_Sin(yAngle * YMGRE_Deg2Rad);
	float32 c = YMGRE_Cos(yAngle * YMGRE_Deg2Rad);
	float32 n = -s;
	//绕y轴旋转矩阵
	mat_o->val[0][0] = c; mat_o->val[0][1] = 0; mat_o->val[0][2] = s; mat_o->val[0][3] = 0;
	mat_o->val[1][0] = 0; mat_o->val[1][1] = 1; mat_o->val[1][2] = 0; mat_o->val[1][3] = 0;
	mat_o->val[2][0] = n; mat_o->val[2][1] = 0; mat_o->val[2][2] = c; mat_o->val[2][3] = 0;
	mat_o->val[3][0] = 0; mat_o->val[3][1] = 0; mat_o->val[3][2] = 0; mat_o->val[3][3] = 1;

	return mat_o;
}
static inline GRE_FMat4x4 YMGRE_FMat4x4_RotateZ_Cal(float32 zAngle)
{
	GRE_FMat4x4 mat_o = YMGRE_Creat_FMAT4x4();
	float32 s = YMGRE_Sin(zAngle * YMGRE_Deg2Rad);
	float32 c = YMGRE_Cos(zAngle * YMGRE_Deg2Rad);
	float32 n = -s;
	//绕z轴旋转矩阵
	mat_o->val[0][0] = c; mat_o->val[0][1] = n; mat_o->val[0][2] = 0; mat_o->val[0][3] = 0;
	mat_o->val[1][0] = s; mat_o->val[1][1] = c; mat_o->val[1][2] = 0; mat_o->val[1][3] = 0;
	mat_o->val[2][0] = 0; mat_o->val[2][1] = 0; mat_o->val[2][2] = 1; mat_o->val[2][3] = 0;
	mat_o->val[3][0] = 0; mat_o->val[3][1] = 0; mat_o->val[3][2] = 0; mat_o->val[3][3] = 1;

	return mat_o;
}
//以单位法向量为旋转轴
//参考：https://zhuanlan.zhihu.com/p/56587491
static inline GRE_FMat4x4 YMGRE_FMat4x4_Rotate_Cal(const GRE_Fvector4d NVec,float32 zAngle)
{
	gre_log_explain(NVec == NULL, GRE_LOG_PtrI, "输入的单位法向量不存在");

	GRE_FMat4x4 mat_o = YMGRE_Creat_FMAT4x4();
	float32 s = YMGRE_Sin(zAngle * YMGRE_Deg2Rad);
	float32 c = YMGRE_Cos(zAngle * YMGRE_Deg2Rad);
	float32 n = 1.0f-c;
	
	YMGRE_Fvector4d_Normalize(NVec);//归一化为单位法向量
	float32 x = NVec->x;
	float32 y = NVec->y;
	float32 z = NVec->z;

	//绕单位法向量旋转矩阵
	mat_o->val[0][0] = x * x * n + c;     mat_o->val[0][1] = x * y * n + z * s; mat_o->val[0][2] = x * z * n - y * s; mat_o->val[0][3] = 0;
	mat_o->val[1][0] = x * y * n - z * s; mat_o->val[1][1] = y * y * n + c;     mat_o->val[1][2] = y * z * n + x * s; mat_o->val[1][3] = 0;
	mat_o->val[2][0] = x * z * n + y * s; mat_o->val[2][1] = y * z * n - x * s; mat_o->val[2][2] = z * z * n + c;     mat_o->val[2][3] = 0;
	mat_o->val[3][0] = 0; mat_o->val[3][1] = 0; mat_o->val[3][2] = 0; mat_o->val[3][3] = 1;

	return mat_o;
}

//矩阵 * 向量减法 
static inline GRE_Fvector4d YMGRE_Fvector4d_MatMult(GRE_FMat4x4 mat0, GRE_Fvector4d vec1)
{
	gre_log_explain((mat0 == NULL)||(vec1==NULL), GRE_LOG_PtrI, "输入的矩阵或向量不存在");
	GRE_Fvector4d vout = YMGRE_Creat_VectorF4d();

	vout->x = mat0->val[0][0] * vec1->x + mat0->val[0][1] * vec1->y + mat0->val[0][2] * vec1->z + mat0->val[0][3] * vec1->w;
	vout->y = mat0->val[1][0] * vec1->x + mat0->val[1][1] * vec1->y + mat0->val[1][2] * vec1->z + mat0->val[1][3] * vec1->w;
	vout->z = mat0->val[2][0] * vec1->x + mat0->val[2][1] * vec1->y + mat0->val[2][2] * vec1->z + mat0->val[2][3] * vec1->w;
	vout->w = mat0->val[0][0] * vec1->x + mat0->val[0][1] * vec1->y + mat0->val[0][2] * vec1->z + mat0->val[3][3] * vec1->w;
	return vout;
}

static inline void YMGRE_Fvector4d_MatMultTo(GRE_FMat4x4 mat0, GRE_Fvector4d vec1, GRE_Fvector4d result)
{
	gre_log_explain((mat0 == NULL) || (vec1 == NULL) || (result == NULL), GRE_LOG_PtrI, "输入的矩阵或向量不存在");
	float32 x = mat0->val[0][0] * vec1->x + mat0->val[0][1] * vec1->y + mat0->val[0][2] * vec1->z + mat0->val[0][3] * vec1->w;
	float32 y = mat0->val[1][0] * vec1->x + mat0->val[1][1] * vec1->y + mat0->val[1][2] * vec1->z + mat0->val[1][3] * vec1->w;
	float32 z = mat0->val[2][0] * vec1->x + mat0->val[2][1] * vec1->y + mat0->val[2][2] * vec1->z + mat0->val[2][3] * vec1->w;
	float32 w = mat0->val[3][0] * vec1->x + mat0->val[3][1] * vec1->y + mat0->val[3][2] * vec1->z + mat0->val[3][3] * vec1->w;

	result->x = x;
	result->y = y;
	result->z = z;
	result->w = w;
}
#endif // !YMGRE_MATHBASE_H
