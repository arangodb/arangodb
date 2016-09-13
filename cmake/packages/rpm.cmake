# -*- mode: CMAKE; -*-

set(CPACK_GENERATOR "RPM")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Installation/rpm/arangodb.spec.in" "${CMAKE_CURRENT_BINARY_DIR}/arangodb.spec" @ONLY IMMEDIATE)
set(CPACK_RPM_USER_BINARY_SPECFILE "${CMAKE_CURRENT_BINARY_DIR}/arangodb.spec")


################################################################################
# This produces the RPM packages, using client/rpm.txt for the second package.
################################################################################

# deploy the Init script:
set(RPM_INIT_SCRIPT "${PROJECT_SOURCE_DIR}/Installation/rpm/rc.arangod.Centos")
set(RPM_INIT_SCRIPT_TARGET "${CMAKE_INSTALL_FULL_SYSCONFDIR}/init.d")
set(RPM_INIT_SCRIPT_TARGET_NAME arangodb3)
set(CPACK_COMPONENTS_GROUPING IGNORE)

install(
  FILES ${RPM_INIT_SCRIPT}
  PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
  DESTINATION ${RPM_INIT_SCRIPT_TARGET}
  RENAME ${RPM_INIT_SCRIPT_TARGET_NAME}
  COMPONENT debian-extras
  )

# 
set(PACKAGE_VERSION "-${CPACK_PACKAGE_VERSION}-${ARANGODB_PACKAGE_REVISION}.${ARANGODB_PACKAGE_ARCHITECTURE}")
set(CPACK_PACKAGE_FILE_NAME   "${CPACK_PACKAGE_NAME}${PACKAGE_VERSION}")
set(CPACK_CLIENT_PACKAGE_FILE_NAME "arangodb3-client${PACKAGE_VERSION}")
set(CPACK_RPM_PACKAGE_RELOCATABLE FALSE)

set(CPACK_TEMPORARY_DIRECTORY         "${PROJECT_BINARY_DIR}/_CPack_Packages/${CMAKE_SYSTEM_NAME}/RPM/RPMS/${ARANGODB_PACKAGE_ARCHITECTURE}")
set(CPACK_TEMPORARY_PACKAGE_FILE_NAME "${CPACK_TEMPORARY_DIRECTORY}/${CPACK_PACKAGE_FILE_NAME}.rpm")

add_custom_target(package-arongodb-server
  COMMAND ${CMAKE_COMMAND} .
  COMMAND ${CMAKE_CPACK_COMMAND} -G RPM
  COMMAND cp "${CPACK_TEMPORARY_DIRECTORY}/${CPACK_CLIENT_PACKAGE_FILE_NAME}.rpm" "${PROJECT_BINARY_DIR}"
  WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
list(APPEND PACKAGES_LIST package-arongodb-server)

#################################################################################
## hook to build the client package
#################################################################################
#set(CLIENT_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/packages/arangodb-client)
#configure_file(cmake/packages/client/rpm.txt ${CLIENT_BUILD_DIR}/CMakeLists.txt @ONLY)
#add_custom_target(package-arongodb-client
#  COMMAND ${CMAKE_COMMAND} .
#  COMMAND ${CMAKE_CPACK_COMMAND} -G RPM
#  COMMAND cp *.rpm ${PROJECT_BINARY_DIR} 
#  WORKING_DIRECTORY ${CLIENT_BUILD_DIR})
#
#
#list(APPEND PACKAGES_LIST package-arongodb-client)
