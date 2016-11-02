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
set(CPACK_INSTALL_PREFIX         "${CPACK_PACKAGE_NAME}.app/${CPACK_BUNDLE_PREFIX}/${CMAKE_INSTALL_PREFIX}")

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
  COMMAND cp *.dmg ${PACKAGE_TARGET_DIR})

list(APPEND COPY_PACKAGES_LIST copy_bundle_packages)
