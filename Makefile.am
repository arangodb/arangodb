# -*- mode: Makefile; -*-

## -----------------------------------------------------------------------------
## --SECTION--                                                    COMMON DEFINES
## -----------------------------------------------------------------------------

comma := ,
empty :=
space := $(empty) $(empty)
MAJOR_MINOR := $(subst $(space),.,$(wordlist 1,2,$(subst ., ,$(VERSION))))

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

CLEANUP =

################################################################################
### @brief targets for cleanup of 3rd party libraries
################################################################################

CLEANUP_3RD =

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

## -----------------------------------------------------------------------------
## --SECTION--                                                             PATHS
## -----------------------------------------------------------------------------

################################################################################
### @brief program suffix
################################################################################

PROGRAM_SUFFIX=$(shell echo | sed '$(program_transform_name)')

################################################################################
### @brief system directory
################################################################################

if ENABLE_RELATIVE

TRI_PKGDATADIR = .
TRI_LOCALSTATEDIR = .
TRI_SBINDIR = ${abs_builddir}/bin
TRI_BINDIR = ${abs_builddir}/bin
TRI_LIBEXECDIR = ${abs_builddir}/bin
TRI_SYSCONFDIR = etc/relative

else

TRI_LOCALSTATEDIR = ${localstatedir}
TRI_PKGDATADIR = ${pkgdatadir}
TRI_SBINDIR = ${sbindir}
TRI_BINDIR = ${bindir}
TRI_LIBEXECDIR = ${libexecdir}
TRI_SYSCONFDIR = ${sysconfdir}/${PACKAGE_TARNAME}

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
	-DTRI_BUILD_DATE='"$(shell date +%Y-%m-%d)"' \
	-D_SYSCONFDIR_='"${TRI_SYSCONFDIR}"' \
	-I@top_srcdir@/lib \
	-I@top_builddir@/lib \
	@LIBEV_CPPFLAGS@ \
	@MATH_CPPFLAGS@ \
	@OPENSSL_CPPFLAGS@ \
	@READLINE_CPPFLAGS@ \
	@ICU_CPPFLAGS@ \
	@ZLIB_CPPFLAGS@ \
	@V8_CPPFLAGS@ 

################################################################################
### @brief linker flags
################################################################################

AM_LDFLAGS = \
	@LIBEV_LDFLAGS@ \
	@MATH_LDFLAGS@ \
	@OPENSSL_LDFLAGS@ \
	@READLINE_LDFLAGS@ \
	@ICU_LDFLAGS@ \
	@ZLIB_LDFLAGS@ \
	@V8_LDFLAGS@ 

################################################################################
### @brief libraries
################################################################################

LIBS = \
	@TCMALLOC_LIBS@ \
	@RT_LIBS@ \
	@LIBEV_LIBS@ \
	@MATH_LIBS@ \
	@OPENSSL_LIBS@ \
	@ICU_LIBS@ \
	@ZLIB_LIBS@ \
	@READLINE_LIBS@ 

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
	lib/libarango_client.a \
	arangod/libarangod.a

################################################################################
### @brief /bin programs
################################################################################

bin_PROGRAMS = \
	bin/arangob \
	bin/arangodump \
	bin/arangorestore \
	bin/arangosh \
	bin/arangoimp

################################################################################
### @brief /sbin programs
################################################################################

sbin_PROGRAMS = \
	bin/arangod

################################################################################
### @brief helper programs
################################################################################

noinst_bin =

################################################################################
### @brief /libexec programs
################################################################################

pkglibexec_SCRIPTS =

if ENABLE_ALL_IN_ONE_ETCD
pkglibexec_SCRIPTS += bin/etcd-arango$(PROGRAM_SUFFIX)

CLEANUP += bin/etcd-arango$(PROGRAM_SUFFIX)
endif

################################################################################
### @brief /bin scripts
################################################################################

bin_SCRIPTS =

################################################################################
### @brief uninstalled programs
################################################################################

