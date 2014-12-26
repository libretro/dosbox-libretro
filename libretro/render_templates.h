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

#if DBPP == 8
#define PSIZE 1
#define PTYPE Bit8u
#define WC scalerWriteCache.b8
//#define FC scalerFrameCache.b8
#define FC (*(scalerFrameCache_t*)(&scalerSourceCache.b32[400][0])).b8
#define redMask		0
#define	greenMask	0
#define blueMask	0
#define redBits		0
#define greenBits	0
#define blueBits	0
#define redShift	0
#define greenShift	0
#define blueShift	0
#elif DBPP == 15 || DBPP == 16
#define PSIZE 2
#define PTYPE Bit16u
#define WC scalerWriteCache.b16
//#define FC scalerFrameCache.b16
#define FC (*(scalerFrameCache_t*)(&scalerSourceCache.b32[400][0])).b16
#if DBPP == 15
#define	redMask		0x7C00
#define	greenMask	0x03E0
#define	blueMask	0x001F
#define redBits		5
#define greenBits	5
#define blueBits	5
#define redShift	10
#define greenShift	5
#define blueShift	0
#elif DBPP == 16
#define redMask		0xF800
#define greenMask	0x07E0
#define blueMask	0x001F
#define redBits		5
#define greenBits	6
#define blueBits	5
#define redShift	11
#define greenShift	5
#define blueShift	0
#endif
#elif DBPP == 32
#define PSIZE 4
#define PTYPE Bit32u
#define WC scalerWriteCache.b32
//#define FC scalerFrameCache.b32
#define FC (*(scalerFrameCache_t*)(&scalerSourceCache.b32[400][0])).b32
#define redMask		0xff0000
#define greenMask	0x00ff00
#define blueMask	0x0000ff
#define redBits		8
#define greenBits	8
#define blueBits	8
#define redShift	16
#define greenShift	8
#define blueShift	0
#endif

#define redblueMask (redMask | blueMask)


#if SBPP == 8 || SBPP == 9
#define SC scalerSourceCache.b8
#if DBPP == 8
#define PMAKE(_VAL) (_VAL)
#elif DBPP == 15
#define PMAKE(_VAL) render.pal.lut.b16[_VAL]
#elif DBPP == 16
#define PMAKE(_VAL) render.pal.lut.b16[_VAL]
#elif DBPP == 32
#define PMAKE(_VAL) render.pal.lut.b32[_VAL]
#endif
#define SRCTYPE Bit8u
#endif

#if SBPP == 15
#define SC scalerSourceCache.b16
#if DBPP == 15
#define PMAKE(_VAL) (_VAL)
#elif DBPP == 16
#define PMAKE(_VAL) (((_VAL) & 31) | ((_VAL) & ~31) << 1)
#elif DBPP == 32
#define PMAKE(_VAL)  (((_VAL&(31<<10))<<9)|((_VAL&(31<<5))<<6)|((_VAL&31)<<3))
#endif
#define SRCTYPE Bit16u
#endif

#if SBPP == 16
#define SC scalerSourceCache.b16
#if DBPP == 15
#define PMAKE(_VAL) (((_VAL&~31)>>1)|(_VAL&31))
#elif DBPP == 16
#define PMAKE(_VAL) (_VAL)
#elif DBPP == 32
#define PMAKE(_VAL)  (((_VAL&(31<<11))<<8)|((_VAL&(63<<5))<<5)|((_VAL&31)<<3))
#endif
#define SRCTYPE Bit16u
#endif

#if SBPP == 32
#define SC scalerSourceCache.b32
#if DBPP == 15
#define PMAKE(_VAL) (PTYPE)(((_VAL&(31<<19))>>9)|((_VAL&(31<<11))>>6)|((_VAL&(31<<3))>>3))
#elif DBPP == 16
#define PMAKE(_VAL) (PTYPE)(((_VAL&(31<<19))>>8)|((_VAL&(63<<10))>>4)|((_VAL&(31<<3))>>3))
#elif DBPP == 32
#define PMAKE(_VAL) (_VAL)
#endif
#define SRCTYPE Bit32u
#endif

//  C0 C1 C2 D3
//  C3 C4 C5 D4
//  C6 C7 C8 D5
//  D0 D1 D2 D6

#define C0 fc[-1 - SCALER_COMPLEXWIDTH]
#define C1 fc[+0 - SCALER_COMPLEXWIDTH]
#define C2 fc[+1 - SCALER_COMPLEXWIDTH]
#define C3 fc[-1 ]
#define C4 fc[+0 ]
#define C5 fc[+1 ]
#define C6 fc[-1 + SCALER_COMPLEXWIDTH]
#define C7 fc[+0 + SCALER_COMPLEXWIDTH]
#define C8 fc[+1 + SCALER_COMPLEXWIDTH]

#define D0 fc[-1 + 2*SCALER_COMPLEXWIDTH]
#define D1 fc[+0 + 2*SCALER_COMPLEXWIDTH]
#define D2 fc[+1 + 2*SCALER_COMPLEXWIDTH]
#define D3 fc[+2 - SCALER_COMPLEXWIDTH]
#define D4 fc[+2]
#define D5 fc[+2 + SCALER_COMPLEXWIDTH]
#define D6 fc[+2 + 2*SCALER_COMPLEXWIDTH]

/* Simple scalers */
#define SCALERNAME		Normal1x
#define SCALERWIDTH		1
#define SCALERHEIGHT	1
#define SCALERFUNC								\
	line0[0] = P;
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		NormalDw
#define SCALERWIDTH		2
#define SCALERHEIGHT	1
#define SCALERFUNC								\
	line0[0] = P;								\
	line0[1] = P;
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		NormalDh
#define SCALERWIDTH		1
#define SCALERHEIGHT	2
#define SCALERFUNC								\
	line0[0] = P;								\
	line1[0] = P;
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

/* Complex scalers */

#undef PSIZE
#undef PTYPE
#undef PMAKE
#undef WC
#undef LC
#undef FC
#undef SC
#undef redMask
#undef greenMask
#undef blueMask
#undef redblueMask
#undef redBits
#undef greenBits
#undef blueBits
#undef redShift
#undef greenShift
#undef blueShift
#undef SRCTYPE
