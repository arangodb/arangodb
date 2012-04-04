ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = \
	-D_SYSCONFDIR_='"${sysconfdir}"' \
	-D_PKGDATADIR_='"${pkgdatadir}"' \
	-D_DATABASEDIR_='"${localstatedir}/${PACKAGE_TARNAME}"' \
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
	@READLINE_LIBS@ \
	@V8_LIBS@

BUILT_SOURCES = build.h
CLEANUP =

noinst_LIBRARIES = libavocado.a
sbin_PROGRAMS = avocado
bin_PROGRAMS = avocsh

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
## avocadodb
################################################################################

build.h: configure.ac
	@echo '#define TRIAGENS_VERSION "@PACKAGE_VERSION@"' > build.h

include Makefile.files
include Makefile.doxygen
include Makefile.javascript
include Makefile.unittests
-include Makefile.local

if ENABLE_FLEX
include Makefile.flex
endif

if ENABLE_BISON
include Makefile.bison
endif

if ENABLE_ALL_IN_ONE
include Makefile.all-in-one
endif

################################################################################
## cleanup
################################################################################

clean-local:
	if test "$(CLEANUP)" != "" ];  then rm -f $(CLEANUP); fi
