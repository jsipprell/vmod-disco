ACLOCAL_AMFLAGS = -I m4 -I ${VARNISHAPI_DATAROOTDIR}/aclocal

SUBDIRS = src

DISTCHECK_CONFIGURE_FLAGS = \
	VMOD_DIR=@VARNISHAPI_VMODDIR@

EXTRA_DIST = README.rst docs/*.rst include/vdir.h include/atomic.h LICENSE

DISTCLEANFILES = $(top_srcdir)/configure \
  $(top_srcdir)/docs/vmod_*.man.rst $(top_srcdir)/docs/vmod_*.rst 

doc_DATA = README.rst

