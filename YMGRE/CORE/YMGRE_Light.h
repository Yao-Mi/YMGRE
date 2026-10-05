#ifndef YMGRE_LIGHT_H
#define YMGRE_LIGHT_H
#include "../OPOBJ/YMGRE_OBJ.h"
#include "../CONFIG/YMGRE_PubDefine.h"
#include "./YMGRE_List.h"

// 视觉等效补偿：输入聚光灯半锥角（度），返回相对于全空间点光源的能量倍率。
// 半锥角 180° 覆盖整个球面，倍率为 1；该函数不自动应用到核心光照计算。
static inline float32 YMGRE_Spot_EquivalentPointLightGain(float32 halfConeAngleDeg)
{
//   半径为 r 的完整球面面积：
//   A_sphere = 4πr²
//   半锥角为 θ 的球面区域是球冠，面积：
//   A_cone = 2πr²(1 - cosθ)

//   所以补偿系数：
//   gain = A_sphere / A_cone
//        = 2 / (1 - cosθ)

	float32 cosAngle = YMGRE_Cos(halfConeAngleDeg * YMGRE_Deg2Rad);
	float32 denominator = GREMax(1.0f - cosAngle, 1e-4f);
	return 2.0f / denominator;
}



/* Shininess is an integer; avoid the general logarithm/exponential path. */
static inline float32 YMGRE_Light_IntegerPower(float32 x, uint8 exponent)
{
	float32 result = 1.0f;
	while (exponent) {
		if (exponent & 1) result *= x;
		exponent >>= 1;
		if (exponent) x *= x;
	}
	return result;
}

