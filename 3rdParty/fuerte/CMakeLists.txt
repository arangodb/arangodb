project(fuerte)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(fuerte STATIC
  src/connection.cpp
  src/ConnectionBuilder.cpp
  src/helper.cpp
  src/http.cpp
  src/H1Connection.cpp
  src/H2Connection.cpp
  src/jwt.cpp
  src/loop.cpp
  src/message.cpp
  src/requests.cpp
  src/types.cpp
  src/connection.cpp)

# TODO: This is not enough to link fuerte on its own
target_link_libraries(fuerte PUBLIC
  nghttp2
  llhttp
  boost_system
  absl::strings
  velocypack)

include(CheckTypeSize)
check_type_size("ssize_t" SIZEOF_SSIZE_T)
if(SIZEOF_SSIZE_T STREQUAL "")
  # ssize_t is a signed type in POSIX storing at least -1.
  # Set it to "int" to match the behavior of AC_TYPE_SSIZE_T (autotools).
  target_compile_definitions(fuerte PRIVATE ssize_t=int)
endif()

target_include_directories(fuerte
  PUBLIC
    src
  SYSTEM PUBLIC
    include
    "${OPENSSL_INCLUDE_DIR}")
