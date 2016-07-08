# -*- mode: Makefile; -*-

## -----------------------------------------------------------------------------
## --SECTION--                                                  COMMON VARIABLES
## -----------------------------------------------------------------------------

SRC=$(shell pwd |sed "s;.*/;;")

.PHONY: warning help

warning:
	@echo "ArangoDB has switched to CMAKE. In order to compile, use:"
	@echo ""
	@echo "  mkdir -p build"
	@echo "  cd build"
	@echo "  cmake .. -DCMAKE_BUILD_TYPE=Release"
	@echo "  make"
	@echo ""
	@echo "MacOS users:"
	@echo "  Please use OPENSSL from homebrew and use"
	@echo ""
	@echo "    cmake .. -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl -DCMAKE_BUILD_TYPE=Release"
	@echo ""
	@echo "  Note that some versions of Apple's clang have severe performance"
	@echo "  issues. Use GCC5 from homebrew in this case."
	@echo ""
	@echo "Use 'make help' to see more options."

help:
	@echo "The most common -D<options> are"
	@echo ""
	@echo "  -DCMAKE_CXX_COMPILER=/usr/local/bin/g++-5"
	@echo "    sets the C++ compiler"
	@echo "  -DCMAKE_C_COMPILER=/usr/local/bin/gcc-5"
	@echo "    sets the C compiler"
	@echo ""
	@echo "ArangoDB supports:"
	@echo ""
	@echo "  -DUSE_BACKTRACE=off"
	@echo "    if ON, enables backtraces in fatal errors"
	@echo "  -DUSE_FAILURE_TESTS=off"
	@echo "    if ON, add failure functions for testing"
	@echo "  -DUSE_MAINTAINER_MODE=off"
	@echo "    if ON, enable maintainer features"
	@echo ""
	@echo "BOOST supports:"
	@echo ""
	@echo "  -DUSE_BOOST_SYSTEM_LIBS=off"
	@echo "    if ON, use the operating system Boost installation"
	@echo "  -DUSE_BOOST_UNITTESTS=on"
	@echo "    if ON, use Boost unittest framework if this available"
	@echo ""
	@echo "OPENSSL supports:"
	@echo "  -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl"
	@echo "    sets the location of the openssl includes and libraries"
	@echo ""
	@echo "JEMALLOC supports:"
	@echo "  -DUSE_JEMALLOC=on"
	@echo "    if ON, link against JEMALLOC"
	@echo ""
	@echo "TCMALLOC supports:"
	@echo "  -DUSE_TCMALLOC=on"
	@echo "    if ON, link against TCMALLOC"
	@echo ""
	@echo "V8 supports:"
	@echo "  -DUSE_PRECOMPILED_V8=off"
	@echo "    if ON, use precompiled V8 libraries"
	@echo ""
	@echo "ZLIB supports:"
	@echo "  -DASM686=on"
	@echo "    if ON, enables building i686 assembly implementation"
	@echo "  -DAMD64=on"
	@echo "    if ON, enables building amd64 assembly implementation"


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
	${MAKE} pack-dmg-cmake

pack-dmg-cmake:
	cd Build && cmake \
		-D "USE_OPTIMIZE_FOR_ARCHITECTURE=Off" \
		-D "CMAKE_BUILD_TYPE=RelWithDebInfo" \
		-D "CMAKE_OSX_DEPLOYMENT_TARGET=10.10" \
		-D "CMAKE_INSTALL_PREFIX=/opt/arangodb" \
		-D "OPENSSL_ROOT_DIR=`brew --prefix`/opt/openssl" \
		..

	cd Build && ${MAKE}

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
	${MAKE} -f GNUMakefile pack-macosxcode-cmake MOREOPTS='$(MOREOPTS)'

pack-macosxcode-cmake:
	cd Build && cmake \
		-D "USE_OPTIMIZE_FOR_ARCHITECTURE=Off" \
		-D "CMAKE_BUILD_TYPE=RelWithDebInfo" \
		-D "CMAKE_OSX_DEPLOYMENT_TARGET=10.10" \
		-D "CMAKE_INSTALL_PREFIX=/opt/arangodb" \
		-D "OPENSSL_ROOT_DIR=`brew --prefix`/opt/openssl" \
		-G Xcode \
		$(MOREOPTS) \
		..


################################################################################
### @brief MacOSX
################################################################################

.PHONY: pack-macosx pack-macosx-cmake

PACK_DESTDIR ?= .

pack-macosx:
	rm -rf Build && mkdir Build

	${MAKE} pack-macosx-cmake MOREOPTS='$(MOREOPTS)'

pack-macosx-cmake:
	cd Build && cmake \
		-D "BUILD_PACKAGE=dmg-cli" \
		-D "CMAKE_INSTALL_PREFIX=${prefix}" \
		-D "OPENSSL_INCLUDE=`brew --prefix`/opt/openssl/include" \
		-D "OPENSSL_LIB_PATH=`brew --prefix`/opt/openssl/lib" \
		-D "OPENSSL_LIBS=`brew --prefix`/opt/openssl/lib/libssl.a;`brew --prefix`/opt/openssl/lib/libcrypto.a" \
		$(MOREOPTS) \
		..

	${MAKE} ${BUILT_SOURCES}

	test -d bin || mkdir bin

	cd Build && ${MAKE} install DESTDIR=${PACK_DESTDIR}

