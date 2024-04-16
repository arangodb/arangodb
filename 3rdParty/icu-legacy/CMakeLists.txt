# -*- mode: CMAKE; -*-

project(icu64 C)
include(ExternalProject)

set(ICU64 "64.2")
set(ICU64_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/icu64")
set(ICU64_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/icu64")

# copy files into binary directory
file(
  COPY
    "${ICU64_SOURCE_DIR}"
  DESTINATION
    "${CMAKE_CURRENT_BINARY_DIR}"
)
file(
  COPY
    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
  DESTINATION
    "${CMAKE_CURRENT_BINARY_DIR}"
)

# generate external project
set(ICU64_LIBS 
    "${CMAKE_CURRENT_BINARY_DIR}/lib/libicui18n64.a"
    "${CMAKE_CURRENT_BINARY_DIR}/lib/libicuuc64.a"
    "${CMAKE_CURRENT_BINARY_DIR}/lib/libicudata64.a"
)

# suppress tons of warnings caused by non-overridden methods in ICU source
set(ICU_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-suggest-override")
string(REGEX REPLACE "-Wsuggest-override" "" ICU_CXX_FLAGS ${ICU_CXX_FLAGS})

ExternalProject_Add(
  iculegacy_build
  SOURCE_DIR
    ${ICU64_SOURCE_DIR}
  BINARY_DIR
    ${ICU64_BINARY_DIR}
  PREFIX
    ${ICU64_BINARY_DIR}
  CONFIGURE_COMMAND
    ./configure
      CC=${CMAKE_C_COMPILER}
      CXX=${CMAKE_CXX_COMPILER}
      CFLAGS=${CMAKE_C_FLAGS}
      CXXFLAGS=${ICU_CXX_FLAGS}
      CPPFLAGS=${CMAKE_CPPFLAGS}
      --prefix=${CMAKE_CURRENT_BINARY_DIR}
      --with-library-suffix=64
      --with-data-packaging=static
      --enable-static
      --disable-samples 
      --disable-tests
      --disable-dyload
      --disable-shared
  BUILD_COMMAND
    $(MAKE)
  INSTALL_COMMAND
    ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/lib
      && ${CMAKE_COMMAND} -E copy ${ICU64_BINARY_DIR}/lib/libicui18n64.a ${CMAKE_CURRENT_BINARY_DIR}/lib
      && ${CMAKE_COMMAND} -E copy ${ICU64_BINARY_DIR}/lib/libicuuc64.a ${CMAKE_CURRENT_BINARY_DIR}/lib
      && ${CMAKE_COMMAND} -E copy ${ICU64_BINARY_DIR}/lib/libicudata64.a ${CMAKE_CURRENT_BINARY_DIR}/lib
)

# copy ICU public header files into build directory
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/unicode)

file(GLOB ICU_COMMON_HEADER_FILES "${ICU64_SOURCE_DIR}/common/unicode/*.h")
file(COPY ${ICU_COMMON_HEADER_FILES} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include/unicode)

file(GLOB ICU_I18N_HEADER_FILES "${ICU64_SOURCE_DIR}/common/i18n/*.h")
file(COPY ${ICU_I18N_HEADER_FILES} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include/unicode)

set(ICU64_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include" PARENT_SCOPE)
set(ICU64_LIBRARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/lib" PARENT_SCOPE)
set(ICU64_LIBS "${ICU64_LIBS}" PARENT_SCOPE)
