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

FetchContent_Declare(
  Boost
  URL https://github.com/boostorg/boost/releases/download/boost-${BOOST_VERSION}/boost-${BOOST_VERSION}-cmake.tar.xz
  URL_HASH SHA256=${BOOST_SHA256}
  DOWNLOAD_EXTRACT_TIMESTAMP OFF
  EXCLUDE_FROM_ALL
  SYSTEM
)
FetchContent_MakeAvailable(Boost)
