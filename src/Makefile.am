AM_CPPFLAGS = @VMOD_INCLUDES@ @ADNS_CPPFLAGS@ -Wall -Werror -Wunused-parameter -Wmissing-prototypes

DEFS = @DEFS@ @ADDL_DEFS@

DEFAULT_INCLUDES = -I. -I$(top_srcdir) -I$(top_srcdir)/include

vmoddir = @VMOD_DIR@
vmod_LTLIBRARIES = libvmod_disco.la
notrans_man_MANS = vmod_disco.3


libvmod_disco_la_LIBADD = @ADNS_LIBS@
libvmod_disco_la_LDFLAGS = $(AM_LDFLAGS) -module -export-dynamic -avoid-version -shared
libvmod_disco_la_SOURCES = vcc_disco_if.c vcc_disco_if.h vdir.c vpridir.c vpridir.h update.c \
	bgthread.c event.c director.c \
	disco.h

vcc_disco_if.c vcc_disco_if.h: @VMODTOOL@ $(top_srcdir)/src/vmod_disco.vcc
	@VMODTOOL@ -w ../docs/ -o vcc_disco_if $(top_srcdir)/src/vmod_disco.vcc

vmod_disco.man.rst: vcc_disco_if.c vcc_disco_if.h
	cp -f $(top_srcdir)/docs/$@ $@

%.3: %.man.rst
if HAVE_RST2MAN
	${RST2MAN} -r error $< $@
else
	@echo "========================================"
	@echo "You need rst2man installed to make dist"
	@echo "========================================"
	@false
endif

EXTRA_DIST = $(top_srcdir)/src/*.vcc

CLEANFILES = $(builddir)/vcc*if.c $(builddir)/vcc*if.h \
	$(builddir)/*.man.rst 	$(builddir)/*.3

SUFFIXES = .man.rst
