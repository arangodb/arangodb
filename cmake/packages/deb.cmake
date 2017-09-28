# -*- mode: CMAKE; -*-

# we will handle install into the target directory on our own via debconf:
set(PACKAGING_HANDLE_CONFIG_FILES true)

################################################################################
# This produces the debian packages, using client/deb.txt for the second package.
################################################################################

FILE(READ "${PROJECT_SOURCE_DIR}/Installation/debian/packagedesc.txt" CPACK_DEBIAN_PACKAGE_DESCRIPTION)
set(CPACK_DEBIAN_PACKAGE_SECTION "database")
set(CPACK_DEBIAN_PACKAGE_CONFLICTS "arangodb, ${CPACKG_PACKAGE_CONFLICTS}, ${CPACKG_PACKAGE_CONFLICTS}-client, ${CPACK_PACKAGE_NAME}-client")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
if(NOT DISABLE_XZ_DEB)
  set(CPACK_DEBIAN_COMPRESSION_TYPE "xz")
endif()
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE ${ARANGODB_URL_INFO_ABOUT})
set(CPACK_COMPONENTS_ALL debian-extras)
set(CPACK_GENERATOR "DEB")

if (SYSTEMD_FOUND)
  set(CPACK_SYSTEMD_FOUND "1")
else()
  set(CPACK_SYSTEMD_FOUND "0")
endif()

# substitute the package name so debconf works:
configure_file (
  "${PROJECT_SOURCE_DIR}/Installation/debian/templates.in"
  "${PROJECT_BINARY_DIR}/Installation/debian/templates"
  NEWLINE_STYLE UNIX)
configure_file (
  "${PROJECT_SOURCE_DIR}/Installation/debian/config.in"
  "${PROJECT_BINARY_DIR}/Installation/debian/config"
  NEWLINE_STYLE UNIX)
configure_file (
  "${PROJECT_SOURCE_DIR}/Installation/debian/postinst.in"
  "${PROJECT_BINARY_DIR}/Installation/debian/postinst"
  NEWLINE_STYLE UNIX)
configure_file (
  "${PROJECT_SOURCE_DIR}/Installation/debian/prerm.in"
  "${PROJECT_BINARY_DIR}/Installation/debian/prerm"
  NEWLINE_STYLE UNIX)
  
list(APPEND CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
  "${PROJECT_BINARY_DIR}/Installation/debian/templates"
  "${PROJECT_BINARY_DIR}/Installation/debian/config"
  "${PROJECT_BINARY_DIR}/Installation/debian/postinst"
  
  "${PROJECT_SOURCE_DIR}/Installation/debian/preinst"
  "${PROJECT_SOURCE_DIR}/Installation/debian/postrm")

################################################################################
# specify which target archcitecture the package is going to be:
################################################################################

set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE ${ARANGODB_PACKAGE_ARCHITECTURE})

