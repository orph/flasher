
NAME=flasher
VERSION=0.2
SOURCES=flasher.c curlstream.c
HEADERS=flasher.h curlstream.h $(NPAPI)
EXTRA_DIST=AUTHORS COPYING Makefile

NPAPI=					\
	npapi/jni.h			\
	npapi/jni_md.h			\
	npapi/jri.h			\
	npapi/jri_md.h			\
	npapi/jritypes.h		\
	npapi/npapi.h			\
	npapi/npruntime.h		\
	npapi/nptypes.h			\
	npapi/npupp.h			\
	npapi/nspr			\
	npapi/nspr/obsolete		\
	npapi/nspr/obsolete/protypes.h	\
	npapi/nspr/prcpucfg.h		\
	npapi/nspr/prtypes.h

CURL_CFLAGS=`curl-config --cflags`
CURL_LIBS=`curl-config --libs`

INCLUDES=-Wall -I npapi -I npapi/nspr $(CURL_CFLAGS)
LIBS=-lXt $(CURL_LIBS)

ifdef DEBUG
INCLUDES+=-DDEBUG
endif

all: $(NAME)

$(NAME): Makefile $(SOURCES)
	$(CC) -o $@ -g $(INCLUDES) $(LIBS) $(SOURCES)

install: $(NAME)
	@echo "Just copy '$(NAME)' to your destination."

clean:
	$(RM) $(NAME)

dist: $(SOURCES) $(HEADERS) $(EXTRA_DIST)
	-$(RM) -r $(NAME)-$(VERSION).tar.gz $(NAME)-$(VERSION)-tmp.tar.gz $(NAME)-$(VERSION)
	tar zcf $(NAME)-$(VERSION)-tmp.tar.gz $^
	mkdir $(NAME)-$(VERSION)
	tar zx -C $(NAME)-$(VERSION) -f $(NAME)-$(VERSION)-tmp.tar.gz
	tar zcf $(NAME)-$(VERSION).tar.gz $(NAME)-$(VERSION)
	-$(RM) -r $(NAME)-$(VERSION)-tmp.tar.gz $(NAME)-$(VERSION)
