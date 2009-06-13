/*==========================================================================*\
 *
 * flasher.c - Standalone Adobe Flash (TM) 7 player window.  
 * flasher (C) 2006 Alex Graveley
 *
\*==========================================================================*/


#include <dlfcn.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>

#include <X11/IntrinsicP.h> /* for XtTMRec */
#include <X11/CoreP.h>      /* for CorePart */

#include "flasher.h"
#include "curlstream.h"


static Display *x_display;
XtAppContext x_app_context; /* for flasher.h */


/*==========================================================================*\
 * Plugin entrypoints...
\*==========================================================================*/

NPPluginFuncs plugin_funcs; /* for flasher.h */
static NPNetscapeFuncs mozilla_funcs;

typedef char * (*NP_GetMIMEDescriptionUPP)(void);
static NP_GetMIMEDescriptionUPP gNP_GetMIMEDescription;

typedef NPError (*NP_GetValueUPP)(void *instance, 
				  NPPVariable variable, 
				  void *value);
static NP_GetValueUPP gNP_GetValue;

typedef NPError (*NP_InitializeUPP)(NPNetscapeFuncs *moz_funcs, 
				    NPPluginFuncs *plugin_funcs);
static NP_InitializeUPP gNP_Initialize;

typedef NPError (*NP_ShutdownUPP)(void);
static NP_ShutdownUPP gNP_Shutdown;


/*==========================================================================*\
 * Browser-side functions...
\*==========================================================================*/

/* Closes and deletes a stream */
NPError
NPN_DestroyStream(NPP instance, NPStream *stream, NPError reason)
{
	Debug("NPN_DestroyStream instance=%p, stream=%p, reason=%d\n", 
	      instance, stream, reason);

	CURLStream *s = stream->ndata;
	if (!s) {
		return NPERR_GENERIC_ERROR;
	}

	CURLStreamDestroy(s, reason);
	return NPERR_NO_ERROR;
}


/* Forces a repaint message for a windowless plug-in */
void
NPN_ForceRedraw(NPP instance)
{
	Debug("NPN_ForceRedraw instance=%p\n", instance);
	NOT_IMPLEMENTED();
}


/* Asks the browser to create a stream for the specified URL */
NPError
NPN_GetURL(NPP instance, const char *url, const char *target)
{
	Debug("NPN_GetURL instance=%p, url=%s, target=%s\n",
	      instance, url, target);

	if (target != NULL) {
		NOT_IMPLEMENTED();
		return NPERR_INVALID_PARAM;
	}

	CURLStream *s = CURLStreamNew(instance, url, False, NULL);
	return s ? NPERR_NO_ERROR : NPERR_GENERIC_ERROR;
}


/* Requests creation of a new stream for the specified URL */
NPError
NPN_GetURLNotify(NPP instance, 
		 const char *url, 
		 const char *target, 
		 void *notifyData)
{
	Debug("NPN_GetURLNotify instance=%p, url=%s, target=%s\n",
	      instance, url, target);

	if (target != NULL) {
		NOT_IMPLEMENTED();
		return NPERR_INVALID_PARAM;
	}

	CURLStream *s = CURLStreamNew(instance, url, True, notifyData);
	return s ? NPERR_NO_ERROR : NPERR_GENERIC_ERROR;
}


/* Allows the plug-in to query the browser for information */
NPError
NPN_GetValue(NPP instance, NPNVariable variable, void *value)
{
	Debug("NPN_GetValue instance=%p, variable=%d [%08x]\n", 
	      instance, variable & 0xffff, variable);

	switch (variable) {
	case NPNVxDisplay:
		*(void **)value = x_display;
		break;
	case NPNVxtAppContext:
		*(void **)value = XtDisplayToApplicationContext(x_display);
		break;
	case NPNVToolkit:
		*(NPNToolkitType *)value = NPNVGtk2;
		break;
	default:
		Warning("Unhandled variable %d for NPN_GetValue\n", 
			variable);
		return NPERR_INVALID_PARAM;
	}

	return NPERR_NO_ERROR;
}


/* 
 * Invalidates specified drawing area prior to repainting or refreshing a
 * windowless plug-in 
 */
void
NPN_InvalidateRect(NPP instance, NPRect *invalidRect)
{
	Debug("NPN_InvalidateRect top=%d, left=%d, bottom=%d, right=%d\n", 
	      invalidRect->top, invalidRect->left, invalidRect->bottom, 
	      invalidRect->right);
	NOT_IMPLEMENTED();
}


/*
 * Invalidates specified region prior to repainting or refreshing a
 * windowless plug-in.
 */
void
NPN_InvalidateRegion(NPP instance, NPRegion invalidRegion)
{
	XRectangle rect;
	XClipBox(invalidRegion, &rect);
	Debug("NPN_InvalidateRegion x=%d, y=%d, width=%d, height=%d\n", 
	      rect.x, rect.y, rect.width, rect.height);
	NOT_IMPLEMENTED();
}


/* Allocates memory from the browser's memory space. */
void *
NPN_MemAlloc(uint32 size)
{
	Debug("NPN_MemAlloc size=%d\n", size);
	return malloc(size);
}


/* Requests that the browser free a specified amount of memory. */
uint32
NPN_MemFlush(uint32 size)
{
	Debug("NPN_MemFlush size=%d\n", size);
	return 0;
}


/* Deallocates a block of allocated memory. */
void
NPN_MemFree(void *ptr)
{
	Debug("NPN_MemFree ptr=%p\n", ptr);
	free(ptr);
}


/* 
 * Requests the creation of a new data stream produced by the plug-in and
 * consumed by the browser.
 */
NPError
NPN_NewStream(NPP instance, 
	      NPMIMEType type, 
	      const char *target, 
	      NPStream **stream)
{
	Debug("NPN_NewStream instance=%p, type=%s, target=%s\n", 
	      instance, type, target);
	NOT_IMPLEMENTED();
	return NPERR_GENERIC_ERROR;
}


/* Posts data to a URL. */
NPError
NPN_PostURL(NPP instance, 
	    const char *url, 
	    const char *target, 
	    uint32 len, 
	    const char *buf, 
	    NPBool file)
{
	Debug("NPN_PostURL instance=%p, url=%s, target=%s, len=%d, "
	      "file=%s\n", instance, url, target, len, 
	      file ? "TRUE" : "FALSE");

	if (target != NULL) {
		NOT_IMPLEMENTED();
		return NPERR_INVALID_PARAM;
	}

	CURLStream *s = CURLStreamNewPost(instance, url, False, NULL, buf, 
					  len, file);
	return s ? NPERR_NO_ERROR : NPERR_GENERIC_ERROR;
}


/* Posts data to a URL, and receives notification of the result. */
NPError
NPN_PostURLNotify(NPP instance, 
		  const char *url, 
		  const char *target, 
		  uint32 len, 
		  const char *buf, 
		  NPBool file, 
		  void *notifyData)
{
	Debug("NPN_PostURLNotify instance=%p, url=%s, target=%s, len=%d, "
	      "file=%s\n", instance, url, target, len, 
	      file ? "TRUE" : "FALSE");

	if (target != NULL) {
		NOT_IMPLEMENTED();
		return NPERR_INVALID_PARAM;
	}

	CURLStream *s = CURLStreamNewPost(instance, url, True, notifyData, buf, 
					  len, file);
	return s ? NPERR_NO_ERROR : NPERR_GENERIC_ERROR;
}


/* Supposed to flush all plugins and reload. */
void
NPN_ReloadPlugins(NPBool reloadPages)
{
	Debug("NPN_ReloadPlugins reloadPages=%d\n", reloadPages);
	NOT_IMPLEMENTED();
}


/* Returns the Java execution environment. */
JRIEnv *
NPN_GetJavaEnv(void)
{
	Debug("NPN_GetJavaEnv\n");
	return NULL;
}


/* Returns the Java object associated with the plug-in instance. */
jref
NPN_GetJavaPeer(NPP instance)
{
	Debug("NPN_GetJavaPeer instance=%p\n", instance);
	return NULL;
}


/* Requests a range of bytes for a seekable stream. */
NPError
NPN_RequestRead(NPStream *stream, NPByteRange *rangeList)
{
	Debug("NPN_RequestRead stream=%p\n", stream);
	NOT_IMPLEMENTED();
	return NPERR_GENERIC_ERROR;
}


/* Sets various modes of plug-in operation. */
NPError
NPN_SetValue(NPP instance, NPPVariable variable, void *value)
{
	Debug("NPN_SetValue instance=%p, variable=%d\n", instance, variable);
	NOT_IMPLEMENTED();
	return NPERR_GENERIC_ERROR;
}


/* Displays a message on the status line of the browser window. */
void
NPN_Status(NPP instance, const char *message)
{
	Debug("NPN_Status instance=%p, message=%s\n", instance, message);
	NOT_IMPLEMENTED();
}


/* Returns the browser's user agent field. */
const char *
NPN_UserAgent(NPP instance)
{
	return "Mozilla/5.0 (X11; U; Linux i686; en-US) Flasher/0.1";
}


/* 
 * Pushes data into a stream produced by the plug-in and consumed by the
 * browser.
 */
int32
NPN_Write(NPP instance, NPStream *stream, int32 len, void *buf)
{
	Debug("NPN_Write instance=%p, stream=%p, len=%d\n", 
	      instance, stream, len);
	NOT_IMPLEMENTED();
	return -1;
}


/*==========================================================================*\
 * Plugin loading and initialization...
\*==========================================================================*/

/* 
 * Attempt to dynamically load libflashplayer.so and lookup entrypoints.  
 * First tries the regular library path, then ~/.mozilla/plugins/, and 
 * finally from /usr/lib/mozilla/plugins.
 */
static void
LoadFlashPlugin(void)
{
	char *user_plugin_path = NULL;

	const char *plugin_path = "libflashplayer.so";
	void *dlobj = dlopen(plugin_path, RTLD_LAZY);
	if (!dlobj) {
		const char *plugin = "/.mozilla/plugins/libflashplayer.so";
		char *home = getenv("HOME");
		assert(home);

		user_plugin_path = malloc(strlen(home) + strlen(plugin));

		strcpy(user_plugin_path, home);
		strcpy(&user_plugin_path[strlen(home)], plugin);

		plugin_path = user_plugin_path;
		dlobj = dlopen(plugin_path, RTLD_LAZY);
	}
	if (!dlobj) {
		plugin_path = "/usr/lib/mozilla/plugins/libflashplayer.so";
		dlobj = dlopen(plugin_path, RTLD_LAZY);
	}
	if (!dlobj) {
		Error("Unable to load Flash plugin: %s\n", dlerror());
	}

	Log("Using plugin: %s\n", plugin_path);
	free(user_plugin_path);

	gNP_GetMIMEDescription = (NP_GetMIMEDescriptionUPP)
		dlsym(dlobj, "NP_GetMIMEDescription");
	if (!gNP_GetMIMEDescription) {
		Error("Loading symbol NP_GetMIMEDescription: %s\n", dlerror());
	}

	gNP_Initialize = (NP_InitializeUPP) dlsym(dlobj, "NP_Initialize");
	if (!gNP_Initialize) {
		Error("Loading symbol NP_Initialize: %s\n", dlerror());
	}

	gNP_Shutdown = (NP_ShutdownUPP) dlsym(dlobj, "NP_Shutdown");
	if (!gNP_Shutdown) {
		Error("Loading symbol NP_Shutdown: %s\n", dlerror());
	}

	/* GetValue is optional. */
	gNP_GetValue = (NP_GetValueUPP) dlsym(dlobj, "NP_GetValue");
}


/* Create X connection and store global Xt app context */
static void
InitializeXt(int *argc, char **argv)
{
        XInitThreads();
        XtToolkitInitialize();
        x_app_context = XtCreateApplicationContext();
        x_display = XtOpenDisplay(x_app_context, NULL, PROGRAM_NAME, 
				  PROGRAM_NAME, NULL, 0, argc, argv);
}


/* Set up function pointers that can be called from the plugin. */
static void
InitializeFuncs(void)
{
	memset(&mozilla_funcs, 0, sizeof(NPNetscapeFuncs));
	mozilla_funcs.size = sizeof(mozilla_funcs);
	mozilla_funcs.version = (NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR;
	mozilla_funcs.geturl = NewNPN_GetURLProc(NPN_GetURL);
	mozilla_funcs.posturl = NewNPN_PostURLProc(NPN_PostURL);
	mozilla_funcs.requestread = NewNPN_RequestReadProc(NPN_RequestRead);
	mozilla_funcs.newstream = NewNPN_NewStreamProc(NPN_NewStream);
	mozilla_funcs.write = NewNPN_WriteProc(NPN_Write);
	mozilla_funcs.destroystream = 
		NewNPN_DestroyStreamProc(NPN_DestroyStream);
	mozilla_funcs.status = NewNPN_StatusProc(NPN_Status);
	mozilla_funcs.uagent = NewNPN_UserAgentProc(NPN_UserAgent);
	mozilla_funcs.memalloc = NewNPN_MemAllocProc(NPN_MemAlloc);
	mozilla_funcs.memfree = NewNPN_MemFreeProc(NPN_MemFree);
	mozilla_funcs.memflush = NewNPN_MemFlushProc(NPN_MemFlush);
	mozilla_funcs.reloadplugins = 
		NewNPN_ReloadPluginsProc(NPN_ReloadPlugins);
	mozilla_funcs.getJavaEnv = NewNPN_GetJavaEnvProc(NPN_GetJavaEnv);
	mozilla_funcs.getJavaPeer = NewNPN_GetJavaPeerProc(NPN_GetJavaPeer);
	mozilla_funcs.geturlnotify = NewNPN_GetURLNotifyProc(NPN_GetURLNotify);
	mozilla_funcs.posturlnotify = 
		NewNPN_PostURLNotifyProc(NPN_PostURLNotify);
	mozilla_funcs.getvalue = NewNPN_GetValueProc(NPN_GetValue);
	mozilla_funcs.setvalue = NewNPN_SetValueProc(NPN_SetValue);
	mozilla_funcs.invalidaterect = 
		NewNPN_InvalidateRectProc(NPN_InvalidateRect);

	memset(&plugin_funcs, 0, sizeof(NPPluginFuncs));
	plugin_funcs.size = sizeof(plugin_funcs);
}


/*==========================================================================*\
 * Plugin instance creation, window creation, file reading...
\*==========================================================================*/

/* Create a new plugin instance, passing some very basic arguments */
static NPError 
CallNew(NPP plugin, char *swf_file, int width, int height)
{
	/* FIXME: Store window state. */
	plugin->ndata = (void *) NULL;

	char width_s[50];
	sprintf(width_s, "%d", width);

	char height_s[50];
	sprintf(height_s, "%d", height);

	char *args [] = { 
		"SRC",
		"WIDTH",
		"HEIGHT",
		"MENU",
		"LOOP",
	};
	char *vals [] = { 
		swf_file,
		width_s,
		height_s,
		"FALSE",
		"TRUE",
	};

	return CallNPP_NewProc(plugin_funcs.newp, 
			       "application/x-shockwave-flash", plugin,
			       NP_FULL, 0, args, vals, NULL);
}


/* Create a new Xt window and pass it to the plugin. */
static NPError
CallSetWindow(NPP plugin, int width, int height)
{
	NPSetWindowCallbackStruct ws_info = { 0 };
	NPWindow win = { 0 };

	ws_info.type = NP_SETWINDOW;
	ws_info.display = x_display;

	int screen = DefaultScreen(ws_info.display);
	ws_info.visual = DefaultVisual(ws_info.display, screen);
	ws_info.colormap = DefaultColormap(ws_info.display, screen);
	ws_info.depth = DefaultDepth(ws_info.display, screen);

	win.type = NPWindowTypeWindow;
	win.x = win.y = 0;
	win.width = width;
	win.height = height;
	win.ws_info = &ws_info;

	XSetWindowAttributes attr;
	attr.bit_gravity = NorthWestGravity;
	attr.colormap = ws_info.colormap;
	attr.event_mask =
		ButtonMotionMask |
		ButtonPressMask |
		ButtonReleaseMask |
		KeyPressMask |
		KeyReleaseMask |
		EnterWindowMask |
		LeaveWindowMask |
		PointerMotionMask |
		StructureNotifyMask |
		VisibilityChangeMask |
		FocusChangeMask |
		ExposureMask;

	unsigned long mask = CWBitGravity | CWEventMask;
	if (attr.colormap)
		mask |= CWColormap;

	Window x_root_win = DefaultRootWindow(x_display);
	Window x_win = XCreateWindow(x_display, x_root_win,
				     win.x, win.y, win.width, win.height,
				     0, ws_info.depth, InputOutput, 
				     ws_info.visual, mask, &attr);

	XSelectInput(x_display, x_win, ExposureMask);
	XMapWindow(x_display, x_win);
	XFlush(x_display);

	Widget top_widget = XtAppCreateShell("drawingArea", PROGRAM_NAME, 
					     applicationShellWidgetClass, 
					     x_display, NULL, 0);

	Arg args[7];
	int n = 0;
	XtSetArg(args[n], XtNwidth, win.width); n++;
	XtSetArg(args[n], XtNheight, win.height); n++;
	XtSetValues(top_widget, args, n);

	Widget form = XtVaCreateWidget("form", 
				       compositeWidgetClass, 
				       top_widget, NULL);

	n = 0;
	XtSetArg(args[n], XtNwidth, win.width); n++;
	XtSetArg(args[n], XtNheight, win.height); n++;
	XtSetArg(args[n], XtNvisual, ws_info.visual); n++;
	XtSetArg(args[n], XtNcolormap, ws_info.colormap); n++;
	XtSetArg(args[n], XtNdepth, ws_info.depth); n++;
	XtSetValues(form, args, n);

	XSync(x_display, False);

	top_widget->core.window = x_win;
	XtRegisterDrawable(x_display, x_win, top_widget);

	XtRealizeWidget(form);
	XtRealizeWidget(top_widget);
	XtManageChild(form);

	XSelectInput(x_display, XtWindow(top_widget), 0x0fffff);
	XSelectInput(x_display, XtWindow(form), 0x0fffff);

	XSync(x_display, False);

	win.window = (void *) XtWindow(form);

	return CallNPP_SetWindowProc(plugin_funcs.setwindow, plugin, &win);
}


/* Open, read, and write the contents of swf_file to the plugin instance. */
static NPError
SendSrcStream(NPP plugin, char *swf_file)
{
	NPError err = NPERR_NO_ERROR;
	NPStream stream = { 0 };
	uint16 stype = 0;

	struct stat swf_stat;
	if (stat(swf_file, &swf_stat) < 0) {
		return NPERR_FILE_NOT_FOUND;
	}

	stream.url = swf_file;
	stream.end = swf_stat.st_size;
	stream.lastmodified = (uint32) swf_stat.st_ctime;

	err = CallNPP_NewStreamProc(plugin_funcs.newstream, plugin, 
				    "application/x-shockwave-flash", &stream,
				    True, &stype);
	if (err != NPERR_NO_ERROR) {
		return err;
	}

	if (stype == NP_NORMAL || stype == NP_ASFILE) {
		FILE *swf_fd = fopen(swf_file, "r");
		if (!swf_fd) {
			return NPERR_NO_DATA;
		}

		int write_idx = 0;
		unsigned char *data [1024 * 10];

		while (stream.end > 0) {
			int write_max = 
				CallNPP_WriteReadyProc(plugin_funcs.writeready,
						       plugin, &stream);
			Debug("NPP_WriteReady: write_max = %d, end = %d\n", 
			      write_max, stream.end);
			if (write_max <= 0) {
				break;
			}

			int bytes_read = MIN(sizeof(data), stream.end);
			if (fread(data, bytes_read, 1, swf_fd) != 1) {
				break;
			}
			Debug("fread: bytes_read = %d\n", bytes_read);

			int bytes_written = 
				CallNPP_WriteProc(plugin_funcs.write, plugin, 
						  &stream, write_idx, 
						  bytes_read,
						  (void *) data);
			Debug("NPP_Write: offset = %d, end = %d, "
			      "written = %d\n", write_idx, stream.end, 
			      bytes_read);
			if (bytes_written <= 0) {
				break;
			}

			write_idx += bytes_written;
			stream.end -= bytes_written;
		}

		fclose(swf_fd);
	}

	if (stype == NP_ASFILE || stype == NP_ASFILEONLY) {
		CallNPP_StreamAsFileProc(plugin_funcs.asfile, plugin, &stream, 
					 (stream.end == 0) ? swf_file : NULL);
	}

	if (stype != NP_SEEK) {
		err = CallNPP_DestroyStreamProc(
			plugin_funcs.destroystream, plugin, &stream,
			(stream.end == 0) ? NPRES_DONE : NPRES_NETWORK_ERR);
	}

	return err;
}


/*==========================================================================*\
 * Play utility, cmdline parsing, main...
\*==========================================================================*/

/* 
 * Helper to manage create a plugin instance, new window, then writing the 
 * file contents to the instance. 
 */
static NPError
PlaySWF(NPP_t *plugin, char *swf_file, int width, int height)
{
	NPError err = NPERR_NO_ERROR;

	/* 
	 * Without this, Flash segfaults attempting to dynamically invoke
	 * gtk_major_mode.  Linking GTK+ means that Flash uses GTK's mainloop
	 * for IO and timeouts, which we don't want.
	 *
	 * FIXME: Is there a better way?
	 */
	putenv("FLASH_GTK_LIBRARY=");

	err = gNP_Initialize(&mozilla_funcs, &plugin_funcs);
	if (err != NPERR_NO_ERROR) {
		Error("NP_Initialize result = %d\n", err);
	}

	err = CallNew(plugin, swf_file, width, height);
	if (err != NPERR_NO_ERROR) {
		Error("NPP_NewProc result = %d\n", err);
	}

	err = CallSetWindow(plugin, width, height);
	if (err != NPERR_NO_ERROR) {
		Error("NPP_SetWindow result = %d\n", err);
	}

	Log("Loading: %s\n", swf_file);

	err = SendSrcStream(plugin, swf_file);
	if (err != NPERR_NO_ERROR) {
		Error("Writing SWF file, result = %d\n", err);
	}

	return NPERR_NO_ERROR;
}


static int
ParseOptions(int argc, 
	     char **argv, 
	     char **geometry, 
	     int *fullscreen, 
	     char **baseurl,
	     char **swf_file)
{
	struct option long_options[] = {
		{ "help", no_argument, NULL, 'h' },
		{ "geometry", required_argument, NULL, 'g' },
		{ "fullscreen", no_argument, NULL, 'f' },
		{ "baseurl", required_argument, NULL, 'b' },
		{ 0, 0, 0, 0 }
	};

	while (True) {
		int opt = getopt_long_only(argc, argv, "-g:fh", long_options, 
					   NULL);
		switch (opt) {
		case -1:
			return True;
			break;
		case '?':
		case 'h':
			return False;
			break;
		case 'g':
			*geometry = optarg;
			break;
		case 'f':
			*fullscreen = True;
			break;
		case 'b':
			*baseurl = optarg;
			break;
		case 1:
			*swf_file = optarg;
			break;
		}
	}

	return True;
}


static void
PrintUsage(void)
{
	printf("Usage: %s SWFFILE [OPTION...]\n", PROGRAM_NAME);
	printf("  --geometry WIDTHxHEIGHT\tSpecify window width and height.\n");
	printf("  --fullsreen\t\t\tRun fullscreen.\n");
	printf("  --baseurl URL\t\t\tAppend relative references to URL.\n");
	printf("  --help\t\t\tPrint this usage information.\n");
	printf("\n");
}


int
main(int argc, char **argv)
{
	NPP_t plugin = { 0 };
	char *geometry = NULL;
	int fullscreen = False; /* FIXME: Implement */
	char *baseurl = NULL;
	char *swf_file = NULL;
	int width = 700;  /* Default height */
	int height = 400; /* Default width */

	if (!ParseOptions(argc, argv, 
			  &geometry, 
			  &fullscreen, 
			  &baseurl, 
			  &swf_file) || !swf_file) {
		PrintUsage();
		return 1;
	}

	if (geometry) {
		sscanf(geometry, "%dx%d", &width, &height);
		Log("Geometry: %dx%d\n", width, height);
	}

	CURLStreamInit(baseurl);
	
	LoadFlashPlugin();

	InitializeXt(&argc, argv);
	InitializeFuncs();

	PlaySWF(&plugin, swf_file, width, height);

	XtAppMainLoop(x_app_context);

	Log("Quitting...\n");
	gNP_Shutdown();

	CURLStreamShutdown();

	return 0;
}

