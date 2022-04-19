add_library(wasm-interface STATIC
  wasm3_interface.cpp wasm3_interface.hpp)
target_link_libraries(wasm-interface fmt::fmt)
target_link_libraries(wasm-interface m3)
target_link_libraries(wasm-interface arango)

add_library(wasm-interface-with-slices STATIC
  wasm_with_slices.cpp wasm_with_slices.hpp)
target_link_libraries(wasm-interface-with-slices wasm-interface)
target_link_libraries(wasm-interface-with-slices velocypack)
