# Include gcc options.
include(${CMAKE_CURRENT_LIST_DIR}/gcc.cmake)

# Compiler options.
add_compile_options(-Wrange-loop-analysis)
