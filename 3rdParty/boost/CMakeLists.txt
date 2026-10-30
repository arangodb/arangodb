include(FetchContent)

set(BOOST_VERSION "1.92.0")
set(BOOST_VERSION ${BOOST_VERSION} PARENT_SCOPE)
set(BOOST_SHA256 "9bed76128d4e46755dbe818487788c6fceb6f72b378f4daa49b7e1e600d9088d")

set(BOOST_INCLUDE_LIBRARIES
  algorithm
  asio
  container
  container_hash
  core
  crc
  lockfree
  predef
  property_tree
  range
  system
  thread
  throw_exception
  type_traits
  uuid
)

set(BOOST_FETCH_OPTIONS
  URL https://github.com/boostorg/boost/releases/download/boost-${BOOST_VERSION}/boost-${BOOST_VERSION}-cmake.tar.xz
  URL_HASH SHA256=${BOOST_SHA256}
)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
  list(APPEND BOOST_FETCH_OPTIONS DOWNLOAD_EXTRACT_TIMESTAMP OFF)
endif()
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.25")
  list(APPEND BOOST_FETCH_OPTIONS SYSTEM)
endif()
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.28")
  list(APPEND BOOST_FETCH_OPTIONS EXCLUDE_FROM_ALL)
endif()

FetchContent_Declare(Boost ${BOOST_FETCH_OPTIONS})
FetchContent_MakeAvailable(Boost)
