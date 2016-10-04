/*
 *  Copyright (C) 2002-2013  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _RENDER_SCALERS_H
#define _RENDER_SCALERS_H

#include "video.h"
// reduced to save some memory
#define SCALER_MAXWIDTH		800 
#define SCALER_MAXHEIGHT	600

#define SCALER_BLOCKSIZE	16

typedef enum {
	scalerMode8, scalerMode15, scalerMode16, scalerMode32
} scalerMode_t;

typedef enum scalerOperation {
	scalerOpNormal,
	scalerLast
} scalerOperation_t;

typedef void (*ScalerLineHandler_t)(const void *src);
typedef void (*ScalerComplexHandler_t)(void);

extern Bit8u Scaler_Aspect[];
extern Bit8u diff_table[];
extern Bitu Scaler_ChangedLineIndex;
extern Bit16u Scaler_ChangedLines[];
typedef union {
	Bit32u b32	[SCALER_MAXHEIGHT] [SCALER_MAXWIDTH];
	Bit16u b16	[SCALER_MAXHEIGHT] [SCALER_MAXWIDTH];
	Bit8u b8	[SCALER_MAXHEIGHT] [SCALER_MAXWIDTH];
} scalerSourceCache_t;
extern scalerSourceCache_t scalerSourceCache;
typedef ScalerLineHandler_t ScalerLineBlock_t[5][4];

typedef struct {
	const char *name;
	Bitu gfxFlags;
	Bitu xscale,yscale;
	ScalerComplexHandler_t Linear[4];
	ScalerComplexHandler_t Random[4];
} ScalerComplexBlock_t;

typedef struct {
	const char *name;
	Bitu gfxFlags;
	Bitu xscale,yscale;
	ScalerLineBlock_t	Linear;
	ScalerLineBlock_t	Random;
} ScalerSimpleBlock_t;


#define SCALE_LEFT	0x1
#define SCALE_RIGHT	0x2
#define SCALE_FULL	0x4

/* Simple scalers */
extern ScalerSimpleBlock_t ScaleNormal1x;
extern ScalerSimpleBlock_t ScaleNormalDw;
extern ScalerSimpleBlock_t ScaleNormalDh;
/* Complex scalers */
#endif
