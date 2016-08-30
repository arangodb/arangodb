# -*- mode: CMAKE; -*-
################################################################################
# This produces the debian packages, using client/deb.txt for the second package.
################################################################################
FILE(READ "${PROJECT_SOURCE_DIR}/Installation/debian/packagedesc.txt" CPACK_DEBIAN_PACKAGE_DESCRIPTION)
set(CPACK_DEBIAN_PACKAGE_SECTION "database")
set(CPACK_DEBIAN_PACKAGE_CONFLICTS "arangodb, arangodb3-client")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_COMPRESSION_TYPE "xz")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE ${ARANGODB_URL_INFO_ABOUT})
set(CPACK_COMPONENTS_ALL debian-extras)
set(CPACK_GENERATOR "DEB")

list(APPEND CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
  "${PROJECT_SOURCE_DIR}/Installation/debian/templates"
  "${PROJECT_SOURCE_DIR}/Installation/debian/config"
  "${PROJECT_SOURCE_DIR}/Installation/debian/postinst"
  "${PROJECT_SOURCE_DIR}/Installation/debian/preinst"
  "${PROJECT_SOURCE_DIR}/Installation/debian/postrm"
  "${PROJECT_SOURCE_DIR}/Installation/debian/prerm;")

if(CMAKE_TARGET_ARCHITECTURES MATCHES ".*x86_64.*")
  set(ARANGODB_PACKAGE_ARCHITECTURE "amd64")
elseif(CMAKE_TARGET_ARCHITECTURES MATCHES "aarch64")
  set(ARANGODB_PACKAGE_ARCHITECTURE "arm64")
elseif(CMAKE_TARGET_ARCHITECTURES MATCHES "armv7")
  set(ARANGODB_PACKAGE_ARCHITECTURE "armhf")
else()
  set(ARANGODB_PACKAGE_ARCHITECTURE "i386")
endif()
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE ${ARANGODB_PACKAGE_ARCHITECTURE})
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${ARANGODB_PACKAGE_REVISION}_${ARANGODB_PACKAGE_ARCHITECTURE}")


# deploy the Init script:
install(
  FILES ${PROJECT_SOURCE_DIR}/Installation/debian/arangodb.init
  PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
  DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/init.d
  RENAME arangodb3
  COMPONENT debian-extras
  )

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
set(CLIENT_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/packages/arangodb-client)
configure_file(cmake/packages/client/deb.txt ${CLIENT_BUILD_DIR}/CMakeLists.txt @ONLY)
add_custom_target(package-arongodb-client
  COMMAND ${CMAKE_COMMAND} .
  COMMAND ${CMAKE_CPACK_COMMAND} -G DEB
  COMMAND cp *.deb ${PROJECT_BINARY_DIR} 
  WORKING_DIRECTORY ${CLIENT_BUILD_DIR})


list(APPEND PACKAGES_LIST package-arongodb-client)
