add_library(arango_futures STATIC
  Future.cpp)

target_include_directories(arango_futures PRIVATE
  ${PROJECT_SOURCE_DIR}/3rdParty/iresearch/external/function2
  PUBLIC
  ${PROJECT_SOURCE_DIR}/lib
  )

if (MSVC)
  target_compile_options(arango_futures PUBLIC "-J")
endif()

if (USE_FAIL_ON_WARNINGS)
  if (MSVC)
    target_compile_options(arango_futures PRIVATE /WX /D_WINSOCK_DEPRECATED_NO_WARNINGS96)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang") # clang-cl.exe
      # There's a warning about /MP being unused if compiling only a single file
      target_compile_options(arango_futures PRIVATE -Wno-unused-command-line-argument)
    endif()
  else ()
    target_compile_options(arango_futures PRIVATE -Werror -Wno-error=deprecated-declarations)
  endif ()
endif ()
