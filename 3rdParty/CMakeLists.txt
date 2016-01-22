add_subdirectory(boost)
#add_subdirectory(etcd)
add_subdirectory(libev)
#add_subdirectory(linenoise-ng)
#add_subdirectory(valgrind)
#add_subdirectory(velocypack)
#add_subdirectory(zlib-1.2.7)

include(ExternalProject)
ExternalProject_Add(v8
    SOURCE_DIR          "${CMAKE_CURRENT_SOURCE_DIR}/V8-4.3.61"
    BUILD_IN_SOURCE     TRUE
    BUILD_COMMAND       CXX=g++ CXX_FLAGS=-std=c++11 make -f Makefile-v8 native
)

