ACLOCAL_AMFLAGS = -I m4

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

AM_LDFLAGS = \
	@BOOST_LDFLAGS@ \
	@LIBEV_LDFLAGS@ \
	@MATH_LDFLAGS@ \
	@NCURSES_LDFLAGS@ \
	@OPENSSL_LDFLAGS@ \
	@READLINE_LDFLAGS@ \
	@V8_LDFLAGS@

LIBS = \
	@BOOST_LIBS@ \
	@LIBEV_LIBS@ \
	@MATH_LIBS@ \
	@NCURSES_LIBS@ \
	@OPENSSL_LIBS@ \
	@READLINE_LIBS@

BUILT_SOURCES = build.h
CLEANUP =

noinst_LIBRARIES = lib/libarango.a lib/libarango_v8.a lib/libarango_fe.a lib/libarango_client.a
sbin_PROGRAMS = bin/arangod
bin_PROGRAMS = bin/arangosh bin/arangoimp

if ENABLE_MRUBY
AM_CPPFLAGS += @MRUBY_CPPFLAGS@
AM_CPPFLAGS += @MRUBY_LDFLAGS@
LIBS += @MRUBY_LIBS@

noinst_LIBRARIES += lib/libarango_mruby.a
bin_PROGRAMS += bin/arangoirb
endif

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

if ENABLE_INSTALL_DBDIR
install-data-local:
	test -d @localstatedir@/${PACKAGE_TARNAME}/_ACTIONS || mkdir -p @localstatedir@/${PACKAGE_TARNAME}/_ACTIONS
	cp ${srcdir}/js/actions/demo/*.js @localstatedir@/${PACKAGE_TARNAME}/_ACTIONS
endif

################################################################################
## arangodb
################################################################################

build.h: configure.ac
	@echo '#define TRIAGENS_VERSION "@PACKAGE_VERSION@"' > build.h

include Makefile.files
include lib/Makefile.files
include arangod/Makefile.files
include arangosh/Makefile.files

if ENABLE_MRUBY
include arangoirb/Makefile.files
include m4/Makefile.mruby
endif

include m4/Makefile.doxygen
include m4/Makefile.javascript

if ENABLE_FLEX
include m4/Makefile.flex
endif

if ENABLE_BISON
include m4/Makefile.bison
endif

include Makefile.unittests
-include Makefile.local

.PHONY: errorfiles

if ENABLE_ERRORS_DEPENDENCY
errorfiles: lib/BasicsC/errors.dat
	@top_srcdir@/config/build_errorfile.sh @top_srcdir@/config/generateErrorfile.py lib/BasicsC/errors.dat lib/BasicsC/voc-errors.h
	@top_srcdir@/config/build_errorfile.sh @top_srcdir@/config/generateErrorfile.py lib/BasicsC/errors.dat lib/BasicsC/voc-errors.c
	@top_srcdir@/config/build_errorfile.sh @top_srcdir@/config/generateErrorfile.py lib/BasicsC/errors.dat js/common/bootstrap/errors.js

BUILT_SOURCES += \
	errorfiles
endif

if ENABLE_ALL_IN_ONE
include m4/Makefile.all-in-one-libev
include m4/Makefile.all-in-one-v8
endif

if ENABLE_MRUBY
include m4/Makefile.all-in-one-mruby
endif

################################################################################
## cleanup
################################################################################

clean-local:
	if test "$(CLEANUP)" != "" ];  then rm -f $(CLEANUP); fi
