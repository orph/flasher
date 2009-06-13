/*==========================================================================*\
 *
 * curlstream.h - cURL stream processing for flasher.  
 * flasher (C) 2006 Alex Graveley
 *
\*==========================================================================*/

#ifndef __CURLSTREAM_H__
#define __CURLSTREAM_H__


#include "flasher.h"


typedef struct _CURLStream CURLStream;


CURLStream *CURLStreamNew(NPP_t *plugin, 
			  const char *url, 
			  Bool notify, 
			  void* notifyData);

CURLStream *CURLStreamNewPost(NPP_t *plugin, 
			      const char *url, 
			      Bool notify, 
			      void* notifyData,
			      const char *buf, 
			      uint32 len,
			      Bool is_file);

void CURLStreamDestroy(CURLStream *s, NPReason reason);

void CURLStreamInit(const char *baseurl);

void CURLStreamShutdown(void);


#endif /* __CURLSTREAM_H__ */
