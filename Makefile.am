ACLOCAL_AMFLAGS = -I m4

AM_CFLAGS = 
AM_CPPFLAGS =
AM_LDFLAGS =
BUILT_SOURCES =
LIBS =

noinst_LIBRARIES = libavocadodb.a
bin_PROGRAMS = avocadodb

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

