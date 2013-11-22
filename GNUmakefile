# -*- mode: Makefile; -*-

## -----------------------------------------------------------------------------
## --SECTION--                                                   SPECIAL TARGETS
## -----------------------------------------------------------------------------

-include Makefile

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
	arangod/Ahuacatl/ahuacatl-tokens.c \
	arangod/Ahuacatl/ahuacatl-grammar.c \
	arangod/Ahuacatl/ahuacatl-grammar.h \
	lib/JsonParser/json-parser.c \
	lib/V8/v8-json.cpp \
	lib/V8/v8-json.h \
	lib/BasicsC/voc-errors.h \
	lib/BasicsC/voc-errors.c \
	js/common/bootstrap/errors.js \
	mr/common/bootstrap/mr-error.h

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
		--prefix=/opt/arangodb \
		--enable-all-in-one-icu \
		--enable-all-in-one-v8 \
		--enable-all-in-one-libev \
		--enable-mruby

	${MAKE} pack-dmg-cmake

pack-dmg-cmake:
	cd Build && cmake \
		-D "BUILD_PACKAGE=dmg-cli" \
		-D "CMAKE_INSTALL_PREFIX=${prefix}" \
		-D "USE_MRUBY=ON" \
		-D "USE_RAW_CONFIG=ON" \
		-D "ARANGODB_VERSION=@VERSION@" \
		-D "CPACK_PACKAGE_VERSION_MAJOR=${VERSION_MAJOR}" \
		-D "CPACK_PACKAGE_VERSION_MINOR=${VERSION_MINOR}" \
		-D "CPACK_PACKAGE_VERSION_PATCH=${VERSION_PATCH}" \
		..

	${MAKE} .libev-build-64
	${MAKE} .zlib-build-64
	${MAKE} .icu-build-64
	${MAKE} .v8-build-64
	${MAKE} .mruby-build-64

	${MAKE} ${BUILT_SOURCES}

	cd Build && ${MAKE}

	cd Build && cpack \
		-G Bundle \
		-D "CPACK_INSTALL_PREFIX=${DMG_NAME}/Contents/MacOS/opt/arangodb"

################################################################################
### @brief MacOSX
################################################################################

.PHONY: pack-macosx pack-macosx-cmake

PACK_DESTDIR ?= .

pack-macosx:
	rm -rf Build && mkdir Build

	./configure \
		--prefix=/opt/arangodb \
		--enable-all-in-one-icu \
		--enable-all-in-one-v8 \
		--enable-all-in-one-libev \
		--enable-mruby

	${MAKE} pack-macosx-cmake

pack-macosx-cmake:
	cd Build && cmake \
		-D "BUILD_PACKAGE=dmg-cli" \
		-D "CMAKE_INSTALL_PREFIX=${prefix}" \
		-D "USE_MRUBY=ON" \
		-D "USE_RAW_CONFIG=ON" \
		-D "ARANGODB_VERSION=@VERSION@" \
		-D "CPACK_PACKAGE_VERSION_MAJOR=${VERSION_MAJOR}" \
		-D "CPACK_PACKAGE_VERSION_MINOR=${VERSION_MINOR}" \
		-D "CPACK_PACKAGE_VERSION_PATCH=${VERSION_PATCH}" \
		..

	${MAKE} .libev-build-64
	${MAKE} .zlib-build-64
	${MAKE} .icu-build-64
	${MAKE} .v8-build-64
	${MAKE} .mruby-build-64

	${MAKE} ${BUILT_SOURCES}

	cd Build && ${MAKE}
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
		--localstatedir=/var \
		--enable-all-in-one-icu \
		--enable-all-in-one-v8 \
		--enable-all-in-one-libev \
		--disable-mruby

	${MAKE} pack-arm-cmake

pack-arm-cmake:
	cd Build && cmake \
		-D "BUILD_PACKAGE=raspbian" \
		-D "CMAKE_INSTALL_PREFIX=${prefix}" \
		-D "ETCDIR=${sysconfdir}" \
		-D "VARDIR=${localstatedir}" \
		-D "USE_MRUBY=OFF" \
		-D "USE_RAW_CONFIG=OFF" \
		-D "ARANGODB_VERSION=@VERSION@" \
		-D "CPACK_PACKAGE_VERSION_MAJOR=${VERSION_MAJOR}" \
		-D "CPACK_PACKAGE_VERSION_MINOR=${VERSION_MINOR}" \
		-D "CPACK_PACKAGE_VERSION_PATCH=${VERSION_PATCH}" \
                -D "V8_LIB_PATH=`pwd`/../3rdParty/V8/out/arm.release/obj.target/tools/gyp" \
                -D "CMAKE_CXX_FLAGS_RELEASE:STRING=-O2 -DNDEBUG" \
                -D "CMAKE_C_FLAGS_RELEASE:STRING=-O2 -DNDEBUG" \
		..

	${MAKE} .libev-build-32
	${MAKE} .zlib-build-32
	${MAKE} .icu-build-32
	${MAKE} .v8-build-32

	${MAKE} ${BUILT_SOURCES}

	cd Build && ${MAKE}

	cd Build && cpack \
		-G DEB

## -----------------------------------------------------------------------------
## --SECTION--                                                       END-OF-FILE
## -----------------------------------------------------------------------------

## Local Variables:
## mode: outline-minor
## outline-regexp: "^\\(### @brief\\|## --SECTION--\\|# -\\*- \\)"
## End:
