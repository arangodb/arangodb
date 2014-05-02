noinst_LTLIBRARIES += libyajl.la

libyajl_la_SOURCES = \
	src/yajl/yajl_tree.h \
	src/yajl/yajl_parse.h \
	src/yajl/yajl_gen.h \
	src/yajl/yajl_common.h \
	src/yajl/yajl_version.h \
	src/yajl/yajl_parser.h \
	src/yajl/yajl_encode.h \
	src/yajl/yajl_buf.h \
	src/yajl/yajl_alloc.h \
	src/yajl/yajl_bytestack.h \
	src/yajl/yajl_lex.h \
	src/yajl/yajl_version.c \
	src/yajl/yajl.c \
	src/yajl/yajl_encode.c \
	src/yajl/yajl_lex.c \
	src/yajl/yajl_parser.c \
	src/yajl/yajl_buf.c \
	src/yajl/yajl_tree.c \
	src/yajl/yajl_alloc.c \
	src/yajl/yajl_gen.c

libyajl_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/bson \
	-DBSON_COMPILATION
libyajl_la_LIBADD = 
libyajl_la_LDFLAGS = 
