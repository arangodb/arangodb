
################################################################################
## generic tarball
################################################################################
set(CPACK_PACKAGE_FILE_NAME "${ARANGODB_BASIC_PACKAGE_FILE_NAME}")
set(CPACK_PACKAGE_TGZ "${CMAKE_BINARY_DIR}/${ARANGODB_BASIC_PACKAGE_FILE_NAME}.tar.gz")
add_custom_target(TGZ_package
  COMMENT "create TGZ-package"
  COMMAND ${CMAKE_CPACK_COMMAND} -G TGZ  -C ${CMAKE_BUILD_TYPE}
  )