set(ARANGODB_DBG_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-dbg-${CPACK_PACKAGE_VERSION}-${ARANGODB_PACKAGE_REVISION}_${ARANGODB_PACKAGE_ARCHITECTURE}")

set(conffiles_list "")
if ("${INSTALL_CONFIGFILES_LIST}" STREQUAL "")
  message("the list is empty in this turn")
else()
  list(REMOVE_DUPLICATES INSTALL_CONFIGFILES_LIST)
  foreach (_configFile ${INSTALL_CONFIGFILES_LIST})
    list(APPEND CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA ${_configFile})
    set(conffiles_list "${conffiles_list}${_configFile}\n")
  endforeach()
endif()

set(DH_CONFFILES_NAME "${PROJECT_BINARY_DIR}/conffiles")
FILE(WRITE ${DH_CONFFILES_NAME} "${conffiles_list}")
list(APPEND CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${DH_CONFFILES_NAME}")


if (SYSTEMD_FOUND)
  # deploy the Init script:
  install(
    FILES ${PROJECT_BINARY_DIR}/arangodb3.service
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    DESTINATION ${SYSTEMD_UNIT_DIR}/
    RENAME ${SERVICE_NAME}.service
    COMPONENT debian-extras
    )

  # deploy the logrotate config:
  install(
    FILES ${PROJECT_BINARY_DIR}/arangod.systemd
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/logrotate.d
    RENAME ${SERVICE_NAME}
    COMPONENT debian-extras
    )
else ()
  # deploy the Init script:
  install(
    FILES ${PROJECT_SOURCE_DIR}/Installation/debian/arangodb.init
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/init.d
    RENAME arangodb3
    COMPONENT debian-extras
    )

  # deploy the logrotate config:
  install(
    FILES ${PROJECT_BINARY_DIR}/arangod.sysv
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/logrotate.d
    RENAME ${SERVICE_NAME}
    COMPONENT debian-extras
    )
endif()

################################################################################
# hook to build the server package
################################################################################
add_custom_target(package-arongodb-server
  COMMAND ${CMAKE_COMMAND} .
  COMMAND ${CMAKE_CPACK_COMMAND} -G DEB
  WORKING_DIRECTORY ${PROJECT_BINARY_DIR})

list(APPEND PACKAGES_LIST package-arongodb-server)

################################################################################
# hook to build the client package
################################################################################
set(CPACK_CLIENT_PACKAGE_NAME "${CPACK_PACKAGE_NAME}-client")

set(ARANGODB_CLIENT_PACKAGE_FILE_NAME "${CPACK_CLIENT_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${ARANGODB_PACKAGE_REVISION}_${ARANGODB_PACKAGE_ARCHITECTURE}")

set(CLIENT_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/packages/arangodb-client)
configure_file(cmake/packages/client/deb.txt ${CLIENT_BUILD_DIR}/CMakeLists.txt @ONLY)
add_custom_target(package-arongodb-client
  COMMAND ${CMAKE_COMMAND} .
  COMMENT "configuring client package environment"
  COMMAND ${CMAKE_CPACK_COMMAND} -G DEB
  COMMENT "building client packages"
  COMMAND ${CMAKE_COMMAND} -E copy ${CLIENT_BUILD_DIR}/${ARANGODB_CLIENT_PACKAGE_FILE_NAME}.deb ${PROJECT_BINARY_DIR}
  COMMENT "uploading client packages"
  WORKING_DIRECTORY ${CLIENT_BUILD_DIR})

list(APPEND PACKAGES_LIST package-arongodb-client)


add_custom_target(copy_deb_packages
  COMMAND ${CMAKE_COMMAND} -E copy ${ARANGODB_CLIENT_PACKAGE_FILE_NAME}.deb ${PACKAGE_TARGET_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy ${CPACK_PACKAGE_FILE_NAME}.deb           ${PACKAGE_TARGET_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy ${ARANGODB_DBG_PACKAGE_FILE_NAME}.deb    ${PACKAGE_TARGET_DIR}
  COMMENT "copying packages to ${PACKAGE_TARGET_DIR}")

list(APPEND COPY_PACKAGES_LIST copy_deb_packages)

add_custom_target(remove_packages
  COMMAND ${CMAKE_COMMAND} -E remove_directory _CPack_Packages
  COMMENT Removing server packaging build directory
  COMMAND ${CMAKE_COMMAND} -E remove_directory packages
  COMMENT Removing client packaging build directory
  COMMAND ${CMAKE_COMMAND} -E remove ${ARANGODB_CLIENT_PACKAGE_FILE_NAME}.deb
  COMMAND ${CMAKE_COMMAND} -E remove ${CPACK_PACKAGE_FILE_NAME}.deb
  COMMAND ${CMAKE_COMMAND} -E remove ${ARANGODB_DBG_PACKAGE_FILE_NAME}.deb
  COMMAND ${CMAKE_COMMAND} -E remove ${PROJECT_BINARY_DIR}/bin/strip/*
  COMMENT Removing local target packages
  )

list(APPEND CLEAN_PACKAGES_LIST remove_packages)


################################################################################
# hook to build the debug package
################################################################################
set(DEBUG_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/packages/arangodb3-dbg)
configure_file(cmake/packages/dbg/deb.txt ${DEBUG_BUILD_DIR}/CMakeLists.txt @ONLY)

add_custom_target(package-arongodb-dbg
  COMMAND ${CMAKE_COMMAND} . -DCMAKE_OBJCOPY=${CMAKE_OBJCOPY}
  COMMAND ${CMAKE_CPACK_COMMAND} -G DEB
  COMMAND ${CMAKE_COMMAND} -E copy ${ARANGODB_DBG_PACKAGE_FILE_NAME}.deb ${PROJECT_BINARY_DIR}
  WORKING_DIRECTORY ${DEBUG_BUILD_DIR})

list(APPEND PACKAGES_LIST package-arongodb-dbg)
