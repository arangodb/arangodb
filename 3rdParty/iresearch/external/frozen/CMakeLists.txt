cmake_minimum_required(VERSION 3.8)
#
# Here we check whether frozen is being configured in isolation or as a component
# of a larger proeject. To do so, we query whether the `PROJECT_NAME` CMake
# variable has been defined. In the case it has, we can conclude frozen is a
# subproject.
#
# This convention has been borrowed from the Catch C++ unit testing library.
#
if(DEFINED PROJECT_NAME)
  set(subproject ON)
  set(INSTALL_SUBPROJECTS ON CACHE BOOL "Install subproject dependencies")
else()
  set(subproject OFF)
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

project(frozen VERSION 1.1.0 LANGUAGES CXX)
include(CMakeDependentOption)
include(CMakePackageConfigHelpers) # provides `write_basic_package_version_file`
include(CTest)
include(GNUInstallDirs)

#
# The `frozen.testing`, `frozen.benchmark`, and `frozen.coverage` options
# only appear as cmake-gui and ccmake options iff frozen is the highest
# level project. In the case that frozen is a subproject, these options are
# hidden from the user interface and set to `OFF`
#
CMAKE_DEPENDENT_OPTION(frozen.tests
  "Build the frozen tests and integrate with ctest"
  ${BUILD_TESTING} "NOT subproject" OFF)

CMAKE_DEPENDENT_OPTION(frozen.benchmark
  "Build the frozen benchmark"
  OFF "NOT subproject" OFF)

CMAKE_DEPENDENT_OPTION(frozen.coverage
  "Enable test coverage collection of frozen tests"
  OFF "NOT subproject" OFF)

#
# When the end user is consuming frozen as a nested subproject, an option
# is provided such that the user may exlude frozen from the set of installed
# cmake projects. This accomodates end users building executables or
# compiled libraries which privately link to frozen, but wish to only ship their
# artifacts in an installation
#
CMAKE_DEPENDENT_OPTION(frozen.installation
  "Include frozen in the install set"
  "${INSTALL_SUBPROJECTS}" "subproject" ON)
mark_as_advanced(frozen.installation)

#
# frozen has no compiled components. As such, we declare it as an `INTERFACE`
# library, which denotes a collection of target properties to be applied
# transitively to linking targets. In our case, this amounts to an include
# directory and project header files.
#
add_library(frozen INTERFACE)
add_library(frozen::frozen ALIAS frozen)

add_library(frozen-headers INTERFACE)
add_library(frozen::frozen-headers ALIAS frozen-headers)

target_link_libraries(frozen-headers INTERFACE frozen::frozen)

#
# frozen requires C++ 14 support, at a minimum. Setting the `cxx_std_14` compile
# features ensures that the corresponding C++ standard flag is populated in
# targets linking to frozen
#
target_compile_features(frozen INTERFACE cxx_std_14)

#
# The include directory for frozen can be expected to vary between build
# and installaion. Here we use a CMake generator expression to dispatch
# on how the configuration under which this library is being consumed.
#

string(CONCAT prefix
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

target_include_directories(frozen INTERFACE ${prefix})
add_subdirectory(include/frozen)

if(frozen.tests)
  add_subdirectory(tests)
  add_subdirectory(examples)
endif()

if(frozen.benchmark)
  add_subdirectory(benchmarks)
endif()

if(frozen.installation)
  #
  # As a header-only library, there are no target components to be installed
  # directly (the PUBLIC_HEADER property is not white listed for INTERFACE
  # targets for some reason).
  #
  # However, it is worthwhile export our target description in order to later
  # generate a CMake configuration file for consumption by CMake's `find_package`
  # intrinsic
  #
  install(TARGETS frozen frozen-headers EXPORT frozenConfig)

  #
  # Non-testing header files (preserving relative paths) are installed to the
  # `include` subdirectory of the `$INSTALL_DIR/${CMAKE_INSTALL_PREFIX}`
  # directory. Source file permissions preserved.
  #
  install(DIRECTORY include/
    DESTINATION include
    USE_SOURCE_PERMISSIONS
    FILES_MATCHING PATTERN "*.h")

  #
  # As a header-only library, there are no target components to be installed
  # directly (the PUBLIC_HEADER property is not white listed for INTERFACE
  # targets for some reason).
  #
  # However, it is worthwhile export our target description in order to later
  # generate a CMake configuration file for consumption by CMake's `find_package`
  # intrinsic
  #
  write_basic_package_version_file("frozenConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

  install(FILES "${PROJECT_BINARY_DIR}/frozenConfigVersion.cmake"
    DESTINATION share/cmake/frozen)

  install(EXPORT frozenConfig
    FILE frozenConfig.cmake
    NAMESPACE frozen::
    DESTINATION share/cmake/frozen)

  #
  # Rudimentary CPack support.
  #
  # CPack provides a mechanism to generate installation packaging for a project,
  # e.g., self-extracting shell scripts, compressed tarballs, Debian Package files,
  # RPM Package Manager files, Windows NSIS installation wizards,
  # Apple Disk Images (.dmg), etc.
  #
  # A packaged installation can be generated by calling
  #
  # ```sh
  # cpack -G <packaging type> --config CPackConfig.cmake
  # ```
  #
  # See `cpack --help` or the CPack documentation for more information.
  #
  if(NOT subproject)
    set(CPACK_PACKAGE_VENDOR "Quarkslab")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
    set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
    set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "a C++14 header-only, constexpr alternative to gperf")
    set(CMAKE_PROJECT_HOMEPAGE_URL "https://blog.quarkslab.com/frozen-an-header-only-constexpr-alternative-to-gperf-for-c14-users.html")
    include(CPack)
  endif()
endif()
