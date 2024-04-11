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
      CXXFLAGS=${CMAKE_CXX_FLAGS}
      CPPFLAGS=${CMAKE_CPPFLAGS}
      --prefix=${CMAKE_CURRENT_BINARY_DIR}
      --with-library-suffix=64
      --with-data-packaging=static
      --enable-static
      --disable-dyload
      --disable-shared
  BUILD_COMMAND
      $(MAKE) -j 16
  INSTALL_COMMAND
    $(MAKE) install
      && ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/lib
      && ${CMAKE_COMMAND} -E copy ${ICU64_BINARY_DIR}/lib/libicui18n64.a ${CMAKE_CURRENT_BINARY_DIR}/lib
      && ${CMAKE_COMMAND} -E copy ${ICU64_BINARY_DIR}/lib/libicuuc64.a ${CMAKE_CURRENT_BINARY_DIR}/lib
      && ${CMAKE_COMMAND} -E copy ${ICU64_BINARY_DIR}/lib/libicudata64.a ${CMAKE_CURRENT_BINARY_DIR}/lib
)

set(ICU64_HOME "${CMAKE_CURRENT_BINARY_DIR}" PARENT_SCOPE)
set(ICU64_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/include" PARENT_SCOPE)
set(ICU64_LIBS "${ICU64_LIBS}" PARENT_SCOPE)
