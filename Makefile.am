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

if ENABLE_FLEX

@srcdir@/%.c: @srcdir@/%.l
	@top_srcdir@/config/flex-c.sh $(LEX) `dirname $@` `basename $^ .l`

else

@srcdir@/%.c: @srcdir@/%.l
	@echo '+---------------------------------------------------------------+'
	@echo '| WARNING: required FLEX version not found, cannot update files |'
	@echo '+---------------------------------------------------------------+'

endif

################################################################################
## FLEX++
################################################################################

if ENABLE_FLEX

@srcdir@/%.cpp: @srcdir@/%.ll
	@top_srcdir@/config/flex-c++.sh $(LEX) `dirname $@` `basename $^ .ll`

else

@srcdir@/%.cpp: @srcdir@/%.ll
	@echo '+---------------------------------------------------------------+'
	@echo '| WARNING: required FLEX version not found, cannot update files |'
	@echo '+---------------------------------------------------------------+'

endif

################################################################################
## BISON
################################################################################

if ENABLE_BISON

@srcdir@/%.cpp: @srcdir@/%.yy
	@top_srcdir@/config/bison-c.sh $(BISON) `dirname $@` `basename $^ .yy`

else

@srcdir@/%.cpp: @srcdir@/%.yy
	@echo '+----------------------------------------------------------------+'
	@echo '| WARNING: required BISON version not found, cannot update files |'
	@echo '+----------------------------------------------------------------+'

endif

################################################################################
## BISON++
################################################################################

if ENABLE_BISON

@srcdir@/%.cpp: @srcdir@/%.yy
	@top_srcdir@/config/bison-c++.sh $(BISON) `dirname $@` `basename $^ .yy`

else

@srcdir@/%.cpp: @srcdir@/%.yy
	@echo '+----------------------------------------------------------------+'
	@echo '| WARNING: required BISON version not found, cannot update files |'
	@echo '+----------------------------------------------------------------+'

endif

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
