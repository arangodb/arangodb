
if (USE_BUILD_ID_READER)

  add_library(arango_build_id
    STATIC
      src/BuildId.cpp)

  target_include_directories(arango_build_id
    PUBLIC
      include)

  target_link_options(arango_build_id
    PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/BuildId.ld
      -Wl,--build-id=sha1)

else ()

  add_library(arango_build_id INTERFACE)
  target_include_directories(arango_build_id
    INTERFACE
      include)

endif ()

