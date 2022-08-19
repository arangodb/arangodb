
option(ARANGODB_SKIP_COMPILER_CHECK "Skip compiler support check.")

macro(CheckMinVersionsSet)
  if(NOT DEFINED ARANGODB_MIN_GCC_VERSION)
    message(FATAL_ERROR "Please define ARANGODB_MIN_GCC_VERSION in CMakeLists.txt.")
  endif()
  if(NOT DEFINED ARANGODB_MIN_CLANG_VERSION)
    message(FATAL_ERROR "Please define ARANGODB_MIN_CLANG_VERSION in CMakeLists.txt.")
  endif()
  if(NOT DEFINED ARANGODB_MIN_MSVC_VERSION)
    message(FATAL_ERROR "Please define ARANGODB_MIN_MSVC_VERSION in CMakeLists.txt.")
  endif()
endmacro()

macro(CheckMinCompilerVersionInner MIN_VERSION)
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS ${MIN_VERSION})
    message(
      FATAL_ERROR
        "ArangoDB requires at least ${CMAKE_CXX_COMPILER_ID} version ${MIN_VERSION},"
        " detected ${CMAKE_CXX_COMPILER_VERSION}."
        "\nYou can use ARANGODB_SKIP_COMPILER_CHECK to skip this check.")
  endif()
endmacro()

macro(CheckCompilerVersion)

  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    CheckMinCompilerVersionInner(${ARANGODB_MIN_GCC_VERSION})
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    CheckMinCompilerVersionInner(${ARANGODB_MIN_CLANG_VERSION})
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    CheckMinCompilerVersionInner(${ARANGODB_MIN_MSVC_VERSION})
  else()
    message(
      FATAL_ERROR
        "ArangoDB does not support CMAKE_CXX_COMPILER_ID ${CMAKE_CXX_COMPILER_ID}"
    )
  endif()

endmacro()

CheckMinVersionsSet()

if (NOT ${ARANGODB_SKIP_COMPILER_CHECK})
  CheckCompilerVersion()
endif()
