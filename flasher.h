/*==========================================================================*\
 *
 * flasher.h - Standalone Adobe Flash (TM) 7 player window.  
 * flasher (C) 2006 Alex Graveley
 *
\*==========================================================================*/

#ifndef __FLASHER_H__
#define __FLASHER_H__


#define PROGRAM_NAME "flasher"


#include <stdio.h>
#include <X11/Intrinsic.h> /* for XtAppContext */

#define XP_UNIX 1
#define MOZ_X11 1
#include "npapi.h"
#include "npupp.h"


/*==========================================================================*\
 * Logging utils...
\*==========================================================================*/

/* #define DEBUG */

#ifdef DEBUG
#define Debug(fmt...) Log(fmt)
#else
#define Debug(fmt...) do {} while (0)
#endif
#define Log(fmt...) printf(fmt)
#define Warning(fmt...) Log("WARNING: " fmt)
#define Error(fmt...) Log("ERROR: " fmt); exit(1)

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define NOT_IMPLEMENTED() \
	Warning("Unimplemented function %s at line %d\n", __func__, __LINE__)


/*==========================================================================*\
 * Globals...
\*==========================================================================*/

extern XtAppContext x_app_context;
extern NPPluginFuncs plugin_funcs;


#endif /* __FLASHER_H__ */
