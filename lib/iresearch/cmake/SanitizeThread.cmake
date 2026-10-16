add_compile_options(-fsanitize=thread)
# MSVC doesn't support thread sanitizer, so don't care
add_link_options(-fsanitize=thread)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  add_compile_options(-gcolumn-info)
endif ()