noinst_PROGRAMS =

################################################################################
### @brief /etc data
################################################################################

arangosysconfdir=$(sysconfdir)/$(PACKAGE_TARNAME)

arangosysconf_DATA = $(shell find @builddir@/etc/arangodb -name "*$(PROGRAM_SUFFIX).conf" -print)

################################################################################
### @brief /share data
################################################################################

JS_FILE_LIST = $(shell @srcdir@/Installation/file-list-js.sh @srcdir@)

pkgdataJS1dir = $(datadir)/arangodb
pkgdataJS2dir = $(datadir)/arangodb
pkgdataJS3dir = $(datadir)/arangodb
pkgdataJS4dir = $(datadir)/arangodb
pkgdataJS5dir = $(datadir)/arangodb

nobase_pkgdataJS1_DATA = $(wordlist    1,   1000, $(JS_FILE_LIST))
nobase_pkgdataJS2_DATA = $(wordlist 1001,   2000, $(JS_FILE_LIST))
nobase_pkgdataJS3_DATA = $(wordlist 2001,   3000, $(JS_FILE_LIST))
nobase_pkgdataJS4_DATA = $(wordlist 3001,   4000, $(JS_FILE_LIST))
nobase_pkgdataJS5_DATA = $(wordlist 4001, 100000, $(JS_FILE_LIST))

nobase_pkgdata_DATA =

################################################################################
### @brief /var data
################################################################################

#if ENABLE_RELATIVE
#else

install-data-local:
	test -d $(DESTDIR)$(TRI_LOCALSTATEDIR)/lib/arangodb || mkdir -p $(DESTDIR)$(TRI_LOCALSTATEDIR)/lib/arangodb
	test -d $(DESTDIR)$(TRI_LOCALSTATEDIR)/lib/arangodb-apps || mkdir -p $(DESTDIR)$(TRI_LOCALSTATEDIR)/lib/arangodb-apps
	test -d $(DESTDIR)$(TRI_LOCALSTATEDIR)/log/arangodb || mkdir -p $(DESTDIR)$(TRI_LOCALSTATEDIR)/log/arangodb

#endif

################################################################################
### @brief symbolic links
################################################################################

install-exec-hook:
	rm -f $(DESTDIR)$(bindir)/foxx-manager$(PROGRAM_SUFFIX)
	$(LN_S) arangosh$(PROGRAM_SUFFIX) $(DESTDIR)$(bindir)/foxx-manager$(PROGRAM_SUFFIX)

	rm -f $(DESTDIR)$(sbindir)/arango-dfdb$(PROGRAM_SUFFIX)
	$(LN_S) arangod$(PROGRAM_SUFFIX) $(DESTDIR)$(sbindir)/arango-dfdb$(PROGRAM_SUFFIX)

## -----------------------------------------------------------------------------
## --SECTION--                                                           TARGETS
## -----------------------------------------------------------------------------

################################################################################
### @brief version number
################################################################################

build.h: configure.ac
	@echo '#define TRI_VERSION "@PACKAGE_VERSION@"' > build.h

################################################################################
### @brief source files
################################################################################

include lib/Makefile.files
include arangod/Makefile.files
include arangosh/Makefile.files
include etc/Makefile.files

################################################################################
### @brief unit tests
################################################################################

include UnitTests/Makefile.unittests

################################################################################
### @brief error code file
################################################################################

if ENABLE_MAINTAINER_MODE

BUILT_SOURCES += \
	@top_srcdir@/lib/Basics/voc-errors.h \
	@top_srcdir@/lib/Basics/voc-errors.cpp \
	@top_srcdir@/js/common/bootstrap/errors.js

@top_srcdir@/lib/Basics/voc-errors.h: lib/Basics/errors.dat
	@top_srcdir@/config/build_errorfile.sh @top_srcdir@/config/generateErrorfile.py lib/Basics/errors.dat @top_srcdir@/lib/Basics/voc-errors.h

@top_srcdir@/lib/Basics/voc-errors.cpp: lib/Basics/errors.dat
	@top_srcdir@/config/build_errorfile.sh @top_srcdir@/config/generateErrorfile.py lib/Basics/errors.dat @top_srcdir@/lib/Basics/voc-errors.cpp

@top_srcdir@/js/common/bootstrap/errors.js: lib/Basics/errors.dat
	@top_srcdir@/config/build_errorfile.sh @top_srcdir@/config/generateErrorfile.py lib/Basics/errors.dat js/common/bootstrap/errors.js

endif

################################################################################
### @brief mimetypes file
################################################################################

if ENABLE_MAINTAINER_MODE

BUILT_SOURCES += \
	@top_srcdir@/lib/Basics/voc-mimetypes.h \
	@top_srcdir@/lib/Basics/voc-mimetypes.cpp \
	@top_srcdir@/js/common/modules/org/arangodb/mimetypes.js

@top_srcdir@/lib/Basics/voc-mimetypes.h: lib/Basics/mimetypes.dat
	@top_srcdir@/config/build_mimetypes.sh @top_srcdir@/config/generateMimetypes.py lib/Basics/mimetypes.dat @top_srcdir@/lib/Basics/voc-mimetypes.h

@top_srcdir@/lib/Basics/voc-mimetypes.cpp: lib/Basics/mimetypes.dat
	@top_srcdir@/config/build_mimetypes.sh @top_srcdir@/config/generateMimetypes.py lib/Basics/mimetypes.dat @top_srcdir@/lib/Basics/voc-mimetypes.cpp

@top_srcdir@/js/common/modules/org/arangodb/mimetypes.js: lib/Basics/mimetypes.dat
	@top_srcdir@/config/build_mimetypes.sh @top_srcdir@/config/generateMimetypes.py lib/Basics/mimetypes.dat js/common/modules/org/arangodb/mimetypes.js

endif

################################################################################
## cleanup
################################################################################

distclean-local:
	@for i in $(CLEANUP_3RD); do  $(MAKE) $$i;  done
	rm -f $(BUILT_SOURCES)

clean-local:
	rm -rf $(CLEANUP)
	rm -rf `find . -name \*.gcno`
	rm -rf `find . -name \*.gcda`

superclean: distclean
	rm -rf arangod/*/.deps lib/*/.deps Makefile

################################################################################
### @brief generated files
################################################################################

.PHONY: built-sources

built-sources: \
	build.h \
	@top_srcdir@/js/common/bootstrap/errors.js \
	@top_srcdir@/js/common/modules/org/arangodb/mimetypes.js

################################################################################
### @brief tags file
################################################################################

GTAGS_FILES = $(sort $(abspath $(SOURCES) $(HEADERS)))

GTAGS:
	echo $(GTAGS_FILES) | tr " " "\n" | gtags -i -f -

################################################################################
### @brief cleanup source and header files
################################################################################

.PHONY: update-disclaimer

update-disclaimer:
	find lib arangod arangosh "(" -name "*.c" -o -name "*.h" -o -name "*.cpp" ")" -exec utils/cleanupCFiles "{}" ";"
	find lib arangod arangosh "(" -name "*.c" -o -name "*.h" -o -name "*.cpp" ")" -exec chmod "644" "{}" ";"
	find js/common/bootstrap js/client/bootstrap js/server/bootstrap -name "*.js" -a ! -name errors.js -exec utils/cleanupCFiles "{}" ";"
	find js/common/modules/org/arangodb js/client/modules/org/arangodb js/server/modules/org/arangodb -name "*.js"
	find js -name "*.js"  -exec chmod "644" "{}" ";"

## -----------------------------------------------------------------------------
## --SECTION--                                                      DEPENDENCIES
## -----------------------------------------------------------------------------

################################################################################
### @brief documentation
################################################################################

include Documentation/Makefile.files

################################################################################
### @brief javascript
################################################################################

include js/Makefile.files

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
	fgrep -v "[Build Status]" $< \
		| fgrep -v "ArangoDB-Logo" \
		| @MARKDOWN_EXEC@ \
		| @HTML2TEXT_EXEC@ -style pretty -nobs \
		| sed -e 's:&gt;:>:g' \
                | awk 'BEGIN { s = 0; } { if (length($0) == 0) {if (s != 0) print $0;} else {s = 1; print $0; }}' \
		> $@.tmp
	mv $@.tmp $@

endif
endif

################################################################################
### @brief generate all flex files
################################################################################

if ENABLE_MAINTAINER_MODE
include lib/Makefile.flex
endif

################################################################################
### @brief generate all bison files
################################################################################

if ENABLE_MAINTAINER_MODE

BUILT_SOURCES += $(BISON_FILES) $(BISONXX_FILES)

%.cpp: %.y
	@top_srcdir@/utils/bison-c.sh $(BISON) $@ $<

%.cpp: %.yy
	@top_srcdir@/utils/bison-c++.sh $(BISON) $@ $<

endif

## -----------------------------------------------------------------------------
## --SECTION--                                                EXTERNAL LIBRARIES
## -----------------------------------------------------------------------------


################################################################################
### @brief libev
################################################################################

include 3rdParty/Makefile.libev

################################################################################
### @brief V8
################################################################################

include 3rdParty/Makefile.v8

################################################################################
### @brief zlib
################################################################################

include 3rdParty/Makefile.zlib

################################################################################
### @brief etcd
################################################################################

if ENABLE_ALL_IN_ONE_ETCD
include 3rdParty/Makefile.etcd
endif

## -----------------------------------------------------------------------------
## --SECTION--                                                       CMAKE SETUP
## -----------------------------------------------------------------------------

################################################################################
### @brief cmake build
################################################################################

.PHONY: cmake-setup

cmake-setup:
	test -d Build || mkdir Build

	cd Build && cmake \
		-D "CMAKE_INSTALL_PREFIX=${prefix}" \
		-D "ETCDIR=${sysconfdir}" \
		-D "VARDIR=${localstatedir}" \
		\
		-D ARANGODB_VERSION=${VERSION} \
		\
		-D ICU_INCLUDE="${CMAKE_ICU_INCLUDE}" \
		-D ICU_LIB_PATH="${CMAKE_ICU_LIB_PATH}" \
		-D ICU_LIBS="${CMAKE_ICU_LIBS}" \
		-D ICU_VERSION="${CMAKE_ICU_VERSION}" \
		\
		-D LIBEV_INCLUDE="${CMAKE_LIBEV_INCLUDE}" \
		-D LIBEV_LIB_PATH="${CMAKE_LIBEV_LIB_PATH}" \
		-D LIBEV_LIBS="${CMAKE_LIBEV_LIBS}" \
		-D LIBEV_VERSION="${CMAKE_LIBEV_VERSION}" \
		\
		-D OPENSSL_INCLUDE= \
		-D OPENSSL_LIB_PATH= \
		-D OPENSSL_LIBS= \
		-D OPENSSL_VERSION= \
		\
		-D READLINE_INCLUDE= \
		-D READLINE_LIB_PATH= \
		-D READLINE_LIBS= \
		-D READLINE_VERSION= \
		\
		-D V8_INCLUDE= \
		-D V8_LIB_PATH= \
		-D V8_LIBS= \
		-D V8_VERSION= \
		\
		-D ZLIB_INCLUDE= \
		-D ZLIB_LIB_PATH= \
		-D ZLIB_LIBS= \
		-D ZLIB_VERSION= \
		\
		..

## -----------------------------------------------------------------------------
## --SECTION--                                                       END-OF-FILE
## -----------------------------------------------------------------------------

## Local Variables:
## mode: outline-minor
## outline-regexp: "^\\(### @brief\\|## --SECTION--\\|# -\\*- \\)"
## End:
