project(nghttp2 C CXX)

# add_subdirectory(lib/includes)

set(NGHTTP2_INCLUDES
  "${CMAKE_CURRENT_SOURCE_DIR}/lib/includes"
  "${CMAKE_CURRENT_BINARY_DIR}/lib/includes"
)

#add_definitions(-DBUILDING_NGHTTP2)

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

#set(NGHTTP2_RES "")

#if(WIN32)
#  configure_file(
#    lib/version.rc.in
#    ${CMAKE_CURRENT_BINARY_DIR}/lib/version.rc
#    @ONLY)
#
#  set(NGHTTP2_RES ${CMAKE_CURRENT_BINARY_DIR}/lib/version.rc)
#endif()

add_library(nghttp2 STATIC
  ${NGHTTP2_SOURCES}
)

target_include_directories(nghttp2 PUBLIC ${NGHTTP2_INCLUDES})
