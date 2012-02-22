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

BUILT_SOURCES = 
CLEANUP =

sbin_PROGRAMS = avocado

nobase_pkgdata_DATA = \
	$(shell find @srcdir@/js/system -name "*.js" -print) \
	$(shell find @srcdir@/js/modules -name "*.js" -print) \
	$(shell find @srcdir@/html -name "*.css" -print) \
	$(shell find @srcdir@/html -name "*.gif" -print) \
	$(shell find @srcdir@/html -name "*.html" -print) \
	$(shell find @srcdir@/html -name "*.ico" -print) \
	$(shell find @srcdir@/html -name "*.js" -print) \
	$(shell find @srcdir@/html -name "*.png" -print)

install-data-local:
	test -d @localstatedir@/${PACKAGE_TARNAME} || mkdir -p @localstatedir@/${PACKAGE_TARNAME}

################################################################################
## avocadodb
################################################################################

include Makefile.files
include Makefile.doxygen
include Makefile.javascript

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
