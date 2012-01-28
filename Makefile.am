ACLOCAL_AMFLAGS = -I m4

AM_CFLAGS = 
AM_CPPFLAGS =
AM_LDFLAGS =
BUILT_SOURCES = .setup-directories
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

.setup-directories:
	@test -d js || mkdir js
	@test -d Doxygen/js || mkdir Doxygen/js
	@test -d Doxygen/js/system || mkdir Doxygen/js/system
	@test -d Doxygen/js/modules || mkdir Doxygen/js/modules
	@touch $@

js/js-%.h: @srcdir@/js/%.js .setup-directories
	@top_srcdir@/config/js2c.sh $< > $@

################################################################################
## FLEX
################################################################################

JsonParser/%.c: @srcdir@/JsonParser/%.l
	@top_srcdir@/config/flex-c.sh $(LEX) $@ $<

QL/%.c: @srcdir@/QL/%.l
	@top_srcdir@/config/flex-c.sh $(LEX) $@ $<

################################################################################
## FLEX++
################################################################################

V8/%.cpp: @srcdir@/V8/%.ll
	@top_srcdir@/config/flex-c++.sh $(LEX) $@ $<

JsonParserX/%.cpp: @srcdir@/JsonParserX/%.ll
	@top_srcdir@/config/flex-c++.sh $(LEX) $@ $<

################################################################################
## BISON
################################################################################

QL/%.c: @srcdir@/QL/%.y
	@top_srcdir@/config/bison-c.sh $(BISON) $@ $<

################################################################################
## BISON++
################################################################################

JsonParserX/%.cpp: @srcdir@/JsonParserX/%.yy
	@top_srcdir@/config/bison-c++.sh $(BISON) $@ $<

################################################################################
## Doxygen
################################################################################

.PHONY: doxygen Doxygen/js Doxygen/js/system Doxygen/js/modules

Doxygen/js/%.c: @srcdir@/js/%.js .setup-directories
	python @top_srcdir@/Doxygen/Scripts/js2doxy.py $< > $@

Doxygen/js/system/%.c: @srcdir@/js/system/%.js .setup-directories
	python @top_srcdir@/Doxygen/Scripts/js2doxy.py $< > $@

Doxygen/js/modules/%.c: @srcdir@/js/system/%.js .setup-directories
	python @top_srcdir@/Doxygen/Scripts/js2doxy.py $< > $@

doxygen: Doxygen/avocado.doxy $(DOXYGEN)
	doxygen Doxygen/avocado.doxy

################################################################################
## wiki
################################################################################

.PHONY: wiki

PANDOC = pandoc -f markdown -t markdown

wiki: $(WIKI)
	@test -d Doxygen/wiki || mkdir Doxygen/wiki
	for w in $(WIKI); do $(PANDOC) -o Doxygen/wiki/`basename $$w` $$w; done

################################################################################
## build information
################################################################################

.PHONY: BUILD_H_TARGET

BUILT_SOURCES += build.c
avocado_SOURCES += build.c

build.c: build.h
	@top_srcdir@/config/build_info.sh @srcdir@/build.info > $@

build.h: BUILD_H_TARGET

BUILD_H_TARGET:
	@test -f build.h || echo '#define TRIAGENS_VERSION "@PACKAGE_VERSION@"' > build.h
	@top_srcdir@/config/build_header.sh @srcdir@/build.info build.h

################################################################################
## cleanup
################################################################################

clean-local:
	rm -f $(BUILT_SOURCES)
	rm -f $(DOXYGEN)
	rm -f $(WIKI)
