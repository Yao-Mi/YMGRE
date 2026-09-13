#include "demo_host.h"
#include <SDL.h> /* Backend-specific adversarial events stay in GRE's own tests. */
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"host input FAIL %d: %s\n",__LINE__,#x);goto done;}}while(0)
int main(void)
{
 YMGRE_DemoHost h={0};int ready=0,ok=0;
 YMGRE_DemoKeyBinding keys[]={{'w',1,0,0},{YMGRE_KEY_UP,1,0,0},{'d',2,0,0},
  {YMGRE_KEY_SPACE,0,4,0},{'1',0,0,1},{'2',0,0,2}};
 CHECK(YMGRE_DemoHost_Init(&h,96,64,16));ready=1;
 CHECK(YMGRE_DemoHost_SetTitle(&h,"GRE portable input test"));
 CHECK(YMGRE_DemoHost_BindKeys(&h,keys,6));
 CHECK(YMGRE_DemoHost_InjectKey(&h,'w',1));
 CHECK(YMGRE_DemoHost_InjectKey(&h,YMGRE_KEY_UP,1));
 CHECK(YMGRE_DemoHost_InjectKey(&h,'d',1));
 CHECK(YMGRE_DemoHost_HeldKeys(&h)==3);
 CHECK(YMGRE_DemoHost_InjectKey(&h,'w',0));
 CHECK(YMGRE_DemoHost_HeldKeys(&h)==3); /* held alias still down */
 CHECK(YMGRE_DemoHost_InjectKey(&h,YMGRE_KEY_UP,0));
 CHECK(YMGRE_DemoHost_HeldKeys(&h)==2);
 CHECK(YMGRE_DemoHost_InjectKey(&h,YMGRE_KEY_SPACE,1));
 CHECK(YMGRE_DemoHost_TakeActions(&h)==4&&YMGRE_DemoHost_TakeActions(&h)==0);
 CHECK(YMGRE_DemoHost_InjectKey(&h,'1',1)&&YMGRE_DemoHost_InjectKey(&h,'2',1));
 CHECK(YMGRE_DemoHost_TakeValue(&h)==2&&YMGRE_DemoHost_TakeValue(&h)==0);
 CHECK(YMGRE_DemoHost_InjectFocusLost(&h)&&YMGRE_DemoHost_HeldKeys(&h)==0);
 CHECK(YMGRE_DemoHost_InjectKey(&h,'w',1));
 YMGRE_DemoKeyBinding invalid[2]={{'w',1,0,0},{'w',2,0,0}};
 CHECK(!YMGRE_DemoHost_BindKeys(&h,invalid,2)&&YMGRE_DemoHost_HeldKeys(&h)==1);
 CHECK(!YMGRE_DemoHost_BindKeys(&h,keys,65));
 CHECK(!YMGRE_DemoHost_InjectKey(&h,99999,1));
 /* Foreign-window events cannot change this host. */
 SDL_Event e;memset(&e,0,sizeof e);e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_d;e.key.windowID=0xffffffffu;
 CHECK(SDL_PushEvent(&e)==1&&YMGRE_DemoHost_HeldKeys(&h)==1);
 /* Capture the injected window ID, then inject OS key-repeat for the action key. */
 unsigned window=0;
 while(SDL_PollEvent(&e))if(e.type==SDL_KEYDOWN&&e.key.keysym.sym==SDLK_w)window=e.key.windowID;
 CHECK(window!=0);
 memset(&e,0,sizeof e);e.type=SDL_KEYDOWN;e.key.windowID=window;e.key.keysym.sym=SDLK_SPACE;e.key.repeat=1;
 CHECK(SDL_PushEvent(&e)==1&&YMGRE_DemoHost_TakeActions(&h)==0);
 YMGRE_DemoHost_ClearInput(&h);CHECK(YMGRE_DemoHost_HeldKeys(&h)==0);
 double t=YMGRE_DemoHost_Time();CHECK(YMGRE_DemoHost_Step(&h,0)&&YMGRE_DemoHost_Time()>=t);
 CHECK(YMGRE_DemoHost_BindKeys(&h,NULL,0));
 CHECK(YMGRE_DemoHost_InjectKey(&h,'w',1)&&YMGRE_DemoHost_HeldKeys(&h)==0);
 ok=1;
 done:
 if(ready)YMGRE_DemoHost_Destroy(&h);
 if(ok&&h.input_backend)ok=0;
 printf("GRE_HOST_INPUT %s\n",ok?"PASS":"FAIL");return !ok;
}
