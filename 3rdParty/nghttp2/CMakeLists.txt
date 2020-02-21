project(nghttp2 C CXX)

# add_subdirectory(lib/includes)

set(NGHTTP2_INCLUDES
  "${CMAKE_CURRENT_SOURCE_DIR}/lib/includes"
  "${CMAKE_CURRENT_BINARY_DIR}/lib/includes"
)

add_definitions(-DBUILDING_NGHTTP2)

set(NGHTTP2_SOURCES
  lib/nghttp2_pq.c 
  lib/nghttp2_map.c 
  lib/nghttp2_queue.c
  lib/nghttp2_frame.c
  lib/nghttp2_buf.c
  lib/nghttp2_stream.c 
  lib/nghttp2_outbound_item.c
  lib/nghttp2_session.c 
  lib/nghttp2_submit.c
  lib/nghttp2_helper.c
  lib/nghttp2_npn.c
  lib/nghttp2_hd.c 
  lib/nghttp2_hd_huffman.c 
  lib/nghttp2_hd_huffman_data.c
  lib/nghttp2_version.c
  lib/nghttp2_priority_spec.c
  lib/nghttp2_option.c
  lib/nghttp2_callbacks.c
  lib/nghttp2_mem.c
  lib/nghttp2_http.c
  lib/nghttp2_rcbuf.c
  lib/nghttp2_debug.c
)

add_library(nghttp2 STATIC
  ${NGHTTP2_SOURCES}
)

include(CheckTypeSize)
check_type_size("ssize_t" SIZEOF_SSIZE_T)
if(SIZEOF_SSIZE_T STREQUAL "")
  # ssize_t is a signed type in POSIX storing at least -1.
  # Set it to "int" to match the behavior of AC_TYPE_SSIZE_T (autotools).
  target_compile_definitions(nghttp2 PRIVATE ssize_t=int)
endif()

# we always build it statically
target_compile_definitions(nghttp2 PUBLIC "-DNGHTTP2_STATICLIB")

# enable for debugging
#target_compile_definitions(nghttp2 PUBLIC "-DDEBUGBUILD=1")

target_include_directories(nghttp2 PUBLIC ${NGHTTP2_INCLUDES})

