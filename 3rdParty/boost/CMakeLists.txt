cmake_minimum_required(VERSION 2.6)

# ------------------------------------------------------------------------------
# V8 / ICU
# ------------------------------------------------------------------------------

################################################################################
## BOOST
################################################################################

option(USE_SYSTEM_BOOST "use libraries provided by the system" OFF)

set(BOOST_MINIMUM_VERSION 1.58)

set(Boost_USE_MULTITHREADED ON)

if (USE_SYSTEM_BOOST)
  find_package(Boost ${BOOST_MINIMUM_VERSION} REQUIRED COMPONENTS unit_test_framework)
else ()
  set(BOOST_INCLUDE ${PROJECT_SOURCE_DIR}/3rdParty/boost/${BOOST_MINIMUM_VERSION}.0 CACHE path "BOOST include path")
  set(Boost_INCLUDE_DIR ${BOOST_INCLUDE})
  set(Boost_VERSION ${BOOST_MINIMUM_VERSION})
  find_package(Boost ${BOOST_MINIMUM_VERSION} REQUIRED)
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

set(BOOST_VERSION
  "${Boost_MAJOR_VERSION}.${Boost_MINOR_VERSION}"
  CACHE INTERNAL
  "Boost: Version"
  FORCE
)

set(BOOST_INCLUDE_DIR
  "${Boost_INCLUDE_DIR}"
  CACHE INTERNAL
  "Boost: Include Directory"
)

set(BOOST_LIBS
  "${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}"
  CACHE INTERNAL
  "Boost: Libraries"
)
