if (NOT(MSVC))
  set(CPACK_SET_DESTDIR ON)
endif()

set(CPACK_PACKAGE_VENDOR  ${ARANGODB_PACKAGE_VENDOR})
set(CPACK_PACKAGE_CONTACT ${ARANGODB_PACKAGE_CONTACT})
set(CPACK_PACKAGE_VERSION "${ARANGODB_VERSION}")
# TODO just for rpm?
set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})

set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")

set(CPACK_STRIP_FILES "ON")

set(CPACK_PACKAGE_NAME "arangodb3")
set(ARANGODB_PACKAGE_ARCHITECTURE ${CMAKE_SYSTEM_PROCESSOR})
# eventually the package string will be modified later on:
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${ARANGODB_PACKAGE_REVISION}.${ARANGODB_PACKAGE_ARCHITECTURE}")

if ("${PACKAGING}" STREQUAL "DEB")
  include(packages/deb)
elseif ("${PACKAGING}" STREQUAL "RPM")
  include(packages/rpm)
elseif ("${PACKAGING}" STREQUAL "Bundle")
  include(packages/bundle)
elseif (MSVC)
  include(packages/nsis)
endif ()

configure_file(
  "${CMAKE_SOURCE_DIR}/Installation/cmake/CMakeCPackOptions.cmake.in"
  "${CMAKE_BINARY_DIR}/CMakeCPackOptions.cmake" @ONLY)
set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_BINARY_DIR}/CMakeCPackOptions.cmake")


# Finally: user cpack
include(CPack)
