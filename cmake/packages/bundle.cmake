message("enabling MacOSX 'Bundle' package")

if (${USE_ENTERPRISE})
  set(CPACK_PACKAGE_NAME           "ArangoDB3e-CLI")
else()
  set(CPACK_PACKAGE_NAME           "ArangoDB3-CLI")
endif()
set(CPACK_BUNDLE_NAME            "${CPACK_PACKAGE_NAME}")

set(CPACK_BUNDLE_ICON            "${PROJECT_SOURCE_DIR}/Installation/MacOSX/Bundle/icon.icns")

configure_file("${PROJECT_SOURCE_DIR}/Installation/MacOSX/Bundle/Info.plist.in" "${CMAKE_CURRENT_BINARY_DIR}/Info.plist")
set(CPACK_BUNDLE_PLIST           "${CMAKE_CURRENT_BINARY_DIR}/Info.plist")

set(CPACK_BUNDLE_PREFIX          "Contents/Resources")
set(CPACK_BUNDLE_APPLE_CERT_APP  "Developer ID Application: ArangoDB GmbH (W7UC4UQXPV)")
set(CPACK_INSTALL_PREFIX         "${CPACK_PACKAGE_NAME}.app/${CPACK_BUNDLE_PREFIX}${CMAKE_INSTALL_PREFIX}")

set(SYSTEM_STATE_DIR "/${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LOCALSTATEDIR}")
set(CPACK_ARANGO_PID_DIR "/${SYSTEM_STATE_DIR}/run")
set(CPACK_INSTALL_SYSCONFDIR "/Library/ArangoDB-${CMAKE_INSTALL_SYSCONFDIR}")
set(CPACK_INSTALL_FULL_SYSCONFDIR "${CMAKE_INSTALL_FULL_SYSCONFDIR_ARANGO}")

set(CPACK_ARANGO_STATE_DIR "/Library/ArangoDB/${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LOCALSTATEDIR}")
set(CPACK_ARANGO_DATA_DIR "${CPACK_ARANGO_STATE_DIR}/lib/arangodb3")
set(CPACK_ARANGO_LOG_DIR "${CPACK_ARANGO_STATE_DIR}/log/arangodb3")
set(CPACK_ARANGODB_APPS_DIRECTORY "${CPACK_ARANGO_STATE_DIR}/lib/arangodb3-apps")

# The Following lines are needed to create the paths used in the initial script.

# Create INC_CPACK_ARANGODB_APPS_DIRECTORY
to_native_path("CPACK_ARANGODB_APPS_DIRECTORY")
# Create INC_CMAKE_INSTALL_DATAROOTDIR_ARANGO
to_native_path("CMAKE_INSTALL_DATAROOTDIR_ARANGO")
# Create INC_CPACK_INSTALL_SYSCONFDIR
to_native_path("CPACK_INSTALL_SYSCONFDIR")
# Create INC_CPACK_ARANGO_PID_DIR
to_native_path("CPACK_ARANGO_PID_DIR")
# Create INC_CPACK_ARANGO_DATA_DIR
to_native_path("CPACK_ARANGO_DATA_DIR")
# Create INC_CPACK_ARANGO_STATE_DIR
to_native_path("CPACK_ARANGO_STATE_DIR")
# Create INC_CPACK_ARANGO_LOG_DIR
to_native_path("CPACK_ARANGO_LOG_DIR")
# Create INC_CPACK_INSTALL_FULL_SYSCONFDIR
to_native_path("CPACK_INSTALL_FULL_SYSCONFDIR")


configure_file("${PROJECT_SOURCE_DIR}/Installation/MacOSX/Bundle/arangodb-cli.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/arangodb-cli.sh"
  @ONLY)


set(CPACK_BUNDLE_STARTUP_COMMAND "${CMAKE_CURRENT_BINARY_DIR}/arangodb-cli.sh")

add_custom_target(package-arangodb-server-bundle
  COMMAND ${CMAKE_COMMAND} .
  COMMAND ${CMAKE_CPACK_COMMAND} -G Bundle -C ${CMAKE_BUILD_TYPE}
  WORKING_DIRECTORY ${PROJECT_BINARY_DIR})

list(APPEND PACKAGES_LIST package-arangodb-server-bundle)

add_custom_target(copy_bundle_packages
  COMMAND ${CMAKE_COMMAND} -E copy ${CPACK_PACKAGE_FILE_NAME}.dmg ${PACKAGE_TARGET_DIR})

list(APPEND COPY_PACKAGES_LIST copy_bundle_packages)

add_custom_target(remove_packages
  COMMAND ${CMAKE_COMMAND} -E remove ${CPACK_PACKAGE_FILE_NAME}.dmg
  COMMAND ${CMAKE_COMMAND} -E remove_directory _CPack_Packages
  )

list(APPEND CLEAN_PACKAGES_LIST remove_packages)
