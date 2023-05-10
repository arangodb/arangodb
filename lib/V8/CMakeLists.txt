add_library(arango_v8 STATIC
  V8SecurityFeature.cpp
  JavaScriptSecurityContext.cpp
  JSLoader.cpp
  V8LineEditor.cpp
  v8-buffer.cpp
  v8-conv.cpp
  v8-globals.cpp
  v8-shell.cpp
  v8-utils.cpp
  v8-vpack.cpp
  v8-environment.cpp)
target_link_libraries(arango_v8
  arango
  boost_system
  boost_boost
  v8_interface
  velocypack)
target_include_directories(arango_v8
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  SYSTEM PUBLIC
  ${OPENSSL_INCLUDE_DIR}
  ${ICU_INCLUDE_DIR})

add_library(arango_v8_no_deadline STATIC
  v8-no-deadline.cpp)
target_link_libraries(arango_v8_no_deadline
  arango
  v8_interface
  velocypack)
target_include_directories(arango_v8_no_deadline PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR})
