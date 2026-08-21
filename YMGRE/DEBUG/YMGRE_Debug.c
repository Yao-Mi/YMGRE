#include"./YMGRE_Debug.h"
#include<stdio.h>
#include<stdlib.h>

#if YMGRE_DEBUG_PRINT

#else
//调试错误信息输出
uint8 GRE_Event_LogOutbuff[YMGRE_LOGOUT_LEN];

#endif // YMGRE_DEBUG_PRINT



/**
  * @brief 断言错误打印
  */
void gre_assert_fail_inform(uint8* failfile, uint32 failline)
{
	gre_log_print("error in file\" %s\" ,line in %d \r\n", failfile, failline);
	fflush(NULL);
	exit(EXIT_FAILURE);
}


/**
  * @brief 调试错误提示信息打印
  */
void gre_logout_imform(uint8* mytips, GREEVNLOG event)
{
	switch (event)
	{
	case GRE_LOG_PtrI: //输入指针为空
	{
		gre_log_print("intput ptr");
		break;
	}
	case GRE_LOG_PtrO: //输出 指针为空
	{
		gre_log_print("output ptr");
		break;
	}
	case GRE_LOG_PtrIO://输入&输出 指针为空
	{
		gre_log_print("io puts ptr");
		break;
	}
	case GRE_LOG_TypeI://输入 类型错误
	{
		gre_log_print("intput type");
		break;
	}
	case GRE_LOG_TypeO://输出 类型错误
	{
		gre_log_print("output type");
		break;
	}
	case GRE_LOG_TypeIO://输入&输出 类型错误
	{
		gre_log_print("io puts type");
		break;
	}
	case GRE_LOG_Mem0://小申请内存失败
	{
		gre_log_print("small memory");
		break;
	}
	case GRE_LOG_Mem1://大申请内存失败
	{
		gre_log_print("big memory");
		break;
	}
	case GRE_LOG_ParamI://输入参数错误
	{
		gre_log_print("input param");
		break;
	}
	default:
	{
		gre_log_print("something");
		break;
	}
	}
	gre_log_print(" is error\r\n");
	gre_log_print("Tips:%s\r\n", mytips);
	fflush(NULL);
	exit(EXIT_FAILURE);
}



