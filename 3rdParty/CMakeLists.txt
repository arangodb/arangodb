#add_subdirectory(boost)
#add_subdirectory(etcd)
#add_subdirectory(libev)
#add_subdirectory(linenoise-ng)
#add_subdirectory(valgrind)
#add_subdirectory(velocypack)
#add_subdirectory(zlib-1.2.7)

include(ExternalProject)

# EV
if (UNIX)
  ExternalProject_Add(ev_build
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libev
    BUILD_IN_SOURCE TRUE
    INSTALL_COMMAND ""
    )
  
endif()

### V8 and ICU

if(UNIX)
    set(V8_DIR "${CMAKE_CURRENT_SOURCE_DIR}/V8-4.3.61")
    set(V8_PLATFORM "native")
    set(V8_BIN_DIR "${V8_DIR}/out/")
else()
    message(ERROR "Not implemented for your OS")
endif()

include(ExternalProject)
set(EXTERNAL_BUILD_ARGS "-j" CACHE STRING "flags for external project builds")
ExternalProject_Add(v8_build
    SOURCE_DIR          "${V8_DIR}"
    BUILD_IN_SOURCE     TRUE
    BUILD_COMMAND       CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} CXX_FLAGS=-std=c++11 make -j8 -f Makefile-v8 strictaliasing=off snapshot=off werror=no  "${V8_PLATFORM}" 
    INSTALL_COMMAND     ""
)

macro(import_target tname tdep tinclude tpath)
    add_library(${tname} STATIC IMPORTED GLOBAL)
    add_dependencies(${tname} ${tdep})
    set_target_properties(${tname} PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES
            "${tinclude}"
        IMPORTED_LOCATION
            "${tpath}"
    )
endmacro()


## V8
import_target(v8_base v8_build
    "${V8_DIR}/include"
    "${V8_BIN_DIR}/${V8_PLATFORM}/obj.target/tools/gyp/libv8_base.a"
)

import_target(v8_libbase v8_build
    "${V8_DIR}/include"
    "${V8_BIN_DIR}/${V8_PLATFORM}/obj.target/tools/gyp/libv8_libbase.a"
)

import_target(v8_libplatform v8_build
    "${V8_DIR}/include"
    "${V8_BIN_DIR}/${V8_PLATFORM}/obj.target/tools/gyp/libv8_libplatform.a"
)


import_target(v8_nosnapshot v8_build
    "${V8_DIR}/include"
    "${V8_BIN_DIR}/${V8_PLATFORM}/obj.target/tools/gyp/libv8_nosnapshot.a"
)

import_target(v8_snapshot v8_build
    "${V8_DIR}/include"
    "${V8_BIN_DIR}/${V8_PLATFORM}/obj.target/tools/gyp/libv8_snapshot.a"
)

## ICU
# include include paths
add_library(icu_header INTERFACE)
target_include_directories(icu_header INTERFACE 
    "${V8_DIR}/third_party/icu/source/common"
    "${V8_DIR}/third_party/icu/source/io"
    "${V8_DIR}/third_party/icu/source/i18n"
)
import_target(icu_uc v8_build
    "${V8_DIR}/third_party/icu/source/common;${V8_DIR}/third_party/icu/source/io;${V8_DIR}/third_party/icu/source/i18n"
    "${V8_BIN_DIR}/${V8_PLATFORM}/obj.target/third_party/icu/libicuuc.a"
)

import_target(icu_data v8_build
    "${V8_DIR}/third_party/icu/source/common;${V8_DIR}/third_party/icu/source/io;${V8_DIR}/third_party/icu/source/i18n"
    "${V8_BIN_DIR}/${V8_PLATFORM}/obj.target/third_party/icu/libicudata.a"
)

import_target(icu_i18n v8_build
    "${V8_DIR}/third_party/icu/source/common;${V8_DIR}/third_party/icu/source/io;${V8_DIR}/third_party/icu/source/i18n"
    "${V8_BIN_DIR}/${V8_PLATFORM}/obj.target/third_party/icu/libicui18n.a"
)
