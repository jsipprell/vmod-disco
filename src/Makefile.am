AM_CFLAGS = $(VARNISHAPI_CFLAGS) @ADNS_CPPFLAGS@ -Wall -Werror -Wunused-parameter -Wmissing-prototypes
AM_LDFLAGS = $(VMOD_LDFLAGS) @LDFLAGS_VERSION@

DEFS = @DEFS@ @ADDL_DEFS@

DEFAULT_INCLUDES = -I. -I$(top_srcdir) -I$(top_srcdir)/include

vmoddir = $(VARNISHAPI_VMODDIR)
vmod_LTLIBRARIES = libvmod_disco.la
notrans_man_MANS = vmod_disco.3

libvmod_disco_la_LIBADD = @ADNS_LIBS@
libvmod_disco_la_SOURCES = vdir.c vpridir.c vpridir.h update.c \
	bgthread.c event.c director.c disco.h $(top_srcdir)/include/atomic.h

nodist_libvmod_disco_la_SOURCES = vcc_disco_if.c vcc_disco_if.h
$(libvmod_disco_la_OBJECTS): vcc_disco_if.h
$(vmod_LTLIBRARIES): symbols.map

@BUILD_VMOD_DISCO@

EXTRA_DIST = $(top_srcdir)/src/*.vcc $(top_srcdir)/src/symbols.map

CLEANFILES = $(builddir)/vcc*if.c $(builddir)/vcc*if.h \
	$(builddir)/*.man.rst 	$(builddir)/*.3

SUFFIXES = .man.rst
