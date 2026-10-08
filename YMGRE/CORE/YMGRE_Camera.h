#ifndef YMGRE_CAMERA_H
#define YMGRE_CAMERA_H

#include "../OPOBJ/YMGRE_OBJ.h"
#include "./YMGRE_MathBase.h"
#include "../CONFIG/YMGRE_Profile.h"
#include "../OPOBJ/YMGRE_Free.h"

// uvn相机位置初始化：位置，注视点，自旋角，up向量(V)
static inline void YMGRE_UVNCamera_PositionInit(GRE_Camera4d myCam, GRE_Fvector4d posPoint, GRE_Fvector4d regardPoint, GRE_Fvector4d Vreference, float32 theta)
{
	//当前位置
	YMGRE_Fvector4d_AssTo(&myCam->pos, posPoint);
	//相机自旋角
	myCam->move.theta = theta;
	//相机注视点
	YMGRE_Fvector4d_AssTo(&myCam->traget, regardPoint);

	//根据上述定义计算其他参数
	gre_fvector4d viewN;
	YMGRE_Fvector4d_SubToResult(&myCam->traget, &myCam->pos, &viewN); //法向量
	//位置与目标重合时仍给出可用朝向，避免后续相机基向量退化。
	if (YMGRE_Fvector4d_Len2(&viewN) < 1e-12f)
		viewN = (gre_fvector4d){ 0, 0, 1, 0 };
	YMGRE_Fvector4d_Normalize(&viewN);
	//默认设置 up 向量朝上，这样设置如果横跨物体，则会产生问题，
	//如在x=0,z=0，N向量与默认的up向量共线，该处为畸点
	gre_fvector4d view_up;
	if (Vreference == NULL)
	{
		view_up.x = 0;
		view_up.y = 1;
		view_up.z = 0;
	}
	//使用外部的参考方向
	else
	{
		view_up.x = Vreference->x;
		view_up.y = Vreference->y;
		view_up.z = Vreference->z;
	}
	view_up.w = 0;
	//默认 up 与视线平行时改用 Z 轴，保证叉乘能形成正交基。
	gre_fvector4d viewU;
	YMGRE_Fvector4d_CrossToResult(&viewN, &view_up, &viewU);// N×v =U ： right向量
	if (YMGRE_Fvector4d_Len2(&viewU) < 1e-12f)
	{
		view_up = (gre_fvector4d){ 0, 0, 1, 0 };
		YMGRE_Fvector4d_CrossToResult(&viewN, &view_up, &viewU);
		if (YMGRE_Fvector4d_Len2(&viewU) < 1e-12f)
		{
			view_up = (gre_fvector4d){ 1, 0, 0, 0 };
			YMGRE_Fvector4d_CrossToResult(&viewN, &view_up, &viewU);
		}
	}
	//计算U V 平面 ,右手定则
	gre_fvector4d viewV;
	YMGRE_Fvector4d_CrossToResult(&viewU, &viewN, &viewV);//N × U = V : up向量
	//归一化向量
	YMGRE_Fvector4d_Normalize(&viewU);
	YMGRE_Fvector4d_Normalize(&viewV);
	//UV绕 N轴 旋转theta度
	gre_fmat4x4 camRotate;
	YMGRE_FMat4x4_RotateTo(&viewN, theta, &camRotate);//必须是绕单位轴旋转
	YMGRE_Fvector4d_MatMultTo(&camRotate, &viewU, &viewU);
	YMGRE_Fvector4d_MatMultTo(&camRotate, &viewV, &viewV);
	//运动接口和变换矩阵必须共享同一组相机基向量。
	myCam->move.cu = viewU;
	myCam->move.cv = viewV;
	myCam->move.cn = viewN;

	//计算相机变换逆矩阵
	gre_fvector4d mov; //平移向量，将相机平移到原点
	mov.x = -myCam->pos.x;
	mov.y = -myCam->pos.y;
	mov.z = -myCam->pos.z;
	mov.w = 0;
	//对相机进行旋转
	myCam->move.TMat.val[0][0] = viewU.x; myCam->move.TMat.val[0][1] = viewU.y; myCam->move.TMat.val[0][2] = viewU.z; myCam->move.TMat.val[0][3] = YMGRE_Fvector4d_Dot(&mov, &viewU);
	myCam->move.TMat.val[1][0] = viewV.x; myCam->move.TMat.val[1][1] = viewV.y; myCam->move.TMat.val[1][2] = viewV.z; myCam->move.TMat.val[1][3] = YMGRE_Fvector4d_Dot(&mov, &viewV);
	myCam->move.TMat.val[2][0] = viewN.x; myCam->move.TMat.val[2][1] = viewN.y; myCam->move.TMat.val[2][2] = viewN.z; myCam->move.TMat.val[2][3] = YMGRE_Fvector4d_Dot(&mov, &viewN);
	myCam->move.TMat.val[3][0] = 0; myCam->move.TMat.val[3][1] = 0; myCam->move.TMat.val[3][2] = 0; myCam->move.TMat.val[3][3] = 1;
}

//相机初始化：背景色，相机采集图像
static inline void YMGRE_CameraImage_Init(GRE_Camera4d myCam,GRErgb24 background)
{
#if YMGRE_PROFILE_RENDER_STAGES
 uint32 start=YMGRE_ProfileNow();
#endif
	GRE_RenderTarget target = (myCam->target != NULL) ? myCam->target : &myCam->img;
	float32 zfar_val= myCam->frustum.Zfar;
	GRE_FramePixel clearPixel = GRE_FramePixel_From_RGB24(background);
	size_t count = (size_t)target->width * target->height;
	/* Keep depth and color writes sequential on external-memory targets. */
	for (size_t i = 0; i < count; i++) target->zbuff[i] = zfar_val;
 if(myCam->backgroundRow) {
  for(uint16 y=0;y<target->height;y++) {
   GRE_FramePixel pixel=myCam->backgroundRow(y,target->height,myCam->backgroundUser);
   GRE_FrameBuffer row=target->data+(size_t)y*target->width;
   for(uint16 x=0;x<target->width;x++)row[x]=pixel;
  }
 } else for (size_t i = 0; i < count; i++) target->data[i] = clearPixel;
#if YMGRE_PROFILE_RENDER_STAGES
 YMGRE_ProfileCycles[0]+=YMGRE_ProfileNow()-start;
#endif
}

//视景体初始化：远近平面
static inline void YMGRE_Camera_Frustum_Init(GRE_Camera4d mycam, float32 znear, float32 zfar)
{
	//视景体的远近截平面初始化
	mycam->frustum.Zfar = zfar;
	mycam->frustum.Znear = znear;

	////初始化左右面,取近景平面作为参考
	////           |    
	////      - - -|    /
	////       \   |   /-#
	////      #-\  |  /
	////         \_| z
	////          x          tan(θ) = x/z   其中   x=w/2 ，可得 x(t) = z(t)*tan(θ)
	//mycam->frustum.Xleft = znear * mycam->perspectPlane.kl; // pL是负数
	//mycam->frustum.Xright = znear * mycam->perspectPlane.kr;
	////上下面,原理同上
	//mycam->frustum.Yup = znear * mycam->perspectPlane.ku;
	//mycam->frustum.Ydown = znear * mycam->perspectPlane.kd;//pD是负数
}

//相机移动
static inline void YMGRE_Camera_Move(GRE_Camera4d myCam, GRE_Fvector4d movVec)
{
	// P += V
	YMGRE_Fvector4d_AddTo(&myCam->pos, movVec);
	YMGRE_Fvector4d_AddTo(&myCam->traget, movVec);
	//需要进行更新
}

//相机yaw旋转 绕V轴
static inline void YMGRE_Camera_RotateYaw(GRE_Camera4d myCam, float32 degree)
{
	//// P += V
	//YMGRE_Fvector4d_AddTo(&myCam->pos, movVec);
	//YMGRE_Fvector4d_AddTo(&myCam->traget, movVec);
	//需要进行更新
}
//相机pitch旋转 绕U轴


#endif // !YMGRE_CAMERA_H
