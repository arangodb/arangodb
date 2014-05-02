lib_LTLIBRARIES += libbson-1.0.la

libbson_1_0_la_SOURCES =
libbson_1_0_la_LIBADD = \
	libbson.la \
	libyajl.la
libbson_1_0_la_LDFLAGS = \
	$(OPTIMIZE_LDFLAGS) \
	$(BSON_LT_LDFLAGS) \
	-no-undefined \
	-export-symbols-regex "^(bson_|bcon_)" \
	-rpath $(libdir)

include src/bson/Makefile.am
include src/yajl/Makefile.am
include src/bindings/Makefile.am

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(top_builddir)/src/libbson-1.0.pc

EXTRA_DIST += \
	src/libbson.symbols \
	src/libbson-1.0.pc.in
