# in other words if not clang-cl
if (NOT CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
  add_link_options(-fsanitize=undefined)
endif ()
add_compile_options(-fsanitize=undefined)
# TODO(MBkkt) Fix alignment issues
add_compile_options(-fno-sanitize=alignment)
# TODO(MBkkt) uncomment
# add_compile_options(-fsanitize=float-divide-by-zero)
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  # add_compile_options(-fsanitize=float-cast-overflow)
else ()
  # add_compile_options(-fsanitize=implicit-conversion,local-bounds)
endif ()
