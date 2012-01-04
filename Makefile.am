ACLOCAL_AMFLAGS = -I m4

AM_CFLAGS = 
AM_CPPFLAGS =
AM_LDFLAGS =
BUILT_SOURCES =
LIBS =

noinst_LIBRARIES = libavocadodb.a
bin_PROGRAMS = avocado

################################################################################
## avocado
################################################################################

include Makefile.files

################################################################################
## FLEX
################################################################################

JsonParser/%.c: @srcdir@/JsonParser/%.l
	@top_srcdir@/config/flex-c.sh $(LEX) "$@" "$^"

################################################################################
## FLEX++
################################################################################

V8/%.cpp: @srcdir@/V8/%.ll
	@top_srcdir@/config/flex-c++.sh $(LEX) "$@" "$^"

################################################################################
## BISON++
################################################################################

JsonParserX/%.cpp: @srcdir@/JsonParserX/%.yy
	@top_srcdir@/config/bison-c++.sh $(BISON) "$@" "$^"

################################################################################
## build information
################################################################################

.PHONY: BUILD_H_TARGET

BUILT_SOURCES += @builddir@/build.c @srcdir@/build.h
avocado_SOURCES += @builddir@/build.c

@builddir@/build.c:
	@top_srcdir@/config/build_info.sh @srcdir@/build.info > $@

@srcdir@/build.h: BUILD_H_TARGET

BUILD_H_TARGET:
	@top_srcdir@/config/build_header.sh @srcdir@/build.info @srcdir@/build.h
