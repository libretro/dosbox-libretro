/*
  libco.win (2016-09-06)
  authors: frangarcj
  license: public domain
*/

#define LIBCO_C
#include <libco.h>
#include <stdlib.h>
#include <emscripten.h>

#ifdef __cplusplus
extern "C" {
#endif

static thread_local cothread_t co_active_ = 0;

void co_thunk(void* argOnInitialize)
{
   ((void (*)(void))argOnInitialize)();
}

cothread_t co_active(void)
{
   if(!co_active_)
   {
      co_active_ = (cothread_t)1;
   }
   return co_active_;
}

cothread_t co_create(unsigned int heapsize, void (*coentry)(void))
{
   if(!co_active_)
   {
      co_active_ = (cothread_t)1;
   }

   emscripten_coroutine ret = emscripten_coroutine_create(co_thunk, coentry, heapsize);
  
   return ret;
}

void co_delete(cothread_t cothread)
{
	 return;
}

void co_switch(cothread_t cothread)
{

   if(cothread == (cothread_t)1){
     co_active_ = cothread;
     emscripten_yield();
   }else{
		 emscripten_coroutine theCoroutine = (emscripten_coroutine)cothread;
		 co_active_ = cothread;
		 emscripten_coroutine_next(theCoroutine);
   }
}

#ifdef __cplusplus
}
#endif