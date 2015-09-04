# -*- mode: Makefile; -*-

## -----------------------------------------------------------------------------
## --SECTION--                                                  COMMON VARIABLES
## -----------------------------------------------------------------------------

-include Makefile

VERSION_MAJOR := $(wordlist 1,1,$(subst ., ,$(VERSION)))
VERSION_MINOR := $(wordlist 2,2,$(subst ., ,$(VERSION)))
VERSION_PATCH := $(wordlist 3,3,$(subst ., ,$(VERSION)))

VERSION_PATCH := $(wordlist 1,1,$(subst -, ,$(VERSION_PATCH)))

## -----------------------------------------------------------------------------
## --SECTION--                                                   SPECIAL TARGETS
## -----------------------------------------------------------------------------

################################################################################
### @brief setup
################################################################################

.PHONY: setup

setup:
	@echo ACLOCAL
	@aclocal -I m4
	@echo AUTOMAKE
	@automake --add-missing --force-missing --copy
	@echo AUTOCONF
	@autoconf -I m4
	@echo auto system configured, proceed with configure

################################################################################
### @brief add maintainer files
################################################################################

MAINTAINER = \
	README \
	arangod/Aql/tokens.cpp \
	arangod/Aql/grammar.cpp \
	arangod/Aql/grammar.h \
	lib/JsonParser/json-parser.cpp \
	lib/V8/v8-json.cpp \
	lib/Basics/voc-errors.h \
	lib/Basics/voc-errors.cpp \
	js/common/bootstrap/errors.js

AUTOMAGIC = \
	Makefile.in \
	aclocal.m4 \
	configure \
	config/compile \
	config/config.guess \
	config/config.sub \
	config/depcomp \
	config/install-sh \
	config/missing

.PHONY: add-maintainer add-automagic

add-maintainer:
	@echo adding generated files to GIT
	git add -f $(MAINTAINER)

remove-maintainer:
	@echo removing generated files from GIT
	git rm -f $(MAINTAINER)

add-automagic:
	@echo adding automagic files to GIT
	git add -f $(AUTOMAGIC)

remove-automagic:
	@echo removing automagic files from GIT
	git rm -f $(AUTOMAGIC)

################################################################################
### @brief make love
################################################################################

love:
	@echo ArangoDB loves you

## -----------------------------------------------------------------------------
## --SECTION--                                                     CMAKE & CPACK
## -----------------------------------------------------------------------------

################################################################################
### @brief MacOSX bundle
################################################################################

.PHONY: pack-dmg pack-dmg-cmake

DMG_NAME := ArangoDB-CLI.app

pack-dmg:
	rm -rf Build && mkdir Build

	./configure \
		--prefix=/opt/arangodb

	${MAKE} pack-dmg-cmake

pack-dmg-cmake:
	cd Build && cmake \
		-D "ARANGODB_VERSION=${VERSION}" \
		-D "BUILD_PACKAGE=dmg-cli" \
		-D "CMAKE_INSTALL_PREFIX=${prefix}" \
		-D "CPACK_PACKAGE_VERSION_MAJOR=${VERSION_MAJOR}" \
		-D "CPACK_PACKAGE_VERSION_MINOR=${VERSION_MINOR}" \
		-D "CPACK_PACKAGE_VERSION_PATCH=${VERSION_PATCH}" \
		-D "LIBEV_VERSION=${LIBEV_VERSION}" \
		-D "READLINE_VERSION=${READLINE_VERSION}" \
		-D "READLINE_INCLUDE=${READLINE_INCLUDE}" \
		-D "READLINE_LIB_PATH=${READLINE_LIB_PATH}" \
		-D "V8_VERSION=${V8_VERSION}" \
		-D "ZLIB_VERSION=${ZLIB_VERSION}" \
		..

	${MAKE} .libev-build-64
	${MAKE} .zlib-build-64
	${MAKE} .v8-build-64

	${MAKE} ${BUILT_SOURCES}

	test -d bin || mkdir bin
	make bin/etcd-arango

	cd Build && ${MAKE}

	./Installation/file-copy-js.sh . Build

	cd Build && cpack \
		-G Bundle \
		-D "CPACK_INSTALL_PREFIX=${DMG_NAME}/Contents/MacOS/opt/arangodb"

################################################################################
### @brief MacOSXcode
################################################################################

.PHONY: pack-macosxcode pack-macosxcode-cmake

PACK_DESTDIR ?= .

pack-macosxcode:
	rm -rf Build && mkdir Build

	./configure \
		--prefix=/opt/arangodb

	${MAKE} -f GNUMakefile pack-macosxcode-cmake MOREOPTS='$(MOREOPTS)'

pack-macosxcode-cmake:
	cd Build && cmake \
		-D "ARANGODB_VERSION=${VERSION}" \
		-D "BUILD_PACKAGE=dmg-cli" \
		-D "CMAKE_INSTALL_PREFIX=${prefix}" \
		-D "CPACK_PACKAGE_VERSION_MAJOR=${VERSION_MAJOR}" \
		-D "CPACK_PACKAGE_VERSION_MINOR=${VERSION_MINOR}" \
		-D "CPACK_PACKAGE_VERSION_PATCH=${VERSION_PATCH}" \
		-D "LIBEV_VERSION=${LIBEV_VERSION}" \
		-D "READLINE_VERSION=${READLINE_VERSION}" \
		-D "READLINE_INCLUDE=${READLINE_INCLUDE}" \
		-D "READLINE_LIB_PATH=${READLINE_LIB_PATH}" \
		-D "V8_VERSION=${V8_VERSION}" \
		-D "ZLIB_VERSION=${ZLIB_VERSION}" \
		-G Xcode \
		$(MOREOPTS) \
		..

	./Installation/file-copy-js.sh . Build


################################################################################
### @brief MacOSX
################################################################################

.PHONY: pack-macosx pack-macosx-cmake

PACK_DESTDIR ?= .

pack-macosx:
	rm -rf Build && mkdir Build

	./configure \
		--prefix=/opt/arangodb

	${MAKE} pack-macosx-cmake MOREOPTS='$(MOREOPTS)'

pack-macosx-cmake:
	cd Build && cmake \
		-D "ARANGODB_VERSION=${VERSION}" \
		-D "BUILD_PACKAGE=dmg-cli" \
		-D "CMAKE_INSTALL_PREFIX=${prefix}" \
		-D "CPACK_PACKAGE_VERSION_MAJOR=${VERSION_MAJOR}" \
		-D "CPACK_PACKAGE_VERSION_MINOR=${VERSION_MINOR}" \
		-D "CPACK_PACKAGE_VERSION_PATCH=${VERSION_PATCH}" \
		-D "LIBEV_VERSION=${LIBEV_VERSION}" \
		-D "READLINE_VERSION=${READLINE_VERSION}" \
		-D "READLINE_INCLUDE=${READLINE_INCLUDE}" \
		-D "READLINE_LIB_PATH=${READLINE_LIB_PATH}" \
		-D "V8_VERSION=${V8_VERSION}" \
		-D "ZLIB_VERSION=${ZLIB_VERSION}" \
		$(MOREOPTS) \
		..

	${MAKE} .libev-build-64
	${MAKE} .zlib-build-64
	${MAKE} .v8-build-64

	${MAKE} ${BUILT_SOURCES}

	test -d bin || mkdir bin
	make bin/etcd-arango

	cd Build && ${MAKE}

	./Installation/file-copy-js.sh . Build

	cd Build && ${MAKE} install DESTDIR=${PACK_DESTDIR}

################################################################################
### @brief debian arm package
################################################################################

.PHONY: pack-arm pack-arm-cmake

pack-arm:
	rm -rf Build && mkdir Build

	./configure \
		--prefix=/usr \
		--sysconfdir=/etc \
		--localstatedir=/var

	touch .libev-build-32
	touch .v8-build-32
	touch .zlib-build-32

	${MAKE} pack-arm-cmake

pack-arm-cmake:
	cd Build && cmake \
		-D "ARANGODB_VERSION=${VERSION}" \
		-D "BUILD_PACKAGE=raspbian" \
		-D "CMAKE_CXX_FLAGS_RELEASE:STRING=-O2 -DNDEBUG" \
		-D "CMAKE_C_FLAGS_RELEASE:STRING=-O2 -DNDEBUG" \
		-D "CMAKE_INSTALL_PREFIX=${prefix}" \
		-D "CPACK_PACKAGE_VERSION_MAJOR=${VERSION_MAJOR}" \
		-D "CPACK_PACKAGE_VERSION_MINOR=${VERSION_MINOR}" \
		-D "CPACK_PACKAGE_VERSION_PATCH=${VERSION_PATCH}" \
		-D "ETCDIR=${sysconfdir}" \
		-D "LIBEV_VERSION=${LIBEV_VERSION}" \
		-D "READLINE_VERSION=${READLINE_VERSION}" \
		-D "V8_VERSION=${V8_VERSION}" \
		-D "VARDIR=${localstatedir}" \
		-D "ZLIB_VERSION=${ZLIB_VERSION}" \
		$(MOREOPTS) \
		..

	${MAKE} ${BUILT_SOURCES}

	cd Build && ${MAKE}

	./Installation/file-copy-js.sh . Build

	cd Build && cpack -G DEB


pack-deb-cmake:
	mkdir Build
	cd Build && cmake \
		-D "ARANGODB_VERSION=${VERSION}" \
		-D "CMAKE_CXX_FLAGS_RELEASE:STRING=-O2 -DNDEBUG" \
		-D "CMAKE_C_FLAGS_RELEASE:STRING=-O2 -DNDEBUG" \
		-D "CMAKE_INSTALL_PREFIX=${prefix}" \
		-D "CPACK_PACKAGE_VERSION_MAJOR=${VERSION_MAJOR}" \
		-D "CPACK_PACKAGE_VERSION_MINOR=${VERSION_MINOR}" \
		-D "CPACK_PACKAGE_VERSION_PATCH=${VERSION_PATCH}" \
		-D "ETCDIR=${sysconfdir}" \
		-D "LIBEV_VERSION=${LIBEV_VERSION}" \
		-D "READLINE_VERSION=${READLINE_VERSION}" \
		-D "V8_VERSION=${V8_VERSION}" \
		-D "VARDIR=${localstatedir}" \
		-D "ZLIB_VERSION=${ZLIB_VERSION}" \
		$(MOREOPTS) \
		..

	${MAKE} ${BUILT_SOURCES}

	cd Build && ${MAKE}

	./Installation/file-copy-js.sh . Build

	cd Build && cpack -G DEB

################################################################################
### @brief Windows 64-bit bundle
################################################################################

.PHONY: pack-win32 pack-winXX winXX-cmake

pack-win32:
	$(MAKE) pack-winXX BITS=32 TARGET="Visual Studio 12"

pack-win64:
	$(MAKE) pack-winXX BITS=64 TARGET="Visual Studio 12 Win64"

pack-win32-relative:
	$(MAKE) pack-winXX BITS=32 TARGET="Visual Studio 12" MOREOPTS='-D "USE_RELATIVE=ON" -D "USE_MAINTAINER_MODE=ON" -D "USE_BACKTRACE=ON"'

pack-win64-relative:
	$(MAKE) pack-winXX BITS=64 TARGET="Visual Studio 12 Win64" MOREOPTS='-D "USE_RELATIVE=ON" -D "USE_MAINTAINER_MODE=ON" -D "USE_BACKTRACE=ON"'

