macro(CheckCompilerVersion MIN_CLANG_VERSION)

string(ASCII 27 ESC)
set(COLOR_RESET "${ESC}[m")
set(RED        "${ESC}[31m")

if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
  set(MIN_VERSION ${MIN_CLANG_VERSION})
else()
  message(WARNING "${RED}${CMAKE_C_COMPILER_ID}/${CMAKE_CXX_COMPILER_ID} compiler is not supported.${COLOR_RESET}")
endif()

if ((CMAKE_C_COMPILER_VERSION VERSION_LESS ${MIN_VERSION}) OR (CMAKE_CXX_COMPILER_VERSION VERSION_LESS ${MIN_VERSION}))
  message(WARNING "${RED}${CMAKE_C_COMPILER_ID} compiler version C(${CMAKE_C_COMPILER_VERSION})/C++(${CMAKE_C_COMPILER_VERSION}) is too old. Need at least ${MIN_VERSION}.${COLOR_RESET}")
endif()

endmacro()