//三角形平面光照
static inline void YMGRE_PolygonLighting_ComponentsAdvanced(GRE_Polygon4d thispoly,
	GRE_Fvector4d planeVetex0, GRE_Fvector4d pN, GRE_Light4d thislight,
	GRERGB24 outBaseColor, GRERGB24 outSpecularColor, float32 mirror_ks,
	uint8 high_n, GRErgb24 materialSpecular)
{
	GRERGB24 lightI = &thislight->proper.lightcolor;
	int r = 0, g = 0, b = 0;
	int sr = 0, sg = 0, sb = 0;
	switch (thislight->type)
	{
	case GRE_GlobalLight://全局光照
	{
		// I = Ia*ka
		float32 strength = GREMax(thislight->proper.strength, 0.0f);
		r = (int)(lightI->R * thispoly->planeColor.R * strength / 255.0f);
		g = (int)(lightI->G * thispoly->planeColor.G * strength / 255.0f);
		b = (int)(lightI->B * thispoly->planeColor.B * strength / 255.0f);
		break;
	}
	case GRE_SpotLight://聚光灯 实现参考：https://zhuanlan.zhihu.com/p/149774959
	{
		gre_fvector4d Dvec;
		YMGRE_Fvector4d_SubToResult(&thislight->proper.pos_, planeVetex0, &Dvec);
		float32 Dlen = YMGRE_Fvector4d_Len1(&Dvec);
		// 无穷远灯光, 因此需要知道面发现和光源方向
		//   L \ | N
		//      \|
		//   --------
		// 这里使用灯光方向的逆方向作为与面发现夹角的向量, 这样当夹角小于90时, 其点积大于零
		float32 dotval = YMGRE_Fvector4d_Dot(&Dvec, pN); // 受光点指向光源与法线的点积

		if (dotval > 0.0f && Dlen > 1e-6f)
		{
			float32 Llen = YMGRE_Fvector4d_Len1(&thislight->proper.spot.direct);// |L|
			float32 Nlen = YMGRE_Fvector4d_Len1(pN);// |N|
			if (Llen <= 1e-6f || Nlen <= 1e-6f) break;

			//夹角衰减公式 atten = ( (cos(θ) - cos(θo))/(cos(θ)i - cos(θo)) )^2
			// 使用项目现有 kc0/kc1/kc2 标定模型，避免世界坐标距离改变强度语义。
			float32 denominator = thislight->proper.kc0 + thislight->proper.kc1 * Dlen +
				thislight->proper.kc2 * Dlen * Dlen;
			float32 distanceAttenuation = denominator > 1e-6f ? 1.0f / denominator : 0.0f;
			// Dvec 指向“光源减去受光点”，与灯光照射方向相反。
			float32 cs_theta_ = -YMGRE_Fvector4d_Dot(&thislight->proper.spot.direct, &Dvec)/(Llen * Dlen);
			float32 atten_k = 0.0f;
			if (cs_theta_ >= thislight->proper.spot.cs_outer_angle)
			{
				float32 t = (cs_theta_ - thislight->proper.spot.cs_outer_angle) *
					thislight->proper.spot.cs_div_;
				t = GREMax(0.0f, GREMin(t, 1.0f));
				atten_k = t * t;
			}

			// 聚光灯光照模型
			// I(d)point  = IOdir * Cldir * atten * bdrf
			float32 temp = dotval * thislight->proper.strength * atten_k *
				distanceAttenuation / (Dlen * Nlen * 255);
			// 接收光照的强度, 多边形法线与光照方向的夹角越小, 那么其接收强度越大
			// 夹角越大, 接收强度越小, 多边形越暗
			r = ((uint32)(lightI->R * thispoly->planeColor.R * temp));
			g = ((uint32)(lightI->G * thispoly->planeColor.G * temp));
			b = ((uint32)(lightI->B * thispoly->planeColor.B * temp));

			// 聚光灯高光沿用点光源的 Blinn-Phong 计算，
			// 并使用同一距离/锥角衰减，保证高光位置与点光源一致。
			if (mirror_ks > 0.0f && Dlen > 1e-6f)
			{
				gre_fvector4d L = Dvec;
				YMGRE_Fvector4d_ScaleTo(&L, 1.0f / Dlen);
				gre_fvector4d View;
				View.x = -planeVetex0->x;
				View.y = -planeVetex0->y;
				View.z = -planeVetex0->z;
				View.w = -planeVetex0->w;
				YMGRE_Fvector4d_Normalize(&View);
				YMGRE_Fvector4d_AddTo(&View, &L);
				YMGRE_Fvector4d_Normalize(&View);

				float32 specular = YMGRE_Fvector4d_Dot(&View, pN) / Nlen;
				specular = GREMax(specular, 0.0f);
				specular = YMGRE_Light_IntegerPower(specular, high_n);
				// 高光还需受入射角约束，避免仅凭 N·H 在整个光锥内铺开。
				float32 noLight = dotval / (Dlen * Nlen);
				noLight = GREMax(noLight, 0.0f);
				specular *= noLight;
				float32 specularScale = thislight->proper.strength *
					distanceAttenuation * atten_k;
				sr += (uint32)(lightI->R * materialSpecular.R / 255.0f *
					specularScale * mirror_ks * specular);
				sg += (uint32)(lightI->G * materialSpecular.G / 255.0f *
					specularScale * mirror_ks * specular);
				sb += (uint32)(lightI->B * materialSpecular.B / 255.0f *
					specularScale * mirror_ks * specular);
			}
		}
		//启用背面阴影模拟
		else if (thislight->proper.shadowK > 0.0f)
		{
			// 背面保留少量环境反弹色：沿用点光源的 shadowK，
			// 但仍经过材质颜色、距离衰减和聚光锥衰减，避免阴影面被补成亮色。
			float32 spotShadow = 0.0f;
			float32 Llen = YMGRE_Fvector4d_Len1(&thislight->proper.spot.direct);
			if (Llen > 1e-6f && Dlen > 1e-6f)
			{
				float32 csTheta = -YMGRE_Fvector4d_Dot(&thislight->proper.spot.direct, &Dvec) /
					(Llen * Dlen);
				if (csTheta > thislight->proper.spot.cs_outer_angle)
				{
					float32 t = (csTheta - thislight->proper.spot.cs_outer_angle) *
						thislight->proper.spot.cs_div_;
					t = GREMax(0.0f, GREMin(t, 1.0f));
					spotShadow = t * t;
				}
			}
			float32 denominator = thislight->proper.kc0 + thislight->proper.kc1 * Dlen +
				thislight->proper.kc2 * Dlen * Dlen;
			float32 distanceAttenuation = denominator > 1e-6f ? 1.0f / denominator : 0.0f;
			float32 shadow = thislight->proper.shadowK * spotShadow * distanceAttenuation / 255.0f;
			r += (uint32)(lightI->R * thispoly->planeColor.R * shadow);
			g += (uint32)(lightI->G * thispoly->planeColor.G * shadow);
			b += (uint32)(lightI->B * thispoly->planeColor.B * shadow);
		}
		break;
	}
	case GRE_PointLight://点光源
	{
		gre_fvector4d Lvec;
		YMGRE_Fvector4d_SubToResult(&thislight->proper.pos_, planeVetex0, &Lvec);//L = light - plane
		//   L \ | N
		//      \|
		//   --------
		float32 dotval = YMGRE_Fvector4d_Dot(&Lvec, pN); // L*N

		//光不在背面
		if (dotval > 0.0f)
		{
			float32 Llen = YMGRE_Fvector4d_Len1(&Lvec);// |L|
			float32 Nlen = YMGRE_Fvector4d_Len1(pN);// |N|
			if (Llen <= 1e-6f || Nlen <= 1e-6f) break;
			// 点光源的光照模型
			//					IOpoint * Clpoint
			// I(d)point = --------------------------
			//				kc + kl * d + kq * d * d
			// 其中d = |p-s| 即点光源到多边形的距离
			float32 denominator = thislight->proper.kc0 + thislight->proper.kc1 * Llen + thislight->proper.kc2 * Llen * Llen;
			float32 atten_k = denominator > 1e-6f ? 1.0f / denominator : 0.0f;//衰减系数 1/(c0 + c1*d + c2*d^2)
			float32 cos_k = dotval / (Llen * Nlen * 255);

			atten_k *= thislight->proper.strength;
			// 光强是光源辐射功率缩放，漫反射和高光必须共用同一缩放。
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
				YMGRE_Fvector4d_ScaleTo(&Lvec, 1.0f / Llen);
				//由于变换后视点为0点，所以 V = 0 - plane
				gre_fvector4d View;
				View.x = -planeVetex0->x;
				View.y = -planeVetex0->y;
				View.z = -planeVetex0->z;
				View.w = -planeVetex0->w;
				YMGRE_Fvector4d_Normalize(&View);//归一化 V
				YMGRE_Fvector4d_AddTo(&View, &Lvec);//计算 L+V
				YMGRE_Fvector4d_Normalize(&View);// 归一化： L+V / |L+V|

				dotval = YMGRE_Fvector4d_Dot(&View, pN) / Nlen; // H*N ,此处对N进行归一化
				dotval = GREMax(dotval, 0.0f); // β半角>90时，会出现负值
				dotval = YMGRE_Light_IntegerPower(dotval, high_n);
				//加入高光部分： Is = Is * Ks * (H *N)^n
				sr += ((uint32)(light_r * materialSpecular.R / 255.0f * mirror_ks * dotval));
				sg += ((uint32)(light_g * materialSpecular.G / 255.0f * mirror_ks * dotval));
				sb += ((uint32)(light_b * materialSpecular.B / 255.0f * mirror_ks * dotval));
			}
		}
		//启用背面阴影模拟
		else if(thislight->proper.shadowK > 0.0f)
		{
			float32 Llen = YMGRE_Fvector4d_Len1(&Lvec);// |L|
			float32 denominator = thislight->proper.kc0 + thislight->proper.kc1 * Llen +
				thislight->proper.kc2 * Llen * Llen;
			float32 atten_k = denominator > 1e-6f ? 1.0f / denominator : 0.0f;
			float32 light_r = lightI->R * atten_k;
			float32 light_g = lightI->G * atten_k;
			float32 light_b = lightI->B * atten_k;

			// 背面补光仍属于漫反射响应，必须保留材质颜色。
			r += (uint32)(light_r * thispoly->planeColor.R * thislight->proper.shadowK / 255.0f);
			g += (uint32)(light_g * thispoly->planeColor.G * thislight->proper.shadowK / 255.0f);
			b += (uint32)(light_b * thispoly->planeColor.B * thislight->proper.shadowK / 255.0f);
		}
		break;
	}
	default:
		break;
	}
	//在原来的基础上增加
	r += outBaseColor->R;
	g += outBaseColor->G;
	b += outBaseColor->B;

	//限制幅度
	outBaseColor->R = GREMin(r, 255);
	outBaseColor->G = GREMin(g, 255);
	outBaseColor->B = GREMin(b, 255);
	if (outSpecularColor != NULL)
	{
		outSpecularColor->R = GREMin(outSpecularColor->R + sr, 255);
		outSpecularColor->G = GREMin(outSpecularColor->G + sg, 255);
		outSpecularColor->B = GREMin(outSpecularColor->B + sb, 255);
	}
	else
	{
		outBaseColor->R = GREMin(outBaseColor->R + sr, 255);
		outBaseColor->G = GREMin(outBaseColor->G + sg, 255);
		outBaseColor->B = GREMin(outBaseColor->B + sb, 255);
	}
}

