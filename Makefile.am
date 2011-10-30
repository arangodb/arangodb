################################################################################
## Makefile.am for AvocadoDB
################################################################################

ACLOCAL_AMFLAGS = -I m4
AM_CFLAGS = 
AM_CPPFLAGS = @V8_CPPFLAGS@
AM_LDFLAGS = @V8_LDFLAGS@
BUILT_SOURCES =
LIBS = @V8_LIBS@ -lreadline
BUILD_H = $(top_srcdir)/build.h

################################################################################
## avocdb program
################################################################################

bin_PROGRAMS = avocdb

avocdb_SOURCES = \
	Basics/associative.c \
	Basics/conversions.c \
	Basics/csv.c \
	Basics/error.c \
	Basics/files.c \
	Basics/hashes.c \
	Basics/init.c \
	Basics/json.c \
	Basics/locks-posix.c \
	Basics/logging.c \
	Basics/memory.c \
	Basics/random.c \
	Basics/string-buffer.c \
	Basics/strings.c \
	Basics/structures.c \
	Basics/system-functions.c \
	Basics/system-windows.c \
	Basics/threads-posix.c \
	Basics/vector.c \
	GeoIndex/GeoIndex.c \
	JsonParser/json-parser.c \
	ShapedJson/json-shaper.c \
	ShapedJson/shape-accessor.c \
	ShapedJson/shaped-json.c \
	V8/v8.cpp \
	V8/v8-json.cpp \
	V8/v8-shell.cpp \
	V8/v8-vocbase.cpp \
	VocBase/blob-collection.c \
	VocBase/collection.c \
	VocBase/datafile.c \
	VocBase/document-collection.c \
	VocBase/garbage-collector.c \
	VocBase/headers.c \
	VocBase/index.c \
	VocBase/query.c \
	VocBase/result-set.c \
	VocBase/simple-collection.c \
	VocBase/vocbase.c \
	VocBase/voc-shaper.c \
	# end of file list
