/*
 * libps_bridge.h — Bridge header for rs_race2 to use libps_compat.
 *
 * This file is included in place of <libps.h> in the rs_race2
 * source files. It includes libps_compat.h which provides all
 * the types and function prototypes the game needs.
 */

#ifndef _LIBPS_BRIDGE_H
#define _LIBPS_BRIDGE_H

/* Pull in the compatibility layer */
#include "libps_compat.h"

/*
 * Additional defines and globals that the game's own headers
 * reference but which are normally in libps.h
 */

#define GsNONINTER	0
#define GsINTER		1
#define GsDITHER	1

/* PadInit/PadRead — provided by libps_compat's GetPadBuf */
#define PadInit()	GetPadBuf(&bb0, &bb1)

/* The game's code uses WORLD as a global */
extern GsCOORDINATE2 *WORLD;

#endif /* _LIBPS_BRIDGE_H */
