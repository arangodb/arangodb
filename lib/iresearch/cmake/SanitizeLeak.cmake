add_compile_options(-fsanitize=leak)
# MSVC doesn't support leak sanitizer, so don't care
add_link_options(-fsanitize=leak)
