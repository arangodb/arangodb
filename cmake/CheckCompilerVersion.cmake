
option(ARANGODB_JUST_WARN_COMPILER_CHECK "Do not exit with FATAL_ERROR and just WARN "
	                                 "if the compiler is not recent enough.")

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
    set(MESSAGE "ArangoDB requires at least ${CMAKE_CXX_COMPILER_ID} version ${MIN_VERSION},"
                " detected ${CMAKE_CXX_COMPILER_VERSION}.")

    if(${ARANGODB_WARN_COMPILER_CHECK})
      message(WARN ${MESSAGE})
    else()
      message(FATAL_ERROR
        ${MESSAGE}
	"\nYou can use -DARANGODB_JUST_WARN_COMPILER_CHECK to turn this error into a warning.")
    endif()
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
        "ArangoDB does not support CMAKE_CXX_COMPILER_ID ${CMAKE_CXX_COMPILER_ID}")
  endif()

endmacro()

CheckMinVersionsSet()

CheckCompilerVersion()
