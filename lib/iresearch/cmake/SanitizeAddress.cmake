add_compile_options(-fsanitize=address)
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  add_compile_options(
    -fsanitize-address-use-after-return
  )
else ()
  # in other words if not clang-cl
  if (NOT CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
    add_link_options(
      -fsanitize=address
    )
    add_compile_options(
      -fno-common
    )
  endif ()
  if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # -fsanitize=pointer-subtract removed because detect_invalid_pointer_pairs fails
    # with fiber fault injection on test initialization, probably caused by GTest
    # TODO(MBkkt) add -fsanitize=pointer-compare when will be fixed:
    # https://github.com/google/sanitizers/issues/1324 also see mentioned issue
    add_compile_options(
      # -fsanitize=pointer-subtract,pointer-compare
      -fsanitize-address-use-after-scope
    )
  elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(
      # -fsanitize=pointer-subtract,pointer-compare
      -fsanitize-address-use-after-scope
      -mllvm -asan-use-private-alias=1
    )
  endif ()
endif ()
