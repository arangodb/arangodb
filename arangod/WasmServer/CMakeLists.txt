add_library(arango_wasm STATIC
  Methods.cpp
  WasmCommon.cpp
  RestWasmHandler.cpp
  WasmServerFeature.cpp
  WasmModuleCollection.cpp)

target_link_libraries(arango_wasm arango)
target_link_libraries(arango_wasm m3)
target_link_libraries(arango_wasm arango_vocbase)
