cmake_minimum_required(VERSION 2.6)

# ------------------------------------------------------------------------------
# V8 / ICU
# ------------------------------------------------------------------------------

################################################################################
## BOOST
################################################################################

option(USE_SYSTEM_BOOST "use libraries provided by the system" OFF)

set(BOOST_MINIMUM_VERSION 1.58)
if (USE_SYSTEM_BOOST)
  find_package(Boost ${BOOST_MINIMUM_VERSION} REQUIRED)
  FIND_PACKAGE(Boost COMPONENTS unit_test_framework)
  set(Boost_USE_MULTITHREADED ON)
else ()
  set(Boost_VERSION "${BOOST_MINIMUM_VERSION}")
  set(Boost_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/3rdParty/boost/${BOOST_MINIMUM_VERSION}.0")
endif ()

if (Boost_UNIT_TEST_FRAMEWORK_FOUND)
  try_compile(
    HAVE_USABLE_BOOST_LIBRARIES
      "${CMAKE_BINARY_DIR}/temp"
      "${CMAKE_SOURCE_DIR}/cmake/test_boost.cpp"
    LINK_LIBRARIES
      ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}
    CMAKE_FLAGS
      "-DINCLUDE_DIRECTORIES=${Boost_INCLUDE_DIR}"
  )

  if (HAVE_USABLE_BOOST_LIBRARIES)
    option(USE_BOOST_UNITTESTS "use boost unit-tests" ON)
  else ()
    message(STATUS "cannot use BOOST library ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}")
    option(USE_BOOST_UNITTESTS "use boost unit-tests" OFF)
  endif ()
else ()
  option(USE_BOOST_UNITTESTS "use boost unit-tests" OFF)
endif ()

if (NOT USE_BOOST_UNITTESTS)
  message(STATUS "BOOST unit-tests are disabled")
endif ()
