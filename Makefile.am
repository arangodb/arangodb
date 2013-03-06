# -*- mode: Makefile; -*-

## -----------------------------------------------------------------------------
## --SECTION--                                                             FILES
## -----------------------------------------------------------------------------

################################################################################
### @brief source to build before compile
################################################################################

BUILT_SOURCES = build_posix.h

################################################################################
### @brief man pages to install
################################################################################

dist_man_MANS =

################################################################################
### @brief files to cleanup
################################################################################

CLEANUP =

################################################################################
### @brief tragets for cleanup of 3rd party libraries
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

BUILT_SOURCES += etc/arangodb/arangod-uid.conf

etc/arangodb/arangod-uid.conf: @srcdir@/etc/arangodb/arangod.conf
	sed -e 's:# \([ug]id\):\1:' $< > $@

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
	@LIBEV_CPPFLAGS@ \
	@MATH_CPPFLAGS@ \
	@OPENSSL_CPPFLAGS@ \
	@READLINE_CPPFLAGS@ \
	@ICU_CPPFLAGS@ \
	@V8_CPPFLAGS@


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
	@LIBEV_LDFLAGS@ \
	@MATH_LDFLAGS@ \
	@OPENSSL_LDFLAGS@ \
	@READLINE_LDFLAGS@ \
	@ICU_LDFLAGS@ \
	@V8_LDFLAGS@

################################################################################
### @brief libraries
################################################################################

LIBS = \
	@LIBEV_LIBS@ \
	@MATH_LIBS@ \
	@OPENSSL_LIBS@ \
	@ICU_LIBS@ \
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
### @brief /bin programs
################################################################################

bin_PROGRAMS = \
	bin/arangosh \
	bin/arangoimp

if ENABLE_ARANGOB	
bin_PROGRAMS += bin/arangob
endif

################################################################################
### @brief /sbin programs
################################################################################

sbin_PROGRAMS = \
	bin/arangod

################################################################################
### @brief /sbin scripts
################################################################################

bin_SCRIPTS =

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

################################################################################
### @brief /var data
################################################################################

#if ENABLE_RELATIVE
#else

install-data-local:
	test -d $(DESTDIR)$(TRI_DATABASEDIR) || mkdir -p $(DESTDIR)$(TRI_DATABASEDIR)
	test -d $(DESTDIR)$(TRI_LOGDIR) || mkdir -p $(DESTDIR)$(TRI_LOGDIR)

#endif

## -----------------------------------------------------------------------------
## --SECTION--                                                           TARGETS
## -----------------------------------------------------------------------------

################################################################################
### @brief version number
################################################################################

build_posix.h: configure.ac
	@echo '#define TRIAGENS_VERSION "@PACKAGE_VERSION@"' > build_posix.h

################################################################################
### @brief local modifications
################################################################################

-include Installation/Makefile.local

################################################################################
### @brief source files
################################################################################

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

include UnitTests/Makefile.unittests

################################################################################
### @brief error code file
################################################################################

if ENABLE_MAINTAINER_MODE

BUILT_SOURCES += \
	@top_srcdir@/lib/BasicsC/voc-errors.h \
	@top_srcdir@/lib/BasicsC/voc-errors.c \
	@top_srcdir@/js/common/bootstrap/errors.js

@top_srcdir@/lib/BasicsC/voc-errors.h: lib/BasicsC/errors.dat
	@top_srcdir@/config/build_errorfile.sh @top_srcdir@/config/generateErrorfile.py lib/BasicsC/errors.dat @top_srcdir@/lib/BasicsC/voc-errors.h

@top_srcdir@/lib/BasicsC/voc-errors.c: lib/BasicsC/errors.dat
	@top_srcdir@/config/build_errorfile.sh @top_srcdir@/config/generateErrorfile.py lib/BasicsC/errors.dat @top_srcdir@/lib/BasicsC/voc-errors.c

@top_srcdir@/js/common/bootstrap/errors.js: lib/BasicsC/errors.dat
	@top_srcdir@/config/build_errorfile.sh @top_srcdir@/config/generateErrorfile.py lib/BasicsC/errors.dat js/common/bootstrap/errors.js

endif

################################################################################
## cleanup
################################################################################

distclean-local:
	@for i in $(CLEANUP_3RD); do  $(MAKE) $$i;  done
	rm -f $(BUILT_SOURCES)

clean-local:
	rm -rf $(CLEANUP)

################################################################################
### @brief generated files
################################################################################

.PHONY: built-sources

built-sources: \
	build_posix.h \
	@top_srcdir@/js/common/bootstrap/errors.js

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

include Documentation/Makefile.files

################################################################################
### @brief javascript
################################################################################

include js/Makefile.files

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
include lib/Makefile.bison
endif

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
