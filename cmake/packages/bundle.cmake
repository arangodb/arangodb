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

set(CPACK_BUNDLE_PREFIX          "Contents/MacOS")
set(CPACK_BUNDLE_APPLE_CERT_APP  "Developer ID Application: ArangoDB GmbH (W7UC4UQXPV)")
set(CPACK_INSTALL_PREFIX         "${CPACK_PACKAGE_NAME}.app/${CPACK_BUNDLE_PREFIX}${CMAKE_INSTALL_PREFIX}")

set(INST_USR_LIBDIR "/Library/ArangoDB")
set(CPACK_ARANGO_PID_DIR "/${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LOCALSTATEDIR}/run")
set(CPACK_ARANGO_DATA_DIR "${INST_USR_LIBDIR}${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LOCALSTATEDIR}/lib/arangodb3")
set(CPACK_ARANGO_LOG_DIR "${INST_USR_LIBDIR}${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LOCALSTATEDIR}/log/arangodb3")
set(CPACK_ARANGO_STATE_DIR "${INST_USR_LIBDIR}${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LOCALSTATEDIR}")
set(CPACK_INSTALL_SYSCONFDIR "/Library/ArangoDB-${CMAKE_INSTALL_SYSCONFDIR}")
set(CPACK_INSTALL_FULL_SYSCONFDIR "${CMAKE_INSTALL_FULL_SYSCONFDIR_ARANGO}")
set(CPACK_ARANGODB_APPS_DIRECTORY "${INST_USR_LIBDIR}${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LOCALSTATEDIR}/lib/arangodb3-apps")
to_native_path("CPACK_ARANGODB_APPS_DIRECTORY")
to_native_path("CMAKE_INSTALL_DATAROOTDIR_ARANGO")
to_native_path("CPACK_INSTALL_SYSCONFDIR")
to_native_path("CPACK_ARANGO_PID_DIR")
to_native_path("CPACK_ARANGO_DATA_DIR")
to_native_path("CPACK_ARANGO_STATE_DIR")
to_native_path("CPACK_ARANGO_LOG_DIR")
to_native_path("CPACK_INSTALL_FULL_SYSCONFDIR")

# we wrap HDIUTIL to inject our own parameter:
find_program(HDIUTIL_EXECUTABLE hdiutil)
# for now 260MB seems to be enough:
set(CMAKE_DMG_SIZE 260)
configure_file("${PROJECT_SOURCE_DIR}/Installation/MacOSX/Bundle/hdiutilwrapper.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/hdiutilwrapper.sh"
  @ONLY)
set(CPACK_COMMAND_HDIUTIL "${CMAKE_CURRENT_BINARY_DIR}/hdiutilwrapper.sh")


configure_file("${PROJECT_SOURCE_DIR}/Installation/MacOSX/Bundle/arangodb-cli.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/arangodb-cli.sh"
  @ONLY)


set(CPACK_BUNDLE_STARTUP_COMMAND "${CMAKE_CURRENT_BINARY_DIR}/arangodb-cli.sh")

add_custom_target(package-arongodb-server-bundle
  COMMAND ${CMAKE_COMMAND} .
  COMMAND ${CMAKE_CPACK_COMMAND} -G Bundle -C ${CMAKE_BUILD_TYPE}
  WORKING_DIRECTORY ${PROJECT_BINARY_DIR})

list(APPEND PACKAGES_LIST package-arongodb-server-bundle)

add_custom_target(copy_bundle_packages
  COMMAND ${CMAKE_COMMAND} -E copy ${CPACK_PACKAGE_FILE_NAME}.dmg ${PACKAGE_TARGET_DIR})

list(APPEND COPY_PACKAGES_LIST copy_bundle_packages)

add_custom_target(remove_packages
  COMMAND ${CMAKE_COMMAND} -E remove ${CPACK_PACKAGE_FILE_NAME}.dmg
  COMMAND ${CMAKE_COMMAND} -E remove_directory _CPack_Packages
  )

list(APPEND CLEAN_PACKAGES_LIST remove_packages)
