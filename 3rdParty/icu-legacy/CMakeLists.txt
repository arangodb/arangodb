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
)

# add build-type specific c flags and cxx flags here, because otherwise
# they would be lost
set(ICU_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE_UPPER}}")
set(ICU_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE_UPPER}} -Wno-suggest-override")
# suppress tons of warnings caused by non-overridden methods in ICU source.
string(REGEX REPLACE "-Wsuggest-override" "" ICU_CXX_FLAGS ${ICU_CXX_FLAGS})
message(DEBUG "ICU_C_FLAGS=${ICU_C_FLAGS} ICU_CXX_FLAGS=${ICU_CXX_FLAGS}")

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
      CFLAGS=${ICU_C_FLAGS}
      CXXFLAGS=${ICU_CXX_FLAGS}
      CPPFLAGS=${CMAKE_CPPFLAGS}
      --prefix=${CMAKE_CURRENT_BINARY_DIR}
      --with-library-suffix=64
      --with-data-packaging=archive
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

set(ICU_DT_LEGACY "${CMAKE_CURRENT_SOURCE_DIR}/icudtl_legacy.dat")
set(ICU_DT_LEGACY ${ICU_DT_LEGACY} PARENT_SCOPE)
set(ICU_DT_LEGACY_DEST "icudtl_legacy.dat" )
set(ICU_DT_LEGACY_DEST ${ICU_DT_LEGACY_DEST} PARENT_SCOPE)

add_custom_command(
    TARGET iculegacy_build POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${ICU_DT_LEGACY} ${CMAKE_BINARY_DIR}/bin/${ICU_DT_LEGACY_DEST})

add_custom_command(
    TARGET iculegacy_build POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${ICU_DT_LEGACY} ${CMAKE_BINARY_DIR}/tests/${ICU_DT_LEGACY_DEST})

