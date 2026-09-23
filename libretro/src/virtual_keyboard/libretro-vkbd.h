#ifndef LIBRETRO_VKBD_H
#define LIBRETRO_VKBD_H

#include "libretro.h"
#include "libretro-graph.h"
#include <stdint.h>

extern bool retro_vkbd;
extern bool retro_capslock;

extern void print_vkbd(void);
extern void input_vkbd(void);
extern void toggle_vkbd(void);

extern unsigned int opt_vkbd_theme;
extern libretro_graph_alpha_t opt_vkbd_alpha;

#define VKBDX 13
#define VKBDY 7

#if 0
#define POINTER_DEBUG
#endif
#ifdef POINTER_DEBUG
extern int pointer_x;
extern int pointer_y;
#endif

#endif /* LIBRETRO_VKBD_H */
