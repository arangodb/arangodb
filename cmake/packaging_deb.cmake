set(CPACK_DEBIAN_PACKAGE_SECTION "database")
FILE(READ "${PROJECT_SOURCE_DIR}/Installation/debian/packagedesc.txt" CPACK_DEBIAN_PACKAGE_DESCRIPTION)
SET(CPACK_DEBIAN_PACKAGE_CONFLICTS "arangodb")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_COMPRESSION_TYPE "xz")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE ${ARANGODB_URL_INFO_ABOUT})
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${PROJECT_BINARY_DIR}/debian-work/debian/${CPACK_PACKAGE_NAME}/DEBIAN/postinst;${PROJECT_BINARY_DIR}/debian-work/debian/${CPACK_PACKAGE_NAME}/DEBIAN/preinst;${PROJECT_BINARY_DIR}/debian-work/debian/${CPACK_PACKAGE_NAME}/DEBIAN/postrm;${PROJECT_BINARY_DIR}/debian-work/debian/${CPACK_PACKAGE_NAME}/DEBIAN/prerm;")

if(CMAKE_TARGET_ARCHITECTURES MATCHES ".*x86_64.*")
  set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
elseif(CMAKE_TARGET_ARCHITECTURES MATCHES "aarch64")
  set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "arm64")
elseif(CMAKE_TARGET_ARCHITECTURES MATCHES "armv7")
  set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "arm7")
else()
  set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "i386")
endif()

find_program(DH_INSTALLINIT dh_installinit)

if (NOT DH_INSTALLINIT)
  message(ERROR "Failed to find debhelper. please install in order to build debian packages.")
endif()

find_program(FAKEROOT fakeroot)
if (NOT FAKEROOT)
  message(ERROR "Failed to find fakeroot. please install in order to build debian packages.")
endif()

add_custom_target(prepare_debian)
SET(DEBIAN_CONTROL_EXTRA_BASENAMES
  postinst
  preinst
  postrm
  prerm
  )
SET(DEBIAN_WORK_DIR "${PROJECT_BINARY_DIR}/debian-work")
add_custom_command(TARGET prepare_debian POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E
  remove_directory "${DEBIAN_WORK_DIR}"
  )
foreach (_DEBIAN_CONTROL_EXTRA_BASENAME ${DEBIAN_CONTROL_EXTRA_BASENAMES})
  SET(RELATIVE_NAME "debian/${_DEBIAN_CONTROL_EXTRA_BASENAME}")
  SET(SRCFILE "${PROJECT_SOURCE_DIR}/Installation/${RELATIVE_NAME}")
  SET(DESTFILE "${DEBIAN_WORK_DIR}/${RELATIVE_NAME}")

  list(APPEND DEBIAN_CONTROL_EXTRA_SRC "${SRCFILE}")
  list(APPEND DEBIAN_CONTROL_EXTRA_DEST "${DESTFILE}")
  
  add_custom_command(TARGET prepare_debian POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E
    copy ${SRCFILE} ${DESTFILE})
endforeach()

add_custom_command(TARGET prepare_debian POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E
  copy "${PROJECT_SOURCE_DIR}/Installation/debian/control" "${DEBIAN_WORK_DIR}/debian/control"
  )
add_custom_command(TARGET prepare_debian POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E
  copy "${PROJECT_SOURCE_DIR}/Installation/debian/compat" "${DEBIAN_WORK_DIR}/debian/compat"
  )
add_custom_command(TARGET prepare_debian POST_BUILD
  COMMAND fakeroot "${DH_INSTALLINIT}" -o 2>/dev/null
  WORKING_DIRECTORY ${DEBIAN_WORK_DIR}
  )
add_custom_command(TARGET prepare_debian POST_BUILD
  COMMAND fakeroot dh_installdeb
  WORKING_DIRECTORY ${DEBIAN_WORK_DIR}
  )
# components
install(
  FILES ${PROJECT_SOURCE_DIR}/Installation/debian/arangodb.init
  PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
  DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/init.d
  RENAME arangodb3
  COMPONENT debian-extras
  )


  
