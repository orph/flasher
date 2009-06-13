/*==========================================================================*\
 *
 * curlstream.c - cURL stream processing for flasher.  
 * flasher (C) 2006 Alex Graveley
 *
\*==========================================================================*/


#include <errno.h>
#include <curl/curl.h>
#include <unistd.h>

#include "curlstream.h"
#include "flasher.h"


struct _CURLStream
{
	NPP_t *plugin;
	NPStream np_stream;
	CURL *req;
	uint16 stype;
	Bool notify;
	char *absolute_url;

	FILE *outfile;
	char *outfile_path;
	int   outfile_idx;
	FILE *infile;
};


static CURLM *curl_handle = NULL;
static XtWorkProcId curl_work_id = 0;
static int curl_running_handles = 0;
static Bool curl_need_perform = False;
static char *curl_baseurl = NULL;


static Boolean CURLStreamPoll(NPP_t *plugin);
static size_t CURLStreamWriteCb(char *buffer, size_t size, size_t nitems, 
			 void *instream);


CURLStream *
CURLStreamNew(NPP_t *plugin, const char *url, Bool notify, void* notifyData)
{
	Debug("CURLStreamNew uri=%s, notify=%d, notifyData=%p\n",
	      url, notify, notifyData);

	CURLStream *s = malloc(sizeof(CURLStream));

	s->plugin = plugin;

	s->np_stream.url = strdup(url);
	s->np_stream.notifyData = notifyData;
	s->np_stream.ndata = s;
	s->np_stream.end = 0;
	s->np_stream.lastmodified = 0;
	s->np_stream.pdata = NULL;

	s->stype = 0;
	s->notify = notify;
	s->outfile = NULL;
	s->outfile_path = NULL;
	s->outfile_idx = 0;
	s->infile = NULL;

	NPError err = CallNPP_NewStreamProc(plugin_funcs.newstream, plugin, 
					    NULL /* FIXME: mimetype */, 
					    &s->np_stream, False, &s->stype);
	if (err != NPERR_NO_ERROR) {
		free((char *) s->np_stream.url);
		free(s);
		return NULL;
	}

	int baseurl_len = strlen(curl_baseurl ? curl_baseurl : "");
	s->absolute_url = malloc(baseurl_len + strlen(url) + 2);

	if (curl_baseurl && !strchr(url, ':')) {
		strcpy(s->absolute_url, curl_baseurl);
		if (curl_baseurl[baseurl_len] != '/') {
			strcat(s->absolute_url, "/");
		}
		strcat(s->absolute_url, url);

		Debug("CURLStreamNew: Using absolute URL '%s'\n", 
		      s->absolute_url);
	} else {
		strcpy(s->absolute_url, url);
	}

	s->req = curl_easy_init();
	curl_easy_setopt(s->req, CURLOPT_URL, s->absolute_url);
	curl_easy_setopt(s->req, CURLOPT_PRIVATE, s);
	curl_easy_setopt(s->req, CURLOPT_WRITEDATA, s);
	curl_easy_setopt(s->req, CURLOPT_WRITEFUNCTION, CURLStreamWriteCb);
	curl_multi_add_handle(curl_handle, s->req);

	if (s->stype == NP_ASFILEONLY || s->stype == NP_ASFILE) {
		char tmppath[100];
		snprintf(tmppath, sizeof(tmppath), "/tmp/%s-%d-XXXXXX",
			 PROGRAM_NAME, getpid());

		int outfd = mkstemp(tmppath); // Mutates tmppath
		s->outfile = fdopen(outfd, "w+");
		s->outfile_path = strdup(tmppath);
	}

	if (curl_work_id == 0) {
		curl_work_id = XtAppAddWorkProc(x_app_context, 
						(XtWorkProc) CURLStreamPoll, 
						&plugin);
	}
	curl_need_perform = True;

	return s;
}


CURLStream *
CURLStreamNewPost(NPP_t *plugin, 
		  const char *url, 
		  Bool notify, 
		  void* notifyData,
		  const char *buf, 
		  uint32 len,
		  Bool is_file)
{
	FILE *infile = NULL;

	if (is_file) {
		infile = fopen(buf, "r");
		if (!infile) {
			Warning("Error opening file '%s' for posting: %s\n", 
				buf, strerror(errno));
			return NULL;
		}
	}

	CURLStream *s = CURLStreamNew(plugin, url, notify, notifyData);
	if (!s) {
		fclose(infile);
		return NULL;
	}

	if (is_file) {
		s->infile = infile;
		curl_easy_setopt(s->req, CURLOPT_INFILE, s->infile);
	} else {
		curl_easy_setopt(s->req, CURLOPT_POSTFIELDS, buf);
		curl_easy_setopt(s->req, CURLOPT_POSTFIELDSIZE, len);
	}

	curl_easy_setopt(s->req, CURLOPT_POST, True);

	return s;
}


void 
CURLStreamDestroy(CURLStream *s, NPReason reason)
{
	Debug("CURLStreamDestroy curlstream=%p, reason=%d\n", s, reason);

	if (reason == NPRES_DONE) {
		CallNPP_StreamAsFileProc(plugin_funcs.asfile, s->plugin,
					 &s->np_stream, s->outfile_path);
	}

	if (s->notify) {
		CallNPP_URLNotifyProc(plugin_funcs.urlnotify, 
				      s->plugin, s->np_stream.url,
				      reason, s->np_stream.notifyData);
	}
	CallNPP_DestroyStreamProc(plugin_funcs.destroystream, s->plugin, 
				  &s->np_stream, reason);

	if (s->req) {
		curl_easy_setopt(s->req, CURLOPT_PRIVATE, NULL);
		curl_easy_cleanup(s->req);
	}

	free((char *) s->np_stream.url);
	free(s->absolute_url);

	if (s->outfile) {
		fclose(s->outfile);
	}
	unlink(s->outfile_path);
	free(s->outfile_path);

	if (s->infile) {
		fclose(s->infile);
	}
	free(s);
}


void 
CURLStreamInit(const char *baseurl)
{
	curl_baseurl = baseurl ? strdup(baseurl) : NULL;

  	curl_global_init(0);
	curl_handle = curl_multi_init();
	assert(curl_handle);
}


void 
CURLStreamShutdown(void)
{
	curl_multi_cleanup(curl_handle);
	curl_global_cleanup();

	free(curl_baseurl);
}


static Boolean
CURLStreamPoll(NPP_t *plugin)
{
	if (curl_running_handles > 0) {
		fd_set read;
		fd_set write;
		fd_set except;
		int max_fd = 0;

		curl_multi_fdset(curl_handle, &read, &write, &except, &max_fd);
		if (max_fd > 0) {
			struct timeval tv;
			tv.tv_sec = 0;
			tv.tv_usec = 0;

			int res = select(max_fd, &read, &write, &except, &tv);
			if (res < 0 && errno != EINTR) {
				Warning("Error waiting for IO: %s\n", 
					strerror(errno));
				return False;
			} else if (res > 0) {
				curl_need_perform = True;
			}
		}
	}

	if (curl_need_perform) {
		while (curl_multi_perform(curl_handle, &curl_running_handles) ==
		       CURLM_CALL_MULTI_PERFORM) {
		}
		curl_need_perform = False;
	}

	while (True) {
		int msg_cnt = 0;
		CURLMsg *msg = curl_multi_info_read(curl_handle, &msg_cnt);
		if (!msg) {
			break;
		} else if (msg->msg != CURLMSG_DONE) {
			continue;
		}

		CURLStream *s = NULL;
		curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &s);
		assert(s);

		CURLStreamDestroy(s, msg->data.result);
	};

	if (curl_running_handles == 0) {
		curl_work_id = 0;
		return True; // Done for now.
	}

	return False;
}


static size_t
CURLStreamWriteCb(char *buffer,
		  size_t size,
		  size_t nitems,
		  void *instream)
{
	Debug("CURLStreamWriteCb buffer=%p, size=%d, nitems=%d, "
	      "curlstream=%p\n", buffer, size, nitems, instream);

	CURLStream *s = (CURLStream *) instream;
	int bytes_written = 0;
	int nwritten = nitems;

	if (s->outfile) {
		nwritten = fwrite(buffer, size, nitems, s->outfile);
	}
	if (s->stype == NP_ASFILEONLY) {
		// Don't send WriteReady and Write calls for ASFILEONLY
		return nwritten;
	}

	while (bytes_written < (nwritten * size)) {
		int write_max = 
			CallNPP_WriteReadyProc(plugin_funcs.writeready,
					       s->plugin, &s->np_stream);
		Debug("NPP_WriteReady: write_max = %d, end = %d\n", 
		      write_max, -1);
		if (write_max <= 0) {
			break;
		}

		bytes_written += 
			CallNPP_WriteProc(plugin_funcs.write, s->plugin, 
					  &s->np_stream, s->outfile_idx, 
					  (nwritten * size) - bytes_written,
					  (void *) buffer);
		Debug("NPP_Write: offset = %d, end = %d, "
		      "written = %d\n", s->outfile_idx, -1, 
		      bytes_written);
		if (bytes_written <= 0) {
			break;
		}

		s->outfile_idx += bytes_written;
		buffer += bytes_written;
	}

	return nwritten;
}
