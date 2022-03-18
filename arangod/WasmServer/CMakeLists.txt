add_library(arango_wasm STATIC
  Wasm3.cpp
  Methods.cpp
  WasmCommon.cpp
  RestWasmHandler.cpp
  WasmServerFeature.cpp
  WasmModuleCollection.cpp)

target_link_libraries(arango_wasm arango)
target_link_libraries(arango_wasm m3)
target_link_libraries(arango_wasm arango_vocbase)