################################################################################
### @brief debian arm package
################################################################################

.PHONY: pack-arm pack-arm-cmake

pack-arm:
	rm -rf Build && mkdir Build

	${MAKE} pack-arm-cmake

pack-arm-cmake:
	cd Build && cmake \
		-D "BUILD_PACKAGE=raspbian" \
		-D "CMAKE_CXX_FLAGS_RELEASE:STRING=-O2 -DNDEBUG" \
		-D "CMAKE_C_FLAGS_RELEASE:STRING=-O2 -DNDEBUG" \
		-D "CMAKE_INSTALL_PREFIX=${prefix}" \
		-D "ETCDIR=${sysconfdir}" \
		-D "VARDIR=${localstatedir}" \
		$(MOREOPTS) \
		..

	${MAKE} ${BUILT_SOURCES}

	cd Build && cpack -G DEB


pack-deb-cmake:
	mkdir Build
	cd Build && cmake \
		-D "CMAKE_CXX_FLAGS_RELEASE:STRING=-O2 -DNDEBUG" \
		-D "CMAKE_C_FLAGS_RELEASE:STRING=-O2 -DNDEBUG" \
		-D "CMAKE_INSTALL_PREFIX=${prefix}" \
		-D "ETCDIR=${sysconfdir}" \
		-D "VARDIR=${localstatedir}" \
		$(MOREOPTS) \
		..

	${MAKE} ${BUILT_SOURCES}

	cd Build && ${MAKE}

	cd Build && cpack -G DEB

################################################################################
### @brief Windows 64-bit bundle
################################################################################

.PHONY: pack-win32 pack-winXX winXX-cmake win64-relative win64-relative-debug

pack-win32: 
	$(MAKE) pack-winXX BITS=32 TARGET="Visual Studio 14" MOREOPTS='-D "V8_TARGET_ARCHS=Release"'

pack-win64:
	$(MAKE) pack-winXX BITS=64 TARGET="Visual Studio 14 Win64" MOREOPTS='-D "V8_TARGET_ARCHS=Release"'

pack-win32-relative:
	$(MAKE) pack-winXX BITS=32 TARGET="Visual Studio 14" MOREOPTS='-D "USE_MAINTAINER_MODE=ON" -D "USE_BACKTRACE=ON" -D "V8_TARGET_ARCHS=Debug"'

pack-win64-relative:
	$(MAKE) pack-winXX BITS=64 TARGET="Visual Studio 14 Win64" MOREOPTS='-D "USE_MAINTAINER_MODE=ON" -D "USE_BACKTRACE=ON" -D "V8_TARGET_ARCHS=Debug"' 

win64-relative:
	$(MAKE) winXX-cmake BITS=64 TARGET="Visual Studio 14 Win64" MOREOPTS='-D "V8_TARGET_ARCHS=Debug"'
	$(MAKE) winXX-build BITS=64 BUILD_TARGET=RelWithDebInfo

win64-relative-debug:
	$(MAKE) winXX-cmake BITS=64 TARGET="Visual Studio 14 Win64" MOREOPTS=' -D "USE_MAINTAINER_MODE=ON" -D "USE_BACKTRACE=ON" -D "V8_TARGET_ARCHS=Debug"' 
	$(MAKE) winXX-build BITS=64 BUILD_TARGET=Debug

pack-winXX:
	rm -rf ../b && mkdir ../b

	${MAKE} winXX-cmake BITS="$(BITS)" TARGET="$(TARGET)" BUILD_TARGET=RelWithDebInfo
	${MAKE} winXX-build BITS="$(BITS)" TARGET="$(TARGET)" BUILD_TARGET=RelWithDebInfo
	${MAKE} packXX BITS="$(BITS)" BUILD_TARGET=RelWithDebInfo

pack-winXX-MOREOPTS:
	rm -rf ../b && mkdir ../b

	${MAKE} winXX-cmake BITS="$(BITS)" TARGET="$(TARGET)" MOREOPTS=$(MOREOPTS)
	${MAKE} winXX-build BITS="$(BITS)" TARGET="$(TARGET)" BUILD_TARGET=Debug
	${MAKE} packXX BITS="$(BITS)" TARGET="$(TARGET)" BUILD_TARGET=Debug

winXX-cmake:
	cd ../b && cmake \
		-G "$(TARGET)" \
		-D "CMAKE_BUILD_TYPE=RelWithDebInfo" \
		-D "BUILD_TYPE=RelWithDebInfo" \
		-D "BUILD_ID=${BUILD_ID}" \
		$(MOREOPTS) \
		../$(SRC)/

winXX-build:
	cp Installation/Windows/Icons/arangodb.ico ../b
	cd ../b && cmake --build . --config $(BUILD_TARGET)

packXX:
	cd ../b && cpack -G NSIS64 -C $(BUILD_TARGET)
	cd ../b && cpack -G ZIP  -C $(BUILD_TARGET)
