ACLOCAL_AMFLAGS = -I m4

AM_CFLAGS = 
AM_CXXFLAGS =
AM_CPPFLAGS = -D_SYSCONFDIR_='"${sysconfdir}"' -D_PKGDATADIR_='"${pkgdatadir}"' -D_DATABASEDIR_='"${localstatedir}/${PACKAGE_TARNAME}"'
AM_LDFLAGS =
BUILT_SOURCES = 
CLEANUP =
LIBS =

noinst_LIBRARIES = libavocadodb.a
bin_PROGRAMS = avocado

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
	test -d @localstatedir@/lib/avocado || mkdir -p @localstatedir@/lib/avocado

################################################################################
## avocado
################################################################################

include Makefile.files
include Makefile.doxygen
include Makefile.javascript

include Makefile.bison
include Makefile.flex

include Makefile.all-in-one

################################################################################
## cleanup
################################################################################

clean-local:
	if test "$(CLEANUP)" != "" ];  then rm -f $(CLEANUP); fi
