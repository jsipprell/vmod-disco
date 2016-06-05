ACLOCAL_AMFLAGS = -I m4 -I ${LIBVARNISHAPI_DATAROOTDIR}/aclocal

SUBDIRS = src

DISTCHECK_CONFIGURE_FLAGS = \
	VMOD_DIR='$${libdir}/varnish/vmods'

EXTRA_DIST = README.rst docs/*.rst include/vdir.h include/_vdir.h include/atomic.h LICENSE

DISTCLEANFILES = $(top_srcdir)/configure \
  $(top_srcdir)/docs/vmod_*.man.rst $(top_srcdir)/docs/vmod_*.rst 

doc_DATA = README.rst

%.1 %.2 %.3 %.4 %.5 %.6 %.7 %.8 %.9:
if HAVE_RST2MAN
	${RST2MAN} $< $@
else
	@echo "========================================"
	@echo "You need rst2man installed to make dist"
	@echo "========================================"
	@false
endif
