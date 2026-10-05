#include "raster_pool.h"
#include <SDL2/SDL.h>
#include <stdlib.h>
struct RasterPool {
 SDL_mutex* mutex;SDL_cond *ready,*done;SDL_Thread* threads[32];
 int workers,stop;unsigned generation;
 uint32 next,count,remaining;GRE_RasterJob job;void* context;
};
static int worker(void* user)
{
 RasterPool* p=user;unsigned generation=0;
 SDL_LockMutex(p->mutex);
 for(;;){
  while(!p->stop&&generation==p->generation)SDL_CondWait(p->ready,p->mutex);
  if(p->stop)break;
  generation=p->generation;
  while(p->next<p->count){
   uint32 index=p->next++;GRE_RasterJob job=p->job;void* context=p->context;
   SDL_UnlockMutex(p->mutex);job(context,index);SDL_LockMutex(p->mutex);
   if(--p->remaining==0)SDL_CondSignal(p->done);
  }
 }
 SDL_UnlockMutex(p->mutex);return 0;
}
RasterPool* raster_pool_create(int threads)
{
 if(threads<2||threads>32)return NULL;
 RasterPool* p=calloc(1,sizeof(*p));if(!p)return NULL;
 p->mutex=SDL_CreateMutex();p->ready=SDL_CreateCond();p->done=SDL_CreateCond();
 if(!p->mutex||!p->ready||!p->done){raster_pool_destroy(p);return NULL;}
 for(int i=0;i<threads;i++){
  p->threads[i]=SDL_CreateThread(worker,"YMGRE raster",p);
  if(!p->threads[i]){raster_pool_destroy(p);return NULL;}
  p->workers++;
 }
 return p;
}
void raster_pool_dispatch(void* user,GRE_RasterJob job,void* context,uint32 count)
{
 RasterPool* p=user;if(!count)return;
 SDL_LockMutex(p->mutex);
 p->job=job;p->context=context;p->count=count;p->remaining=count;p->next=0;p->generation++;
 SDL_CondBroadcast(p->ready);
 while(p->remaining)SDL_CondWait(p->done,p->mutex);
 SDL_UnlockMutex(p->mutex);
}
void raster_pool_destroy(RasterPool* p)
{
 if(!p)return;
 if(p->workers){
  SDL_LockMutex(p->mutex);p->stop=1;SDL_CondBroadcast(p->ready);SDL_UnlockMutex(p->mutex);
  for(int i=0;i<p->workers;i++)SDL_WaitThread(p->threads[i],NULL);
 }
 if(p->done)SDL_DestroyCond(p->done);if(p->ready)SDL_DestroyCond(p->ready);
 if(p->mutex)SDL_DestroyMutex(p->mutex);free(p);
}
