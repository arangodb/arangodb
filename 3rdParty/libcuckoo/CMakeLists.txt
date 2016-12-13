cmake_minimum_required (VERSION 2.8)

include_directories("${PROJECT_BINARY_DIR}/include/")
#target_link_libraries(LIBCUCKOO pthread)
add_library(libcuckoo include/cityhash/city.cc)
