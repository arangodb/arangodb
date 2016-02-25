# -*- mode: Makefile; -*-

## -----------------------------------------------------------------------------
## --SECTION--                                                  COMMON VARIABLES
## -----------------------------------------------------------------------------

.PHONY: warning

warning:
	@echo "ArangoDB has switch to CMAKE. In order to compile, use:"
	@echo ""
	@echo "  mkdir build"
	@echo "  cd build"
	@echo "  cmake .."
	@echo "  make"
	@echo ""
	@echo MacOS users:""
	@echo "  Please use OPENSSL from homebrew and use"
	@echo ""
	@echo "    cmake .. -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl"
	@echo ""
	@echo "  Note that some versions of Apple's clang have severe performance"
	@echo "  issues. Use GCC5 from homebrew in this case."

VERSION_MAJOR := $(wordlist 1,1,$(subst ., ,$(VERSION)))
VERSION_MINOR := $(wordlist 2,2,$(subst ., ,$(VERSION)))
VERSION_PATCH := $(wordlist 3,3,$(subst ., ,$(VERSION)))
VERSION_PATCH := $(wordlist 1,1,$(subst -, ,$(VERSION_PATCH)))

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
		CPPFLAGS="-I`brew --prefix`/opt/openssl/include" \
		LDFLAGS="-L`brew --prefix`/opt/openssl/lib"

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
		-D "OPENSSL_INCLUDE=`brew --prefix`/opt/openssl/include" \
		-D "OPENSSL_LIB_PATH=`brew --prefix`/opt/openssl/lib" \
		-D "OPENSSL_LIBS=`brew --prefix`/opt/openssl/lib/libssl.a;`brew --prefix`/opt/openssl/lib/libcrypto.a" \
		-D "V8_VERSION=${V8_VERSION}" \
		-D "ZLIB_VERSION=${ZLIB_VERSION}" \
		..

	${MAKE} .libev-build-64
	${MAKE} .zlib-build-64
	${MAKE} .v8-build-64

	${MAKE} ${BUILT_SOURCES}

	test -d bin || mkdir bin
	make bin/etcd-arango

	rm -f ./.file-list-js
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
		--prefix=/opt/arangodb \
		CPPFLAGS="-I`brew --prefix`/opt/openssl/include" \
		LDFLAGS="-L`brew --prefix`/opt/openssl/lib"

	${MAKE} -f GNUMakefile pack-macosxcode-cmake MOREOPTS='$(MOREOPTS)'

pack-macosxcode-cmake:
	rm -f ./.file-list-js
	cd Build && cmake \
		-D "ARANGODB_VERSION=${VERSION}" \
		-D "BUILD_PACKAGE=dmg-cli" \
		-D "CMAKE_INSTALL_PREFIX=${prefix}" \
		-D "CPACK_PACKAGE_VERSION_MAJOR=${VERSION_MAJOR}" \
		-D "CPACK_PACKAGE_VERSION_MINOR=${VERSION_MINOR}" \
		-D "CPACK_PACKAGE_VERSION_PATCH=${VERSION_PATCH}" \
		-D "LIBEV_VERSION=${LIBEV_VERSION}" \
		-D "OPENSSL_INCLUDE=`brew --prefix`/opt/openssl/include" \
		-D "OPENSSL_LIB_PATH=`brew --prefix`/opt/openssl/lib" \
		-D "OPENSSL_LIBS=`brew --prefix`/opt/openssl/lib/libssl.a;`brew --prefix`/opt/openssl/lib/libcrypto.a" \
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
		--prefix=/opt/arangodb \
		CPPFLAGS="-I`brew --prefix`/opt/openssl/include" \
		LDFLAGS="-L`brew --prefix`/opt/openssl/lib"

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
		-D "OPENSSL_INCLUDE=`brew --prefix`/opt/openssl/include" \
		-D "OPENSSL_LIB_PATH=`brew --prefix`/opt/openssl/lib" \
		-D "OPENSSL_LIBS=`brew --prefix`/opt/openssl/lib/libssl.a;`brew --prefix`/opt/openssl/lib/libcrypto.a" \
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

	rm -f ./.file-list-js
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
		-D "V8_VERSION=${V8_VERSION}" \
		-D "VARDIR=${localstatedir}" \
		-D "ZLIB_VERSION=${ZLIB_VERSION}" \
		$(MOREOPTS) \
		..

	${MAKE} ${BUILT_SOURCES}

	rm -f ./.file-list-js
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
		-D "V8_VERSION=${V8_VERSION}" \
		-D "VARDIR=${localstatedir}" \
		-D "ZLIB_VERSION=${ZLIB_VERSION}" \
		$(MOREOPTS) \
		..

	${MAKE} .libev-build-64
	${MAKE} .zlib-build-64
	${MAKE} .v8-build-64

	${MAKE} ${BUILT_SOURCES}
	${MAKE} ${BUILT_SOURCES}

	rm -f ./.file-list-js
	cd Build && ${MAKE}

	./Installation/file-copy-js.sh . Build

	cd Build && cpack -G DEB

################################################################################
### @brief Windows 64-bit bundle
################################################################################

.PHONY: pack-win32 pack-winXX winXX-cmake win64-relative win64-relative-debug

pack-win32: 
	$(MAKE) pack-winXX BITS=32 TARGET="Visual Studio 12"

pack-win64:
	$(MAKE) pack-winXX BITS=64 TARGET="Visual Studio 12 Win64"

pack-win32-relative:
	$(MAKE) pack-winXX BITS=32 TARGET="Visual Studio 12" MOREOPTS='-D "USE_RELATIVE=ON" -D "USE_MAINTAINER_MODE=ON" -D "USE_BACKTRACE=ON"'

pack-win64-relative:
	$(MAKE) pack-winXX BITS=64 TARGET="Visual Studio 12 Win64" MOREOPTS='-D "USE_RELATIVE=ON" -D "USE_MAINTAINER_MODE=ON" -D "USE_BACKTRACE=ON"'

win64-relative:
	$(MAKE) winXX-cmake BITS=64 TARGET="Visual Studio 12 Win64" MOREOPTS='-D "USE_RELATIVE=ON"'
	$(MAKE) winXX-build BITS=64 BUILD_TARGET=RelWithDebInfo

win64-relative-debug:
	$(MAKE) winXX-cmake BITS=64 TARGET="Visual Studio 12 Win64" MOREOPTS='-D "USE_RELATIVE=ON" -D "USE_MAINTAINER_MODE=ON" -D "USE_BACKTRACE=ON"'
	$(MAKE) winXX-build BITS=64 BUILD_TARGET=Debug

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
	rm -f ./.file-list-js
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
