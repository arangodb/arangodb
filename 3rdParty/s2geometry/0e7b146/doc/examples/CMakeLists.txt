add_executable(point_index point_index.cc)
target_link_libraries(point_index LINK_PUBLIC s2testing s2)
add_executable(term_index term_index.cc)
target_link_libraries(term_index LINK_PUBLIC s2testing s2)
