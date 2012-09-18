# -*- mode: Makefile; -*-

## -----------------------------------------------------------------------------
## --SECTION--                                                             FILES
## -----------------------------------------------------------------------------

################################################################################
### @brief source to build before compile
################################################################################

BUILT_SOURCES = build.h

################################################################################
### @brief man pages to install
################################################################################

dist_man_MANS =

################################################################################
### @brief files to cleanup
################################################################################

CLEANUP = .setup-js-directories .setup-mr-directories

################################################################################
### @brief flex files
################################################################################

FLEX_FILES = 

################################################################################
### @brief flex++ files
################################################################################

FLEXXX_FILES = 

################################################################################
### @brief bison files
################################################################################

BISON_FILES =

################################################################################
### @brief bison++ files
################################################################################

BISONXX_FILES = 

################################################################################
### @brief protocol buffers files
################################################################################

PROTOBUF_FILES =

## -----------------------------------------------------------------------------
## --SECTION--                                                             PATHS
## -----------------------------------------------------------------------------

if ENABLE_RELATIVE

TRI_DATABASEDIR = 
TRI_LOGDIR = /tmp
TRI_PKGDATADIR = .
TRI_SBINDIR = ${abs_builddir}/bin

else

TRI_DATABASEDIR = ${localstatedir}/lib/${PACKAGE_TARNAME}
TRI_LOGDIR = ${localstatedir}/log/${PACKAGE_TARNAME}
TRI_PKGDATADIR = ${pkgdatadir}
TRI_SBINDIR = ${sbindir}

endif

## -----------------------------------------------------------------------------
## --SECTION--                                                             FLAGS
## -----------------------------------------------------------------------------

################################################################################
### @brief m4 directory
################################################################################

ACLOCAL_AMFLAGS = -I m4

################################################################################
### @brief preprocessor flags
################################################################################

AM_CPPFLAGS = \
	-I@top_srcdir@/lib \
	-I@top_builddir@/lib \
	@BOOST_CPPFLAGS@ \
	@LIBEV_CPPFLAGS@ \
	@MATH_CPPFLAGS@ \
	@OPENSSL_CPPFLAGS@ \
	@READLINE_CPPFLAGS@ \
	@ICU_CPPFLAGS@ \
	@PROTOBUF_CPPFLAGS@ \
	@V8_CPPFLAGS@


if ENABLE_ZEROMQ

AM_CPPFLAGS += \
	@ZEROMQ_CPPFLAGS@

endif


if ENABLE_RELATIVE

AM_CPPFLAGS += \
	-D_SYSCONFDIR_='"etc/relative"'


else

AM_CPPFLAGS += \
	-D_SYSCONFDIR_='"${sysconfdir}/${PACKAGE_TARNAME}"'

endif

################################################################################
### @brief linker flags
################################################################################

AM_LDFLAGS = \
	@BOOST_LDFLAGS@ \
	@LIBEV_LDFLAGS@ \
	@MATH_LDFLAGS@ \
	@OPENSSL_LDFLAGS@ \
	@READLINE_LDFLAGS@ \
	@PROTOBUF_LDFLAGS@ \
	@ICU_LDFLAGS@ \
	@V8_LDFLAGS@

if ENABLE_ZEROMQ

AM_LDFLAGS += \
	@ZEROMQ_LDFLAGS@

endif

################################################################################
### @brief libraries
################################################################################

LIBS = \
	@BOOST_LIBS@ \
	@LIBEV_LIBS@ \
	@MATH_LIBS@ \
	@OPENSSL_LIBS@ \
	@ICU_LIBS@ \
	@PROTOBUF_LIBS@ \
	@READLINE_LIBS@

################################################################################
### @brief ruby additions
################################################################################

if ENABLE_MRUBY

AM_CPPFLAGS += @MRUBY_CPPFLAGS@
AM_CPPFLAGS += @MRUBY_LDFLAGS@
LIBS += @MRUBY_LIBS@

endif

## -----------------------------------------------------------------------------
## --SECTION--                                              LIBRARIES & PROGRAMS
## -----------------------------------------------------------------------------

################################################################################
### @brief auxiliary libraries
################################################################################

noinst_LIBRARIES = \
	lib/libarango.a \
	lib/libarango_v8.a \
	lib/libarango_fe.a \
	lib/libarango_client.a

################################################################################
### @brief /sbin programs
################################################################################

sbin_PROGRAMS = \
	bin/arangod

################################################################################
### @brief /bin programs
################################################################################

bin_PROGRAMS = \
	bin/arangosh \
	bin/arangoimp \
	bin/arangob

################################################################################
### @brief uninstalled programs
################################################################################

noinst_PROGRAMS =

################################################################################
### @brief ruby additions
################################################################################

if ENABLE_MRUBY

noinst_LIBRARIES += lib/libarango_mruby.a
bin_PROGRAMS += bin/arangoirb

endif

################################################################################
### @brief /etc data
################################################################################

arangosysconfdir=$(sysconfdir)/$(PACKAGE_TARNAME)

arangosysconf_DATA = $(shell find @builddir@/etc/arangodb -name "*.conf" -print)

################################################################################
### @brief /share data
################################################################################

nobase_pkgdata_DATA = \
	$(shell find @srcdir@/js/actions -name "*.js" -print) \
	$(shell find @srcdir@/js/common -name "*.js" -print) \
	$(shell find @srcdir@/js/server -name "*.js" -print) \
	$(shell find @srcdir@/js/client -name "*.js" -print) \
	$(shell find @srcdir@/html -name "*.css" -print) \
	$(shell find @srcdir@/html -name "*.gif" -print) \
	$(shell find @srcdir@/html -name "*.html" -print) \
	$(shell find @srcdir@/html -name "*.ico" -print) \
	$(shell find @srcdir@/html -name "*.js" -print) \
	$(shell find @srcdir@/html -name "*.png" -print)

if ENABLE_MRUBY

nobase_pkgdata_DATA += \
	$(shell find @srcdir@/mr/actions/system -name "*.rb" -print) 

endif

## -----------------------------------------------------------------------------
## --SECTION--                                                           TARGETS
## -----------------------------------------------------------------------------

################################################################################
### @brief version number
################################################################################

build.h: configure.ac
	@echo '#define TRIAGENS_VERSION "@PACKAGE_VERSION@"' > build.h

################################################################################
### @brief local modifications
################################################################################

-include Installation/Makefile.local

################################################################################
### @brief source files
################################################################################

include Makefile.files
include lib/Makefile.files
include arangod/Makefile.files
include arangosh/Makefile.files
include etc/Makefile.files
include utils/Makefile.files

if ENABLE_MRUBY
include arangoirb/Makefile.files
endif

################################################################################
### @brief unit tests
################################################################################

include UnitTests/Makefile.files

################################################################################
### @brief error code file
################################################################################

.PHONY: errorfiles

if ENABLE_ERRORS_DEPENDENCY

errorfiles: lib/BasicsC/errors.dat
	@top_srcdir@/config/build_errorfile.sh @top_srcdir@/config/generateErrorfile.py lib/BasicsC/errors.dat lib/BasicsC/voc-errors.h
	@top_srcdir@/config/build_errorfile.sh @top_srcdir@/config/generateErrorfile.py lib/BasicsC/errors.dat lib/BasicsC/voc-errors.c
	@top_srcdir@/config/build_errorfile.sh @top_srcdir@/config/generateErrorfile.py lib/BasicsC/errors.dat js/common/bootstrap/errors.js

BUILT_SOURCES += \
	errorfiles

endif

################################################################################
## cleanup
################################################################################

clean-local:
	rm -f $(CLEANUP); 

################################################################################
### @brief generated files
################################################################################

.PHONY: built-sources

built-sources: build.h errorfiles $(JAVASCRIPT_HEADER)

################################################################################
### @brief tags file
################################################################################

GTAGS_FILES = $(sort $(abspath $(SOURCES) $(HEADERS)))

GTAGS:
	echo $(GTAGS_FILES) | tr " " "\n" | gtags -i -f -

## -----------------------------------------------------------------------------
## --SECTION--                                                      DEPENDENCIES
## -----------------------------------------------------------------------------

################################################################################
### @brief documentation
################################################################################

include Documentation/Makefile.doxygen

################################################################################
### @brief javascript
################################################################################

include js/Makefile.javascript

################################################################################
### @brief mruby
################################################################################

if ENABLE_MRUBY
include mr/Makefile.mruby
endif

## -----------------------------------------------------------------------------
## --SECTION--                                                   GENERATED FILES
## -----------------------------------------------------------------------------

################################################################################
### @brief generate readme
################################################################################

if ENABLE_MARKDOWN
if ENABLE_HTML2TEXT

BUILT_SOURCES += README

README: README.md
	@MARKDOWN_EXEC@ -o html $< | @HTML2TEXT_EXEC@ -style pretty | fgrep -v _Build_Status_ > $@.tmp
	mv $@.tmp $@

endif
endif

################################################################################
### @brief generate all flex files
################################################################################

if ENABLE_FLEX
include lib/Makefile.flex
endif

################################################################################
### @brief generate all bison files
################################################################################

if ENABLE_BISON
include lib/Makefile.bison
endif

################################################################################
### @brief generate all protocol buffers
################################################################################

include lib/Makefile.protobuf

## -----------------------------------------------------------------------------
## --SECTION--                                                EXTERNAL LIBRARIES
## -----------------------------------------------------------------------------

################################################################################
### @brief libev
################################################################################

if ENABLE_ALL_IN_ONE_LIBEV
include 3rdParty/Makefile.all-in-one-libev
endif

################################################################################
### @brief protcol buffers
################################################################################

if ENABLE_ALL_IN_ONE_PROTOBUF
include 3rdParty/Makefile.all-in-one-protobuf
endif

################################################################################
### @brief V8
################################################################################

if ENABLE_ALL_IN_ONE_V8
include 3rdParty/Makefile.all-in-one-v8
endif

################################################################################
### @brief mruby
################################################################################

if ENABLE_MRUBY
include 3rdParty/Makefile.all-in-one-mruby
endif

################################################################################
### @brief zeromq
################################################################################

if ENABLE_ZEROMQ
include 3rdParty/Makefile.all-in-one-zeromq
endif

################################################################################
### @brief ICU
################################################################################

if ENABLE_ALL_IN_ONE_ICU
include 3rdParty/Makefile.all-in-one-icu
endif

## -----------------------------------------------------------------------------
## --SECTION--                                                       END-OF-FILE
## -----------------------------------------------------------------------------

## Local Variables:
## mode: outline-minor
## outline-regexp: "^\\(### @brief\\|## --SECTION--\\|# -\\*- \\)"
## End:
