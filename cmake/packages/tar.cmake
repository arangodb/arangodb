
################################################################################
## generic tarball
################################################################################
set(CPACK_PACKAGE_TGZ "${CMAKE_BINARY_DIR}/${CPACK_PACKAGE_FILE_NAME}.tar.gz")
add_custom_target(TGZ_package
  COMMENT "create TGZ-package"
  COMMAND ${CMAKE_CPACK_COMMAND} -G TGZ -C ${CMAKE_BUILD_TYPE}
  )

add_custom_target(remove_tgz_packages
  COMMAND ${CMAKE_COMMAND} -E remove_directory _CPack_Packages
  COMMENT Removing server packaging build directory
  COMMAND ${CMAKE_COMMAND} -E remove ${CPACK_PACKAGE_TGZ}
  COMMENT Removing local tgz packages
  )

list(APPEND CLEAN_PACKAGES_LIST remove_tgz_packages)