static inline void YMGRE_PolygonLighting_ColorAdvanced(GRE_Polygon4d thispoly,
	GRE_Fvector4d planeVetex0, GRE_Fvector4d pN, GRE_Light4d thislight,
	GRERGB24 outColor, float32 mirror_ks, uint8 high_n, GRErgb24 specularColor)
{
	YMGRE_PolygonLighting_ComponentsAdvanced(thispoly, planeVetex0, pN,
		thislight, outColor, NULL, mirror_ks, high_n, specularColor);
}

static inline void YMGRE_PolygonLighting_Color(GRE_Polygon4d thispoly,
	GRE_Fvector4d planeVetex0, GRE_Fvector4d pN, GRE_Light4d thislight,
	GRERGB24 outColor, float32 mirror_ks, uint8 high_n)
{
	YMGRE_PolygonLighting_ColorAdvanced(thispoly, planeVetex0, pN, thislight,
		outColor, mirror_ks, high_n, (GRErgb24){ 255, 255, 255 });
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
		gre_fvector4d u;
		gre_fvector4d v;
		gre_fvector4d pN;
		YMGRE_Fvector4d_SubToResult(&tpoints[i2].pos, &tpoints[i1].pos, &u);
		YMGRE_Fvector4d_SubToResult(&tpoints[i3].pos, &tpoints[i1].pos, &v);
		// 由于相机是左手坐标系，所以平面法实际向量与所求刚好相反
		YMGRE_Fvector4d_CrossToResult(&v, &u, &pN);//计算法向量 n = v×u = -u×v

		GRErgb24 planeColor = { 0 };
		//遍历光源
		for (GRE_ListNode curLightlist = LightList->listhead; curLightlist != NULL; curLightlist = curLightlist->next)
		{
			GRE_Light4d thisLight = curLightlist->data;
			YMGRE_PolygonLighting_Color(thispoly, &tpoints[i1].pos, &pN, thisLight, &planeColor, myobj->mirrorKs, 30);//平面光照计算
		}
		thispoly->planeColor_.R = planeColor.R;
		thispoly->planeColor_.G = planeColor.G;
		thispoly->planeColor_.B = planeColor.B;
	}
}

