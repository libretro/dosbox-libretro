#ifndef _LIBRETRO_DOSBOX_H
#define _LIBRETRO_DOSBOX_H

#include <stdint.h>
#include <pthread.h>
#include <queue>
#include "libretro.h"

#if 1
# define RETROLOG(msg) printf("%s\n", msg)
#else
# define RETROLOG(msg)
#endif

extern retro_video_refresh_t video_cb;
extern retro_audio_sample_batch_t audio_batch_cb;
extern retro_input_poll_t poll_cb;
extern retro_input_state_t input_cb;
extern retro_environment_t environ_cb;

#if 0
struct RetroFrame
{
    uint8_t* data;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
};

struct lr_event_t
{
    void (*fn)(void* data);
    void* data;
    
    lr_event_t() : fn(0), data(0) { }
    lr_event_t(void (*fn_)(void*), void* data_) : fn(fn_), data(data_) { }
};

struct lr_queue_t
{
    std::queue<lr_event_t> queue;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    
    lr_queue_t()
    {
        if (pthread_mutex_init(&lock, 0))
            RETROLOG("Could not create queue mutex.");
            
        if (pthread_cond_init(&cond, 0))
            RETROLOG("Could not create queue condition.");
    }
    
    ~lr_queue_t()
    {
        if (pthread_mutex_destroy(&lock))
            RETROLOG("Could not destroy queue mutex.");
            
        if (pthread_cond_destroy(&cond))
            RETROLOG("Could not destory queue condition.");
    }
    
    void add_event(void (*fn)(void*), void* data)
    {
        if (pthread_mutex_lock(&lock))
            RETROLOG("Could not lock queue mutex.");

        queue.push(lr_event_t(fn, data));
        
        if (pthread_mutex_unlock(&lock))
            RETROLOG("Could not unlock queue mutex.");
            
        if (pthread_cond_signal(&cond))
            RETROLOG("Could not signal the condition.");
    }
    
    bool process_event()
    {
        if (pthread_mutex_lock(&lock))
            RETROLOG("Could not lock queue mutex.");

        while (queue.empty())
            if (pthread_cond_wait(&cond, &lock))
                RETROLOG("Could not wait for condition.");

        lr_event_t event = queue.front();
        queue.pop();
            
        if (pthread_mutex_unlock(&lock))
            RETROLOG("Could not unlock queue mutex.");

        event.fn(event.data);
            
        return true;
    }
};

extern lr_queue_t libretro_queue; // Commands to run on libretro thread
extern lr_queue_t dosbox_queue;   // Commands to run on dosbox thread

//

extern volatile bool have_frame;

void RETRO_PostFrameEvent(void* data);
void RETRO_PostAudioEvent(void* data);

void GFX_GetRetroColorFormat();

#endif
#endif