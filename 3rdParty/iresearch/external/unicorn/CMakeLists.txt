################################################################################
### Unicorn library
################################################################################
set(UNICORN_sources
  ${CMAKE_CURRENT_SOURCE_DIR}/unicorn/segment.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/unicorn/character.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/unicorn/utf.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/unicorn/ucd-property-tables.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/unicorn/ucd-block-tables.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/unicorn/ucd-case-tables.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/unicorn/ucd-character-names.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/unicorn/ucd-decomposition-tables.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/unicorn/ucd-numeric-tables.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/unicorn/ucd-property-tables.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/unicorn/ucd-script-tables.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/unicorn/ucd-bidi-tables.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/unicorn/string-case.cpp
)

add_library(
  ${IResearch_TARGET_NAME}-unicorn
  STATIC
  ${UNICORN_sources}
)

target_include_directories(
  ${IResearch_TARGET_NAME}-unicorn
  SYSTEM
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
if(NOT ZLIB_FOUND AND NOT TARGET zlibstatic)
  if ("${ZLIB_ROOT}" STREQUAL "")
    set(ZLIB_ROOT "$ENV{ZLIB_ROOT}")
  endif()
  if ("${ZLIB_ROOT}" STREQUAL "") # we don`t have zlib root dir set. Try to find package
    find_package(ZLIB REQUIRED)
  else()
    message(STATUS  "Have zlib sources at ${ZLIB_ROOT}!  Will build zlib.")
    set(SKIP_INSTALL_ALL ON)
    add_subdirectory(
      ${ZLIB_ROOT}
      ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/iresearch-zlibstatic.dir
    )
    set(ZLIB_FOUND True)
    get_target_property(ZLIB_INCLUDE_DIRS zlibstatic INCLUDE_DIRECTORIES)
  endif()
endif()

if(NOT DEFINED ZLIB_INCLUDE_DIRS AND TARGET zlibstatic)
  get_target_property(ZLIB_INCLUDE_DIRS zlibstatic INCLUDE_DIRECTORIES)
endif()


if (TARGET zlibstatic)
  target_link_libraries(${IResearch_TARGET_NAME}-unicorn zlibstatic)
else()
  if (MSVC)
    find_library(ZLIB_STATIC_LIBRARIES NAMES zlibstatic.lib)
  else()
    find_library(ZLIB_STATIC_LIBRARIES NAMES libz.a)
  endif()
  target_link_libraries(${IResearch_TARGET_NAME}-unicorn ${ZLIB_STATIC_LIBRARIES})
endif()

target_include_directories(
  ${IResearch_TARGET_NAME}-unicorn
  SYSTEM
  PRIVATE ${ZLIB_INCLUDE_DIRS}
)

# disable warnings for 3rd-party libs for a cleaner build
if (MSVC)
  target_compile_definitions(
    ${IResearch_TARGET_NAME}-unicorn
    PUBLIC "_CRT_SECURE_NO_WARNINGS"
  )
else()
  target_compile_options(
    ${IResearch_TARGET_NAME}-unicorn
    PRIVATE "-fPIC"
  )
endif()
