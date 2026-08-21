#ifndef YMGRE_LIGHT_H
#define YMGRE_LIGHT_H
#include "../OPOBJ/YMGRE_OBJ.h"
#include "../CONFIG/YMGRE_PubDefine.h"
#include "./YMGRE_List.h"



//三角形平面光照
static inline void YMGRE_PolygonLighting_Color(GRE_Polygon4d thispoly, GRE_Fvector4d planeVetex0, GRE_Fvector4d pN, GRE_Light4d thislight, GRERGB24 outColor,float32 mirror_ks,uint8 high_n)
{
	GRERGB24 lightI = &thislight->proper.lightcolor;
	int r = 0, g = 0, b = 0;
	switch (thislight->type)
	{
	case GRE_GlobalLight://全局光照
	{
		// I = Ia*ka
		r =((lightI->R * (uint32)thispoly->planeColor.R) >> 8);
		g =((lightI->G * (uint32)thispoly->planeColor.G) >> 8);
		b =((lightI->B * (uint32)thispoly->planeColor.B) >> 8);
		break;
	}
	case GRE_SpotLight://聚光灯 实现参考：https://zhuanlan.zhihu.com/p/149774959
	{				
		// 无穷远灯光, 因此需要知道面发现和光源方向
		//   L \ | N
		//      \|
		//   --------
		// 这里使用灯光方向的逆方向作为与面发现夹角的向量, 这样当夹角小于90时, 其点积大于零
		float32 dotval = -YMGRE_Fvector4d_Dot(&thislight->proper.spot.direct, pN); // L*N

		if (dotval > 0.0f)
		{
			float32 Llen = YMGRE_Fvector4d_Len1(&thislight->proper.spot.direct);// |L|
			float32 Nlen = YMGRE_Fvector4d_Len1(pN);// |N|

			//夹角衰减公式 atten = ( (cos(θ) - cos(θo))/(cos(θ)i - cos(θo)) )^2
			GRE_Fvector4d Dvec = YMGRE_Fvector4d_Sub(&thislight->proper.pos_, planeVetex0);//Dvec = light - plane
			float32 Dlen = YMGRE_Fvector4d_Len1(Dvec);// |D|
			float32 cs_theta_ = YMGRE_Fvector4d_Dot(&thislight->proper.spot.direct, Dvec)/(Llen * Dlen); //cos(θ)
			cs_theta_ -= thislight->proper.spot.cs_outer_angle;
			float32 atten_k = cs_theta_* cs_theta_ / thislight->proper.spot.cs_div_;

			// 聚光灯光照模型
			// I(d)point  = IOdir * Cldir * atten * bdrf
			float32 temp = dotval* thislight->proper.strength / (Llen * Nlen * 255);
			temp *= atten_k* (1.0f/ YMGRE_Pai);
			// 接收光照的强度, 多边形法线与光照方向的夹角越小, 那么其接收强度越大
			// 夹角越大, 接收强度越小, 多边形越暗
			r = ((uint32)(lightI->R * thispoly->planeColor.R * temp));
			g = ((uint32)(lightI->G * thispoly->planeColor.G * temp));
			b = ((uint32)(lightI->B * thispoly->planeColor.B * temp));
		}
		//启用背面阴影模拟
		else if (thislight->proper.shadowK > 0.0f)
		{
			// 这里当多边形是背朝光源时, 也进行了一些处理, 只是把它的颜色相对调暗，用于模拟背部阴影
			r += lightI->R * thislight->proper.shadowK;
			g += lightI->G * thislight->proper.shadowK;
			b += lightI->B * thislight->proper.shadowK;
		}
		break;
	}
	case GRE_PointLight://点光源
	{
		GRE_Fvector4d Lvec = YMGRE_Fvector4d_Sub(&thislight->proper.pos_, planeVetex0);//L = light - plane
		//   L \ | N
		//      \|
		//   --------
		float32 dotval = YMGRE_Fvector4d_Dot(Lvec, pN); // L*N

		//光不在背面
		if (dotval > 0.0f)
		{
			float32 Llen = YMGRE_Fvector4d_Len1(Lvec);// |L|
			float32 Nlen = YMGRE_Fvector4d_Len1(pN);// |N|
			// 点光源的光照模型
			//					IOpoint * Clpoint
			// I(d)point = --------------------------
			//				kc + kl * d + kq * d * d
			// 其中d = |p-s| 即点光源到多边形的距离
			float32 atten_k =1.0f/( thislight->proper.kc0 + thislight->proper.kc1* Llen + thislight->proper.kc2* Llen* Llen);//衰减系数 1/(c0 + c1*d + c2*d^2)
			float32 cos_k = dotval / (Llen * Nlen * 255);

			float32 light_r = lightI->R * atten_k;
			float32 light_g = lightI->G * atten_k;
			float32 light_b = lightI->B * atten_k;
			//计算带衰减点光照 Id = Ip * Kp*cos(θ) * （1/atten_k）
			r = ((uint32)(light_r * thispoly->planeColor.R * cos_k));
			g = ((uint32)(light_g * thispoly->planeColor.G * cos_k));
			b = ((uint32)(light_b * thispoly->planeColor.B * cos_k));

			//高光部分计算：
			if (mirror_ks > 0.0f)
			{
				//归一化 L
				YMGRE_Fvector4d_ScaleTo(Lvec, 1.0f / Llen);
				//由于变换后视点为0点，所以 V = 0 - plane
				gre_fvector4d View;
				View.x = -planeVetex0->x;
				View.y = -planeVetex0->y;
				View.z = -planeVetex0->z;
				View.w = -planeVetex0->w;
				YMGRE_Fvector4d_Normalize(&View);//归一化 V
				YMGRE_Fvector4d_AddTo(&View, Lvec);//计算 L+V
				YMGRE_Fvector4d_Normalize(&View);// 归一化： L+V / |L+V|

				dotval = YMGRE_Fvector4d_Dot(&View, pN) / Nlen; // H*N ,此处对N进行归一化
				dotval = GREMax(dotval, 0.0f); // β半角>90时，会出现负值
				dotval = YMGRE_Pow(dotval, high_n);
				//加入高光部分： Is = Is * Ks * (H *N)^n
				r += ((uint32)(light_r * mirror_ks * dotval));
				g += ((uint32)(light_g * mirror_ks * dotval));
				b += ((uint32)(light_b * mirror_ks * dotval));
			}	
		} 
		//启用背面阴影模拟
		else if(thislight->proper.shadowK > 0.0f)
		{

			float32 Llen = YMGRE_Fvector4d_Len1(Lvec);// |L|
			float32 Nlen = YMGRE_Fvector4d_Len1(pN);// |N|
			// 点光源的光照模型
			//					IOpoint * Clpoint
			// I(d)point = --------------------------
			//				kc + kl * d + kq * d * d
			// 其中d = |p-s| 即点光源到多边形的距离
			float32 atten_k = 1.0f / (thislight->proper.kc0 + thislight->proper.kc1 * Llen + thislight->proper.kc2 * Llen * Llen);//衰减系数 1/(c0 + c1*d + c2*d^2)
			float32 cos_k = dotval / (Llen * Nlen * 255);

			float32 light_r = lightI->R * atten_k;
			float32 light_g = lightI->G * atten_k;
			float32 light_b = lightI->B * atten_k;

			// 这里当多边形是背朝光源时, 也进行了一些处理, 只是把它的颜色相对调暗，用于模拟背部阴影
			r += light_r * thislight->proper.shadowK;
			g += light_g * thislight->proper.shadowK;
			b += light_b * thislight->proper.shadowK;
		}
		//释放内存
		YMGRE_Free_VectorF4d(Lvec);
		break;
	}
	default:
		break;
	}
	//在原来的基础上增加
	r += outColor->R;
	g += outColor->G;
	b += outColor->B;

	//限制幅度
	outColor->R = GREMin(r, 255);
	outColor->G = GREMin(g, 255);
	outColor->B = GREMin(b, 255);
}

//物体光照
static inline void YMGRE_ObjectLighting_Color(GRE_Object4d myobj, GRE_List LightList,GRE_Camera4d mycam)
{
	//GRErgb24 
	for (int i = 0; i < myobj->polygonNum; i++)
	{
		GRE_Polygon4d thispoly = &myobj->polygonList[i];
		//计算平面法向量pN
		// 0   3
		// 1   2
		GRE_Vertex4d tpoints = myobj->pointList_; //取透视变换后的点
		int i1 = thispoly->index[0];
		int i2 = thispoly->index[1];
		int i3 = thispoly->index[2];

		// 取 平面上的 两个向量
		GRE_Fvector4d u = YMGRE_Fvector4d_Sub(&tpoints[i2].pos, &tpoints[i1].pos);
		GRE_Fvector4d v = YMGRE_Fvector4d_Sub(&tpoints[i3].pos, &tpoints[i1].pos);
		// 由于相机是左手坐标系，所以平面法实际向量与所求刚好相反
		GRE_Fvector4d pN = YMGRE_Fvector4d_Cross(v, u);//计算法向量 n = v×u = -u×v

		GRErgb24 planeColor = { 0 };
		//遍历光源
		for (GRE_ListNode curLightlist = LightList->listhead; curLightlist != NULL; curLightlist = curLightlist->next)
		{
			GRE_Light4d thisLight = curLightlist->data;
			YMGRE_PolygonLighting_Color(thispoly, &tpoints[0].pos, pN, thisLight, &planeColor, myobj->mirrorKs, 30);//平面光照计算
		}
		thispoly->planeColor_.R = planeColor.R;
		thispoly->planeColor_.G = planeColor.G;
		thispoly->planeColor_.B = planeColor.B;
		//释放内存
		YMGRE_Free_VectorF4d(u);
		YMGRE_Free_VectorF4d(v);
		YMGRE_Free_VectorF4d(pN);
	}
}


#endif // !YMGRE_LIGHT_H