win64-relative:
	$(MAKE) winXX-cmake BITS=64 TARGET="Visual Studio 12 Win64" MOREOPTS='-D "USE_RELATIVE=ON" -D "USE_MAINTAINER_MODE=ON" -D "USE_BACKTRACE=ON"'
	$(MAKE) winXX-build BITS=64

pack-winXX:
	rm -rf Build$(BITS) && mkdir Build$(BITS)

	${MAKE} winXX-cmake BITS="$(BITS)" TARGET="$(TARGET)" VERSION="`awk '{print substr($$3,2,length($$3)-2);}' build.h`"
	${MAKE} winXX-build BITS="$(BITS)" TARGET="$(TARGET)" BUILD_TARGET=RelWithDebInfo
	${MAKE} packXX BITS="$(BITS)"

pack-winXX-MOREOPTS:
	rm -rf Build$(BITS) && mkdir Build$(BITS)

	${MAKE} winXX-cmake BITS="$(BITS)" TARGET="$(TARGET)" VERSION="`awk '{print substr($$3,2,length($$3)-2);}' build.h`" MOREOPTS=$(MOREOPTS)
	${MAKE} winXX-build BITS="$(BITS)" TARGET="$(TARGET)" BUILD_TARGET=Debug
	${MAKE} packXX BITS="$(BITS)" TARGET="$(TARGET)" BUILD_TARGET=Debug

winXX-cmake: checkcmake
	cd Build$(BITS) && cmake \
		-G "$(TARGET)" \
		-D "ARANGODB_VERSION=${VERSION}" \
		-D "CMAKE_BUILD_TYPE=RelWithDebInfo" \
		-D "BUILD_TYPE=RelWithDebInfo" \
		-D "CPACK_PACKAGE_VERSION_MAJOR=${VERSION_MAJOR}" \
		-D "CPACK_PACKAGE_VERSION_MINOR=${VERSION_MINOR}" \
		-D "CPACK_PACKAGE_VERSION_PATCH=${VERSION_PATCH}" \
		-D "LIBEV_VERSION=4.11" \
		-D "V8_VERSION=4.3.61" \
		-D "ZLIB_VERSION=1.2.7" \
		-D "BUILD_ID=${BUILD_ID}" \
		$(MOREOPTS) \
		..

winXX-build:
	cp Installation/Windows/Icons/arangodb.ico Build$(BITS) 
	cd Build$(BITS) && cmake --build . --config $(BUILD_TARGET)

packXX:
	./Installation/file-copy-js.sh . Build$(BITS)

	cd Build$(BITS) && cp -a bin/RelWithDebInfo bin/Release
	cd Build$(BITS) && cpack -G NSIS -D "BUILD_TYPE=RelWithDebInfo"
	cd Build$(BITS) && cpack -G ZIP -D "BUILD_TARGET=RelWithDebInfo"

	./Installation/Windows/installer-generator.sh $(BITS) $(shell pwd)

checkcmake:
	if test -z "`cmake --help |grep -i visual`"; then \
		echo "Your cmake is not sufficient; it lacks support for visual studio." ; \
		exit 1; \
	fi


################################################################################
### @brief generates a tar archive
################################################################################

.PHONY: pack-tar pack-tar-config

pack-tar-config:
	./configure \
		--prefix=/usr \
		--sysconfdir=/etc \
		--localstatedir=/var

pack-tar:
	rm -rf /tmp/pack-arangodb
	make install-strip DESTDIR=/tmp/pack-arangodb
	tar -c -v -z -f arangodb-$(VERSION).tar.gz -C /tmp/pack-arangodb .

## -----------------------------------------------------------------------------
## --SECTION--                                                       END-OF-FILE
## -----------------------------------------------------------------------------

## Local Variables:
## mode: outline-minor
## outline-regexp: "### @brief\\|## --SECTION--\\|# -\\*-"
## End:
