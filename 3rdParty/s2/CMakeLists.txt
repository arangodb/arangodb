
cmake_minimum_required(VERSION 2.8)
project(s2-geometry)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(CheckCXXCompilerFlag)
include(FindPackageHandleStandardArgs)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

if(WIN32)
  CMAKE_DEPENDENT_OPTION(CURL_WINDOWS_SSPI "Use windows libraries to allow NTLM authentication without openssl" ON
    "NOT CMAKE_USE_OPENSSL" OFF)
  mark_as_advanced(CURL_WINDOWS_SSPI)
endif()

set(USE_OPENSSL OFF)
set(HAVE_LIBCRYPTO OFF)
set(HAVE_LIBSSL OFF)

if(CMAKE_USE_OPENSSL)
  find_package(OpenSSL)
  if(OPENSSL_FOUND)
    list(APPEND CURL_LIBS ${OPENSSL_LIBRARIES})
    set(USE_OPENSSL ON)
    set(HAVE_LIBCRYPTO ON)
    set(HAVE_LIBSSL ON)
    include_directories(${OPENSSL_INCLUDE_DIR})
    set(CMAKE_REQUIRED_INCLUDES ${OPENSSL_INCLUDE_DIR})
    check_include_file("openssl/crypto.h" HAVE_OPENSSL_CRYPTO_H)
    check_include_file("openssl/engine.h" HAVE_OPENSSL_ENGINE_H)
    check_include_file("openssl/err.h"    HAVE_OPENSSL_ERR_H)
    check_include_file("openssl/pem.h"    HAVE_OPENSSL_PEM_H)
    check_include_file("openssl/pkcs12.h" HAVE_OPENSSL_PKCS12_H)
    check_include_file("openssl/rsa.h"    HAVE_OPENSSL_RSA_H)
    check_include_file("openssl/ssl.h"    HAVE_OPENSSL_SSL_H)
    check_include_file("openssl/x509.h"   HAVE_OPENSSL_X509_H)
    check_include_file("openssl/rand.h"   HAVE_OPENSSL_RAND_H)
  elseif(WIN32)
    set(CURL_WINDOWS_SSPI ON)
  endif()
endif()

find_package(OpenSSL REQUIRED)

ADD_DEFINITIONS (-Wno-deprecated-declarations -DNDEBUG -std=c++11)

include_directories(geometry)
include_directories(geometry/util/endian)

file(GLOB S2Glob geometry/s2.cc geometry/s1interval.cc geometry/base/strtoint.cc geometry/util/math/exactfloat/exactfloat.cc geometry/strings/strutil.cc geometry/strings/split.cc geometry/base/stringprintf.cc geometry/s2cap.cc geometry/s1angle.cc geometry/s2cell.cc geometry/s2cellunion.cc geometry/s2cellid.cc geometry/s2edgeindex.cc geometry/s2edgeutil.cc geometry/s2latlngrect.cc geometry/s2loop.cc geometry/s2pointregion.cc geometry/s2latlng.cc geometry/s2polygon.cc geometry/s2polygonbuilder.cc geometry/s2polyline.cc geometry/s2r2rect.cc geometry/s2regionunion.cc geometry/s2region.cc geometry/s2regioncoverer.cc geometry/s2regionintersection.cc geometry/base/logging.cc geometry/util/coding/*.cc)

add_library(s2geo STATIC ${S2Glob})
# Add RPATH info to executables so that when they are run after being installed
# (i.e., from /usr/local/bin/) the linker can find library dependencies. For
# more info see http://www.cmake.org/Wiki/CMake_RPATH_handling
set_property(TARGET s2geo PROPERTY INSTALL_RPATH_USE_LINK_PATH TRUE)

if (GTEST_ROOT)
  add_library(s2testing STATIC geometry/s2testing.cc)
endif()

file(GLOB InstallGlob geometry/*.h)
file(GLOB BaseGlob geometry/base/*.h)
file(GLOB UtilMathGlob geometry/util/math/*.h)
if (GTEST_ROOT)
  file(GLOB TestGlob geometry/*_test.cc)
endif()

install(FILES ${InstallGlob} DESTINATION include/s2geo)
install(FILES ${BaseGlob} DESTINATION include/s2geo/base)
install(FILES ${UtilMathGlob} DESTINATION include/util/math)
install(TARGETS s2geo DESTINATION lib)

if (GTEST_ROOT)
  enable_testing()
  find_package(GTest REQUIRED)
  include_directories(${GTEST_INCLUDE_DIR})


  cmake_policy(SET CMP0037 OLD) # allow underscores in targets
  foreach( testsourcefile ${TestGlob} )
    get_filename_component(testname ${testsourcefile} NAME_WE)
    add_executable( ${testname} ${testsourcefile} )
    target_link_libraries( ${testname} s2geo s2testing ${OPENSSL_LIBRARIES} ${GTEST_BOTH_LIBRARIES} )
  endforeach()
endif()
