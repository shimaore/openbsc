AM_CPPFLAGS = $(all_includes) -I$(top_srcdir)/include
AM_CFLAGS=-Wall -ggdb3 $(LIBOSMOCORE_CFLAGS) $(LIBOSMOGSM_CFLAGS)
AM_LDFLAGS = $(COVERAGE_LDFLAGS)

EXTRA_DIST = gbproxy_test.ok

noinst_PROGRAMS = gbproxy_test

gbproxy_test_SOURCES = gbproxy_test.c
gbproxy_test_LDADD = \
			$(top_builddir)/src/gprs/gb_proxy.o \
			$(top_builddir)/src/libcommon/libcommon.a \
			$(top_builddir)/src/libbsc/libbsc.a \
			$(top_builddir)/src/libtrau/libtrau.a \
			$(top_builddir)/src/libmsc/libmsc.a -ldbi \
			$(top_builddir)/src/libtrau/libtrau.a \
			$(LIBOSMOCORE_LIBS) $(LIBOSMOGB_LIBS) \
			$(LIBOSMOGSM_LIBS)  $(LIBOSMOVTY_LIBS) \
			$(LIBOSMOABIS_LIBS) $(LIBRARY_DL) \
			-lrt
