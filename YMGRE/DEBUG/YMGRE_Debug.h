#ifndef _YMGRE_DEBUG_H
#define _YMGRE_DEBUG_H

#include "../CONFIG/YMGRE_PubType.h"

#define YMGRE_ASSERT_DEBUG 1 //断言调试
#define YMGRE_DEBUG_MODE   1 //调试模式
#define YMGRE_DEBUG_PRINT  1 //直接打印

//调试信息打印输出
#if YMGRE_DEBUG_PRINT
#define gre_log_print(...)  printf(__VA_ARGS__)
#else 
#define YMGRE_LOGOUT_LEN 100 //调试信息输出缓冲区
extern uint8 GRE_Event_LogOutbuff[YMGRE_LOGOUT_LEN];

#endif // YMGRE_DEBUG_PRINT



//调试的日志信息类型
typedef enum
{
	GRE_LOG_OK = 0x00,//运行正常
	GRE_LOG_PtrI   = 0x01, //输入指针为空
	GRE_LOG_PtrO   = 0x02, //输出 指针为空
	GRE_LOG_PtrIO  = 0x03, //输入&输出 指针为空  GRE_LOG_PtrI|GRE_LOG_PtrO =1+2=3

	GRE_LOG_TypeI  = 0x04, //输入 类型错误
	GRE_LOG_TypeO  = 0x08, //输出 类型错误
	GRE_LOG_TypeIO = 0x0C, //输入&输出 类型错误  GRE_LOG_TypeI|GRE_LOG_TypeO=4+8=c
	GRE_LOG_Mem0   = 0x10, //小申请内存失败
	GRE_LOG_Mem1   = 0x20, //大申请内存失败

	GRE_LOG_ParamI = 0x40,//输入数据错误
	GRE_LOG_FILE = 0x50,//文件读写错误
}GREEVNLOG;



#if YMGRE_ASSERT_DEBUG
//定义断言
#define gre_assert(x)  ((x)? (void)0U:  gre_assert_fail_inform((uint8*)__FILE__,__LINE__))
//断言信息输出
void gre_assert_fail_inform(uint8* failfile, uint32 failline);

#else
#define gre_assert(x)
#endif // YMGRE_ASSERT_DEBUG


#if YMGRE_DEBUG_MODE

//解释
#define gre_log_explain(logthis,logtype,logtips) ((logthis)? gre_logout_imform((uint8*)logtips,logtype):(void)0U)
//提示信息
void gre_logout_imform(uint8* tips,GREEVNLOG event);
#else

#endif



#endif
