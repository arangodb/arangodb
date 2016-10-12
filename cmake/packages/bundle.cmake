message("enabling MacOSX 'Bundle' package")
if (${USE_ENTERPRISE})
  set(CPACK_PACKAGE_NAME           "ArangoDB3e-CLI")
else()
  set(CPACK_PACKAGE_NAME           "ArangoDB3-CLI")
endif()
set(CPACK_BUNDLE_NAME            "${CPACK_PACKAGE_NAME}")
set(CPACK_BUNDLE_PLIST           "${CMAKE_CURRENT_BINARY_DIR}/Info.plist")
set(CPACK_BUNDLE_ICON            "${PROJECT_SOURCE_DIR}/Installation/MacOSX/Bundle/icon.icns")
set(CPACK_BUNDLE_STARTUP_COMMAND "${PROJECT_SOURCE_DIR}/Installation/MacOSX/Bundle/arangodb-cli.sh")
configure_file("${PROJECT_SOURCE_DIR}/Installation/MacOSX/Bundle/Info.plist.in" "${CMAKE_CURRENT_BINARY_DIR}/Info.plist")

add_custom_target(package-arongodb-server-bundle
  COMMAND ${CMAKE_COMMAND} .
  COMMAND ${CMAKE_CPACK_COMMAND} -G Bundle -C ${CMAKE_BUILD_TYPE}
  WORKING_DIRECTORY ${PROJECT_BINARY_DIR})

list(APPEND PACKAGES_LIST package-arongodb-server-nsis)

add_custom_target(copy_packages
  COMMAND cp *.dmg ${PACKAGE_TARGET_DIR})
