#include <SDL_version.h>

#if !SDL_VERSION_ATLEAST(2, 0, 0)
#    include <SDL_rwops.h>

Sint64 SDL_RWsize_missing(SDL_RWops* context)
{
    const int current = SDL_RWtell(context);
    const int begin = SDL_RWseek(context, 0, RW_SEEK_SET);
    const int end = SDL_RWseek(context, 0, RW_SEEK_END);
    SDL_RWseek(context, current, RW_SEEK_SET);
    return end - begin;
}

#endif
