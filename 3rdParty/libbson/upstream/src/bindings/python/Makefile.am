if ENABLE_PYTHON

pymoduledir = $(libdir)/python2.7/site-packages

pymodule_LTLIBRARIES = cbson.la

cbson_la_SOURCES = \
	src/bindings/python/cbson.c \
	src/bindings/python/cbson-dbref.c \
	src/bindings/python/cbson-dbref.h \
	src/bindings/python/cbson-oid.c \
	src/bindings/python/cbson-oid.h \
	src/bindings/python/cbson-util.c \
	src/bindings/python/cbson-util.h \
	src/bindings/python/time64.c \
	src/bindings/python/time64.h \
	src/bindings/python/time64_config.h \
	src/bindings/python/time64_limits.h

cbson_la_LIBADD = \
	libbson.la \
	libyajl.la \
	-l$(PYTHON_LIB)

cbson_la_CFLAGS = \
	-I$(top_srcdir)/src/bson \
	-I$(top_builddir)/src/bson \
	$(OPTIMIZE_CFLAGS) \
	$(MAINTAINER_CFLAGS)

cbson_la_LDFLAGS = -module

cbson_la_CPPFLAGS = -I$(PYTHON_INCLUDE_DIR)

endif
