include(ExternalProject)

macro(import_target tname tdep tinclude tpath)
  add_library(${tname} STATIC IMPORTED GLOBAL)
  add_dependencies(${tname} ${tdep})
  set_target_properties(${tname} PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${tinclude}"
    IMPORTED_LOCATION "${tpath}"
    )
endmacro()

################################################################################
## ETCD
################################################################################

if (GO_FOUND)
  if (MSVC)
    set (ETCD_BUILD_COMMAND build.bat)
  else()
    set (ETCD_BUILD_COMMAND ./build)
  endif()
  ExternalProject_Add(etcd_build
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/etcd
    CONFIGURE_COMMAND ""
    BUILD_IN_SOURCE TRUE
    BUILD_COMMAND "${ETCD_BUILD_COMMAND}"
    INSTALL_COMMAND "")
endif()

################################################################################
## ZLIB
################################################################################

if (UNIX)
  ExternalProject_Add(zlib_build
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/zlib-1.2.7
    BUILD_IN_SOURCE 1
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/zlib
    CONFIGURE_COMMAND ./configure --prefix=${CMAKE_CURRENT_BINARY_DIR}/zlib --static
    INSTALL_COMMAND ""
    UPDATE_COMMAND ""
    )  
endif()

import_target(zlib zlib_build
    "${CMAKE_CURRENT_SOURCE_DIR}/libev"
    "${CMAKE_CURRENT_SOURCE_DIR}/libev/.libs/libev.a"
)

################################################################################
## EV
################################################################################

if (UNIX)
  ExternalProject_Add(ev_build
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libev
    BUILD_IN_SOURCE 1
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/libev
    CONFIGURE_COMMAND ./configure --prefix=${CMAKE_CURRENT_BINARY_DIR}/libev --enable-static --disable-shared
    INSTALL_COMMAND ""
    UPDATE_COMMAND ""
    )  
endif()

import_target(ev ev_build
    "${CMAKE_CURRENT_SOURCE_DIR}/libev"
    "${CMAKE_CURRENT_SOURCE_DIR}/libev/.libs/libev.a"
)

################################################################################
## V8 and ICU
################################################################################

if(UNIX)
  set(V8_DIR "${CMAKE_CURRENT_SOURCE_DIR}/V8-4.3.61")
  set(V8_PLATFORM "native")
  set(V8_BIN_DIR "${V8_DIR}/out/${V8_PLATFORM}")
  set(ICU_BIN_DIR "${V8_BIN_DIR}")
  if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(V8_BIN_DIR "${V8_BIN_DIR}/obj.target/tools/gyp")
    set(ICU_BIN_DIR "${ICU_BIN_DIR}/obj.target/third_party/icu")
  endif()
else()
  message(ERROR "Not implemented for your OS")
endif()

set (V8_BUILD_COMMAND $(MAKE) -f Makefile-v8
  strictaliasing=off snapshot=off werror=no
  ${V8_PLATFORM}
  CC=${CMAKE_C_COMPILER}
  CXX=${CMAKE_CXX_COMPILER}
  CXXFLAGS=${CMAKE_CXX_FLAGS}
  LIBS=${CMAKE_EXE_LINKER_FLAGS}
  PYTHON_EXECUTABLE=${PYTHON_EXECUTABLE}
  )

if (APPLE AND CMAKE_COMPILER_IS_CLANG)
  set (V8_BUILD_COMMAND ${V8_BUILD_COMMAND} LDFLAGS=-stdlib=libc++)
endif()

if (NOT USE_PRECOMPILED_V8)
ExternalProject_Add(v8_build SOURCE_DIR "${V8_DIR}"
  BUILD_IN_SOURCE TRUE BUILD_COMMAND "${V8_BUILD_COMMAND}" 
  INSTALL_COMMAND "")
endif()

################################################################################
## V8
################################################################################

import_target(v8_base v8_build "${V8_DIR}/include"
  "${V8_BIN_DIR}/libv8_base.a") 

import_target(v8_libbase v8_build "${V8_DIR}/include"
  "${V8_BIN_DIR}/libv8_libbase.a")

import_target(v8_libplatform v8_build "${V8_DIR}/include"
  "${V8_BIN_DIR}/libv8_libplatform.a") 

import_target(v8_nosnapshot v8_build "${V8_DIR}/include"
  "${V8_BIN_DIR}/libv8_nosnapshot.a") 

import_target(v8_snapshot v8_build "${V8_DIR}/include"
  "${V8_BIN_DIR}/libv8_snapshot.a") 


################################################################################
## ICU
################################################################################

# include include paths
#add_library(icu_header INTERFACE)
#target_include_directories(icu_header INTERFACE
#  "${V8_DIR}/third_party/icu/source/common"
#  "${V8_DIR}/third_party/icu/source/io"
#  "${V8_DIR}/third_party/icu/source/i18n") 

import_target(icu_uc v8_build
  "${V8_DIR}/third_party/icu/source/common;${V8_DIR}/third_party/icu/source/io;${V8_DIR}/third_party/icu/source/i18n"
  "${ICU_BIN_DIR}/libicuuc.a"
)

import_target(icu_data v8_build
  "${V8_DIR}/third_party/icu/source/common;${V8_DIR}/third_party/icu/source/io;${V8_DIR}/third_party/icu/source/i18n"
  "${ICU_BIN_DIR}/libicudata.a"
  )

import_target(icu_i18n v8_build
  "${V8_DIR}/third_party/icu/source/common;${V8_DIR}/third_party/icu/source/io;${V8_DIR}/third_party/icu/source/i18n"
  "${ICU_BIN_DIR}/libicui18n.a"
  )
