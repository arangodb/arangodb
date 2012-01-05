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
## JavaScript source code as header
################################################################################

js:
	mkdir js

js/js-%.h: @srcdir@/js/%.js js
	@top_srcdir@/config/js2c.sh $< > $@

################################################################################
## FLEX
################################################################################

JsonParser/%.c: @srcdir@/JsonParser/%.l
	@top_srcdir@/config/flex-c.sh $(LEX) $@ $<

################################################################################
## FLEX++
################################################################################

V8/%.cpp: @srcdir@/V8/%.ll
	@top_srcdir@/config/flex-c++.sh $(LEX) $@ $<

JsonParserX/%.cpp: @srcdir@/JsonParserX/%.ll
	@top_srcdir@/config/flex-c++.sh $(LEX) $@ $<

################################################################################
## BISON++
################################################################################

JsonParserX/%.cpp: @srcdir@/JsonParserX/%.yy
	@top_srcdir@/config/bison-c++.sh $(BISON) $@ $<

################################################################################
## Doxygen
################################################################################

.PHONY: doxygen

Doxygen/js:
	mkdir Doxygen/js

Doxygen/js/system:
	mkdir Doxygen/js/system

Doxygen/js/%.c: @srcdir@/js/%.js Doxygen/js
	python @top_srcdir@/config/js2doxy.py $< > $@

Doxygen/js/system/%.c: @srcdir@/js/system/%.js Doxygen/js/system
	python @top_srcdir@/config/js2doxy.py $< > $@

doxygen: Doxygen/avocado.doxy
	doxygen Doxygen/avocado.doxy

################################################################################
## build information
################################################################################

.PHONY: BUILD_H_TARGET

BUILT_SOURCES += @builddir@/build.c
avocado_SOURCES += @builddir@/build.c

@builddir@/build.c: @srcdir@/build.h
	@top_srcdir@/config/build_info.sh @srcdir@/build.info > $@

@srcdir@/build.h: BUILD_H_TARGET

BUILD_H_TARGET:
	@top_srcdir@/config/build_header.sh @srcdir@/build.info @srcdir@/build.h

################################################################################
## cleanup
################################################################################

clean-local:
	rm -f $(BUILT_SOURCES)
