# -*- mode: Makefile; -*-

################################################################################
## --SECTION--                                                 FLAGS & VARIABLES
################################################################################

################################################################################
### @brief m4 directory
################################################################################

ACLOCAL_AMFLAGS = -I m4

################################################################################
### @brief source to build before compile
################################################################################

BUILT_SOURCES = build.h

################################################################################
### @brief files to cleanup
################################################################################

CLEANUP =

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

################################################################################
### @brief preprocessor flags
################################################################################

AM_CPPFLAGS = \
	-D_SYSCONFDIR_='"${sysconfdir}"' \
	-D_PKGDATADIR_='"${pkgdatadir}"' \
	-D_DATABASEDIR_='"${localstatedir}/${PACKAGE_TARNAME}"' \
	-I@top_srcdir@/lib \
	@BOOST_CPPFLAGS@ \
	@LIBEV_CPPFLAGS@ \
	@MATH_CPPFLAGS@ \
	@NCURSES_CPPFLAGS@ \
	@OPENSSL_CPPFLAGS@ \
	@READLINE_CPPFLAGS@ \
	@V8_CPPFLAGS@

################################################################################
### @brief linker flags
################################################################################

AM_LDFLAGS = \
	@BOOST_LDFLAGS@ \
	@LIBEV_LDFLAGS@ \
	@MATH_LDFLAGS@ \
	@NCURSES_LDFLAGS@ \
	@OPENSSL_LDFLAGS@ \
	@READLINE_LDFLAGS@ \
	@V8_LDFLAGS@

################################################################################
### @brief libraries
################################################################################

LIBS = \
	@BOOST_LIBS@ \
	@LIBEV_LIBS@ \
	@MATH_LIBS@ \
	@NCURSES_LIBS@ \
	@OPENSSL_LIBS@ \
	@READLINE_LIBS@

################################################################################
### @brief ruby additions
################################################################################

if ENABLE_MRUBY
AM_CPPFLAGS += @MRUBY_CPPFLAGS@
AM_CPPFLAGS += @MRUBY_LDFLAGS@
LIBS += @MRUBY_LIBS@
endif

################################################################################
## --SECTION--                                              LIBRARIES & PROGRAMS
################################################################################

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
	bin/arangoimp

################################################################################
### @brief ruby additions
################################################################################

if ENABLE_MRUBY

noinst_LIBRARIES += lib/libarango_mruby.a
bin_PROGRAMS += bin/arangoirb

endif

################################################################################
### @brief /share data
################################################################################

nobase_pkgdata_DATA = \
	$(shell find @srcdir@/js/actions/system -name "*.js" -print) \
	$(shell find @srcdir@/js/common/modules -name "*.js" -print) \
	$(shell find @srcdir@/js/server/modules -name "*.js" -print) \
	$(shell find @srcdir@/js/client/modules -name "*.js" -print) \
	$(shell find @srcdir@/html -name "*.css" -print) \
	$(shell find @srcdir@/html -name "*.gif" -print) \
	$(shell find @srcdir@/html -name "*.html" -print) \
	$(shell find @srcdir@/html -name "*.ico" -print) \
	$(shell find @srcdir@/html -name "*.js" -print) \
	$(shell find @srcdir@/html -name "*.png" -print)

################################################################################
### @brief /var data
################################################################################

if ENABLE_INSTALL_DBDIR
install-data-local:
	test -d @localstatedir@/${PACKAGE_TARNAME}/_ACTIONS || mkdir -p @localstatedir@/${PACKAGE_TARNAME}/_ACTIONS
	cp ${srcdir}/js/actions/demo/*.js @localstatedir@/${PACKAGE_TARNAME}/_ACTIONS
endif

################################################################################
## --SECTION--                                                           TARGETS
################################################################################

################################################################################
### @brief version number
################################################################################

build.h: configure.ac
	@echo '#define TRIAGENS_VERSION "@PACKAGE_VERSION@"' > build.h

################################################################################
### @brief local modifications
################################################################################

-include Makefile.local

################################################################################
### @brief source files
################################################################################

include Makefile.files
include lib/Makefile.files
include arangod/Makefile.files
include arangosh/Makefile.files

if ENABLE_MRUBY
include arangoirb/Makefile.files
endif

################################################################################
### @brief unit tests
################################################################################

include Makefile.unittests

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
	if test "$(CLEANUP)" != "" ];  then rm -f $(CLEANUP); fi

################################################################################
## --SECTION--                                                      DEPENDENCIES
################################################################################

################################################################################
### @brief mruby
################################################################################

if ENABLE_MRUBY
include m4/Makefile.mruby
endif

################################################################################
### @brief documentation
################################################################################

include m4/Makefile.doxygen

################################################################################
### @brief javascript
################################################################################

include m4/Makefile.javascript

################################################################################
### @brief flex
################################################################################

if ENABLE_FLEX
include m4/Makefile.flex
endif

################################################################################
### @brief bison
################################################################################

if ENABLE_BISON
include m4/Makefile.bison
endif

################################################################################
### @brief protocol buffers
################################################################################

if ENABLE_PROTOBUF_DEPENDENCY
include m4/Makefile.protobuf
endif

################################################################################
## --SECTION--                                                EXTERNAL LIBRARIES
################################################################################

################################################################################
### @brief libev and V8
################################################################################

if ENABLE_ALL_IN_ONE
include m4/Makefile.all-in-one-libev
include m4/Makefile.all-in-one-v8
endif

################################################################################
### @brief mruby
################################################################################

if ENABLE_MRUBY
include m4/Makefile.all-in-one-mruby
endif

################################################################################
### @brief zeromq
################################################################################

if ENABLE_ZEROMQ
include m4/Makefile.all-in-one-zeromq
endif

################################################################################
## --SECTION--                                                       END-OF-FILE
################################################################################

## Local Variables:
## mode: outline-minor
## outline-regexp: "^\\(### @brief\\|## --SECTION--\\|# -\\*- \\)"
## End:
