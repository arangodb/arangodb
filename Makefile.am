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

arangosysconfdir=$(sysconfdir)/$(PACKAGE_TARNAME)
databasedir=$(localstatedir)/$(PACKAGE_TARNAME)
logdir=$(localstatedir)/log/$(PACKAGE_TARNAME)

AM_CPPFLAGS = \
	-D_SYSCONFDIR_='"${arangosysconfdir}"' \
	-D_PKGDATADIR_='"${pkgdatadir}"' \
	-D_DATABASEDIR_='"${databasedir}"' \
	-I@top_srcdir@/lib \
	@BOOST_CPPFLAGS@ \
	@LIBEV_CPPFLAGS@ \
	@MATH_CPPFLAGS@ \
	@NCURSES_CPPFLAGS@ \
	@OPENSSL_CPPFLAGS@ \
	@READLINE_CPPFLAGS@ \
	@PROTOBUF_CPPFLAGS@ \
	@V8_CPPFLAGS@

if ENABLE_ZEROMQ

AM_CPPFLAGS += \
	@ZEROMQ_CPPFLAGS@

endif

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
	@PROTOBUF_LDFLAGS@ \
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
	@NCURSES_LIBS@ \
	@OPENSSL_LIBS@ \
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
	bin/arangod \
        bin/arango-password

# exclude arango-password from sources list
bin_arango_password_SOURCES =

################################################################################
### @brief /bin programs
################################################################################

bin_PROGRAMS = \
	bin/arangosh \
	bin/arangoimp

################################################################################
### @brief uninstalled programs
################################################################################

noinst_PROGRAMS =

if ENABLE_ZEROMQ

noinst_PROGRAMS += \
	bin/zclient \
	bin/zserver

endif

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

arangosysconf_DATA = Installation/arangod.conf

BUILT_SOURCES += Installation/arangod.conf

Installation/arangod.conf: Installation/arangod.conf.in Makefile
	sed \
		-e 's%@LOGDIR@%${logdir}%g' \
		-e 's%@DATABASE@%${databasedir}%g' \
		-e 's%@STATICFILES@%${pkgdatadir}%g' \
		$< > $@

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

if ENABLE_INSTALL_DBDIR

install-data-local:
	test -d @localstatedir@/${PACKAGE_TARNAME} || mkdir -p @localstatedir@/${PACKAGE_TARNAME}

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

include PerfTests/Makefile.files

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
### @brief generated files
################################################################################

.PHONY: built-sources

built-sources: build.h errorfiles $(JAVASCRIPT_HEADER)

################################################################################
### @brief arango password
################################################################################

bin/arango-password: bin/arango-password.in
if ENABLE_RELATIVE_DEVEL
	sed \
		-e 's%@SBINDIR@%bin%g' \
		-e 's%@DATABASE@%${databasedir}%g' \
		-e 's%@STATICFILES@%${pkgdatadir}%g' \
		$< > $@
	chmod 755 $@
else   
	sed \
		-e 's%@SBINDIR@%${DESTDIR}${sbindir}%g' \
		-e 's%@DATABASE@%${databasedir}%g' \
		-e 's%@STATICFILES@%${pkgdatadir}%g' \
		$< > $@
	chmod 755 $@
endif

CLEANUP += bin/arango-password

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

include m4/Makefile.all-in-one-protobuf
include m4/Makefile.protobuf

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
