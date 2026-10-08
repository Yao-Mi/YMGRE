#include "YMGRE_Profile.h"
#include <string.h>
uint32 YMGRE_ProfileCycles[8];
static GRE_ProfileClock profileClock;
static void* profileUser;
void YMGRE_Profile_SetClock(GRE_ProfileClock clock,void* user){profileClock=clock;profileUser=user;}
void YMGRE_Profile_Reset(void){memset(YMGRE_ProfileCycles,0,sizeof(YMGRE_ProfileCycles));}
uint32 YMGRE_ProfileNow(void){
 if(profileClock)return profileClock(profileUser);
#if (defined(__CC_ARM) || defined(__ARMCC_VERSION)) && (defined(__TARGET_ARCH_7_M) || defined(__TARGET_ARCH_7E_M))
 return *(volatile uint32*)0xe0001004u;
#elif defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)
 return *(volatile uint32*)0xe0001004u;
#else
 return 0;
#endif
}