//物体光照结果写入外部缓存
static inline void YMGRE_ObjectLighting_ColorTo(GRE_Object4d myobj, GRE_Vertex4d tpoints, GRE_List LightList,
	gre_fvector4d* lightPos, GRE_FMat4x4 worldToCamera, GRErgb24* polygonColor)
{
	gre_log_explain((myobj == NULL) || (tpoints == NULL) || (worldToCamera == NULL) ||
		(polygonColor == NULL), GRE_LOG_PtrIO, "输入的物体、顶点或颜色缓存不存在");
	for (int i = 0; i < myobj->polygonNum; i++)
	{
		GRE_Polygon4d thispoly = &myobj->polygonList[i];
		int i1 = thispoly->index[0];
		int i2 = thispoly->index[1];
		int i3 = thispoly->index[2];
		gre_fvector4d u;
		gre_fvector4d v;
		gre_fvector4d pN;
		YMGRE_Fvector4d_SubToResult(&tpoints[i2].pos, &tpoints[i1].pos, &u);
		YMGRE_Fvector4d_SubToResult(&tpoints[i3].pos, &tpoints[i1].pos, &v);
		YMGRE_Fvector4d_CrossToResult(&v, &u, &pN);//计算法向量 n = v×u = -u×v

		GRErgb24 planeColor = { 0 };
		uint32 lightIndex = 0;
		for (GRE_ListNode curLightlist = LightList->listhead; curLightlist != NULL; curLightlist = curLightlist->next)
		{
			gre_light4d light = *(GRE_Light4d)curLightlist->data;
			light.proper.pos_ = lightPos[lightIndex++];
			if (light.type == GRE_SpotLight) {
				gre_fvector4d worldDirection = light.proper.spot.direct;
				YMGRE_Fvector4d_MatMultTo(worldToCamera, &worldDirection, &light.proper.spot.direct);
				light.proper.spot.direct.w = 0;
			}
			YMGRE_PolygonLighting_Color(thispoly, &tpoints[i1].pos, &pN, &light, &planeColor, myobj->mirrorKs, 30);//平面光照计算
		}
		polygonColor[i] = planeColor;
	}
}


#endif // !YMGRE_LIGHT_H
