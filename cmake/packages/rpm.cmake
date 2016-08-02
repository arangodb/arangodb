# -*- mode: CMAKE; -*-

set(CPACK_GENERATOR "RPM")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Installation/rpm/arangodb.spec.in" "${CMAKE_CURRENT_BINARY_DIR}/arangodb.spec" @ONLY IMMEDIATE)
set(CPACK_RPM_USER_BINARY_SPECFILE "${CMAKE_CURRENT_BINARY_DIR}/arangodb.spec")


################################################################################
# This produces the RPM packages, using client/rpm.txt for the second package.
################################################################################

set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${ARANGODB_PACKAGE_REVISION}_${ARANGODB_PACKAGE_ARCHITECTURE}")


# deploy the Init script:
#install(
#  FILES ${PROJECT_SOURCE_DIR}/Installation/debian/arangodb.init
#  PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
#  DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/init.d
#  RENAME arangodb3
#  COMPONENT debian-extras
#  )

################################################################################
# hook to build the server package
################################################################################
add_custom_target(package-arongodb-server
  COMMAND ${CMAKE_COMMAND} .
  COMMAND ${CMAKE_CPACK_COMMAND} -G RPM
  WORKING_DIRECTORY ${PROJECT_BINARY_DIR})

list(APPEND PACKAGES_LIST package-arongodb-server)

################################################################################
# hook to build the client package
################################################################################
set(CLIENT_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/packages/arangodb-client)
configure_file(cmake/packages/client/rpm.txt ${CLIENT_BUILD_DIR}/CMakeLists.txt @ONLY)
add_custom_target(package-arongodb-client
  COMMAND ${CMAKE_COMMAND} .
  COMMAND ${CMAKE_CPACK_COMMAND} -G RPM
  COMMAND cp *.rpm ${PROJECT_BINARY_DIR} 
  WORKING_DIRECTORY ${CLIENT_BUILD_DIR})


list(APPEND PACKAGES_LIST package-arongodb-client)
