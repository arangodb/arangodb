
option(ARANGODB_JUST_WARN_COMPILER_CHECK "Do not exit with FATAL_ERROR and just WARN "
	                                 "if the compiler is not recent enough.")

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

function(CheckCompilerVersion ...)
  if(ARGC LESS 3)
	  message(FATAL_ERROR "CheckCompilerVersion: Not enough arguments\n"
	                      "usage:\n"
			      "  CheckCompilerVersion(MIN_GCC_VERSION MIN_CLANG_VERSION MIN_MSVC_VERSION)")
  endif()
  set(MIN_GCC_VERSION ARG0)
  set(MIN_CLANG_VERSION ARG1)
  set(MIN_MSVC_VERSION ARG2)

  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    CheckMinCompilerVersionInner(MIN_GCC_VERSION)
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    CheckMinCompilerVersionInner(MIN_CLANG_VERSION)
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    CheckMinCompilerVersionInner(MIN_MSVC_VERSION)
  else()
    message(
      FATAL_ERROR
        "ArangoDB does not support CMAKE_CXX_COMPILER_ID ${CMAKE_CXX_COMPILER_ID}")
  endif()
endfunction()

