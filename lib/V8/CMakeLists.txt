add_library(arango_v8 STATIC
  V8PlatformFeature.cpp
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
  arango_assertions
  arango_static_strings
  arango_futures
  arango_basic_errors
  arango_basic_utils
  arango_basic_features
  arango_basic_logger
  arango_inspection
  boost_boost
  boost_system
  absl::flat_hash_set
  absl::synchronization
  arango_async
  fmt::fmt
  v8_interface
  velocypack
  immer
)
target_include_directories(arango_v8
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  SYSTEM PUBLIC
  ${OPENSSL_INCLUDE_DIR}
  ${ICU64_INCLUDE_DIR})

# Enterprise
if (USE_ENTERPRISE)
  target_include_directories(arango_v8 PUBLIC "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")
endif()

add_library(arango_v8_no_deadline STATIC
  v8-no-deadline.cpp)
target_link_libraries(arango_v8_no_deadline
  arango
  v8_interface
  velocypack)
target_include_directories(arango_v8_no_deadline PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR})
