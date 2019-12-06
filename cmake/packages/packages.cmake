if (NOT(MSVC))
  set(CPACK_SET_DESTDIR ON)
endif()

set(CPACK_PACKAGE_VENDOR  ${ARANGODB_PACKAGE_VENDOR})
set(CPACK_PACKAGE_CONTACT ${ARANGODB_PACKAGE_CONTACT})

# TODO just for rpm?
if (NOT(MSVC))
  set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
endif()

if (USE_ENTERPRISE)
  set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/enterprise/LICENSE")
else ()
  set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
endif ()

set(CPACK_STRIP_FILES "ON")

if (${USE_ENTERPRISE})
  set(CPACKG_PACKAGE_CONFLICTS "arangodb3")
  set(CPACK_PACKAGE_NAME "arangodb3e")
else ()
  set(CPACK_PACKAGE_NAME "arangodb3")
  set(CPACKG_PACKAGE_CONFLICTS "arangodb3e")
endif ()
set(ARANGODB_PACKAGE_ARCHITECTURE ${CMAKE_SYSTEM_PROCESSOR})
# eventually the package string will be modified later on:

set(LOGROTATE_GROUP "arangodb")

if ("${PACKAGING}" STREQUAL "DEB")
  set(CPACK_PACKAGE_VERSION "${ARANGODB_DEBIAN_UPSTREAM}")

  if(CMAKE_TARGET_ARCHITECTURES MATCHES ".*x86_64.*")
    set(ARANGODB_PACKAGE_ARCHITECTURE "amd64")
  elseif(CMAKE_TARGET_ARCHITECTURES MATCHES "aarch64")
    set(ARANGODB_PACKAGE_ARCHITECTURE "arm64")
  elseif(CMAKE_TARGET_ARCHITECTURES MATCHES "armv7")
    set(ARANGODB_PACKAGE_ARCHITECTURE "armhf")
  else()
    set(ARANGODB_PACKAGE_ARCHITECTURE "i386")
  endif()
  
  set(CPACK_PACKAGE_FILE_NAME
    "${CPACK_PACKAGE_NAME}-${ARANGODB_DEBIAN_UPSTREAM}-${ARANGODB_DEBIAN_REVISION}_${ARANGODB_PACKAGE_ARCHITECTURE}")

  include(packages/deb)
  include(packages/tar)

  if (USE_SNAPCRAFT)
    if(NOT DEFINED SNAP_PORT)
      set(SNAP_PORT 8529)
    endif()
    include(packages/snap)
  endif ()
elseif ("${PACKAGING}" STREQUAL "RPM")
  set(CPACK_PACKAGE_VERSION "${ARANGODB_RPM_UPSTREAM}")

  set(PACKAGE_VERSION "-${CPACK_PACKAGE_VERSION}-${ARANGODB_RPM_REVISION}.${ARANGODB_PACKAGE_ARCHITECTURE}")
  set(CPACK_PACKAGE_FILE_NAME  "${CPACK_PACKAGE_NAME}${PACKAGE_VERSION}")
  include(packages/rpm)
  include(packages/tar)
elseif ("${PACKAGING}" STREQUAL "Bundle")
  set(CPACK_PACKAGE_VERSION "${ARANGODB_DARWIN_UPSTREAM}")

  if ("${ARANGODB_DARWIN_REVISION}" STREQUAL "")
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}.x86_64")
  else()
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${ARANGODB_DARWIN_REVISION}.x86_64")
  endif()
  include(packages/bundle)
  include(packages/tar)
elseif (MSVC)
  set(CPACK_PACKAGE_VERSION "${ARANGODB_WINDOWS_UPSTREAM}")

  if (${USE_ENTERPRISE})
    set(CPACK_PACKAGE_NAME "ArangoDB3e")
  else()
    set(CPACK_PACKAGE_NAME "ArangoDB3")
  endif()
  if (CMAKE_CL_64)
    SET(ARANGODB_PACKAGE_ARCHITECTURE "win64")
  else ()
    SET(ARANGODB_PACKAGE_ARCHITECTURE "win32")
  endif ()
  set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}_${ARANGODB_PACKAGE_ARCHITECTURE}")
  include(packages/nsis)
  include(packages/tar)
else ()
  set(CPACK_PACKAGE_VERSION "${ARANGODB_VERSION}")
  set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-1_${ARANGODB_PACKAGE_ARCHITECTURE}")
  include(packages/tar)
endif ()

if (UNIX)
  if (SYSTEMD_FOUND)
    # configure and install logrotate file
    configure_file (
      ${ARANGODB_SOURCE_DIR}/Installation/logrotate.d/arangod.systemd
      ${PROJECT_BINARY_DIR}/arangod.systemd
      NEWLINE_STYLE UNIX
      )
    install(
      FILES ${PROJECT_BINARY_DIR}/arangod.systemd
      PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
      DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/logrotate.d
      RENAME ${SERVICE_NAME}
      )
  else ()
    configure_file (
      ${ARANGODB_SOURCE_DIR}/Installation/logrotate.d/arangod.sysv
      ${PROJECT_BINARY_DIR}/arangod.sysv
      NEWLINE_STYLE UNIX
      )
  endif ()
endif ()


configure_file(
  "${CMAKE_SOURCE_DIR}/Installation/cmake/CMakeCPackOptions.cmake.in"
  "${CMAKE_BINARY_DIR}/CMakeCPackOptions.cmake" @ONLY)
set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_BINARY_DIR}/CMakeCPackOptions.cmake")


# Finally: user cpack
include(CPack)
