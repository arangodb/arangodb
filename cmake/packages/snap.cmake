
find_program (SNAP_EXE snapcraft)
if(EXISTS ${SNAP_EXE})
  set(SNAPCRAFT_FOUND ON)
endif()

if(SNAPCRAFT_FOUND)
  set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${ARANGODB_PACKAGE_REVISION}_${ARANGODB_PACKAGE_ARCHITECTURE}")
  set(SNAPCRAFT_TEMPLATE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Installation/Ubuntu")
  set(CPACK_PACKAGE_TGZ "${CMAKE_BINARY_DIR}/${CPACK_PACKAGE_FILE_NAME}.tar.gz")
  set(SNAPCRAFT_SOURCE_DIR "${CMAKE_BINARY_DIR}/_CPack_Packages/SNAP")

  message(STATUS "Create snap package")

  if (VERBOSE)
    message(STATUS "CPACK_PACKAGE_FILE_NAME :" ${CPACK_PACKAGE_FILE_NAME})
    message(STATUS "SNAPCRAFT_TEMPLATE_DIR: " ${SNAPCRAFT_TEMPLATE_DIR})
    message(STATUS "CPACK_PACKAGE_TGZ: " ${CPACK_PACKAGE_TGZ})
    message(STATUS "SNAPCRAFT_SOURCE_DIR: " ${SNAPCRAFT_SOURCE_DIR})
  endif ()

#  set(SNAP_PORT 8529)
  set(CPACK_STRIP_FILES ON)
  set(CMAKE_INSTALL_PREFIX "/")

  configure_file(
    "${SNAPCRAFT_TEMPLATE_DIR}/snapcraft.yaml.in"
    "${SNAPCRAFT_SOURCE_DIR}/snapcraft.yaml"
    NEWLINE_STYLE UNIX
    @ONLY
  )

  file(
    COPY        "${SNAPCRAFT_TEMPLATE_DIR}/arangodb.png"
    DESTINATION "${SNAPCRAFT_SOURCE_DIR}/"
  )

  add_custom_target(snap_TGZ
    COMMENT "create TGZ-package"
    COMMAND ${CMAKE_CPACK_COMMAND} -G TGZ
  )

  add_custom_target(snap
    COMMENT "create snap-package"
    COMMAND ${SNAP_EXE} snap
    COMMAND cp *.snap ${PROJECT_BINARY_DIR}
    DEPENDS snap_TGZ
    WORKING_DIRECTORY ${SNAPCRAFT_SOURCE_DIR}
  )

  add_custom_target(copy_snap_packages
    COMMAND cp *.snap ${PACKAGE_TARGET_DIR})

  list(APPEND COPY_PACKAGES_LIST copy_snap_packages)

  list(APPEND PACKAGES_LIST snap)
endif()
