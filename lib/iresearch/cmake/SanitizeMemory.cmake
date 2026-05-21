# Memory Sanitizer doesn't work without compile stdlib with it!
add_compile_options(-fsanitize=memory)
# MSVC doesn't support memory sanitizer, so don't care
add_link_options(-fsanitize=memory)
