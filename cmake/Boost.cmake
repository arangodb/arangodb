cmake_minimum_required(VERSION 2.6)

################################################################################
## BOOST
################################################################################

set(BOOST_VERSION 1.61.0b1)

message(STATUS "using 3rdParty BOOST")
option(USE_BOOST_UNITTESTS "use boost unit-tests" ON)

set(Boost_VERSION "${BOOST_MINIMUM_VERSION}")
set(Boost_INCLUDE_DIR
  "${PROJECT_SOURCE_DIR}/3rdParty/boost/${BOOST_MINIMUM_VERSION}"
)

if (NOT USE_BOOST_UNITTESTS)
  message(STATUS "BOOST unit-tests are disabled")
endif ()
