#if(NOT DEFINED CMAKE_INSTALL_SYSCONFDIR)
#  set(CMAKE_INSTALL_SYSCONFDIR "etc/arangodb3" CACHE PATH "read-only single-machine data (etc)")
#endif()
include(GNUInstallDirs)

file(TO_NATIVE_PATH "${CMAKE_INSTALL_FULL_SYSCONFDIR}" ETCDIR_NATIVE)
STRING(REGEX REPLACE "\\\\" "\\\\\\\\" ETCDIR_ESCAPED "${ETCDIR_NATIVE}")
add_definitions("-D_SYSCONFDIR_=\"${ETCDIR_ESCAPED}\"")

# database directory 
FILE(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/var/lib/arangodb3")

# apps
FILE(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/var/lib/arangodb3-apps")

# logs
FILE(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/var/log/arangodb3")

include(ArangoDBinstallCfg)

# install ----------------------------------------------------------------------
install(DIRECTORY ${PROJECT_SOURCE_DIR}/Documentation/man/
  DESTINATION share/man)

install_readme(README README.txt)
install_readme(README.md README.md)
install_readme(LICENSE LICENSE.txt)
install_readme(LICENSES-OTHER-COMPONENTS.md LICENSES-OTHER-COMPONENTS.md)


# Build package ----------------------------------------------------------------

if (NOT(MSVC))
  set(CPACK_SET_DESTDIR ON)
endif()

set(CPACK_PACKAGE_VENDOR  "ArangoDB GmbH")
set(CPACK_PACKAGE_CONTACT "info@arangodb.com")
set(CPACK_PACKAGE_VERSION "${ARANGODB_VERSION}")

set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")

set(CPACK_STRIP_FILES "ON")

set(CPACK_PACKAGE_NAME "arangodb3")

set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}.${CMAKE_SYSTEM_PROCESSOR}")


if ("${PACKAGING}" STREQUAL "DEB")##################################################
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

  if(CMAKE_TARGET_ARCHITECTURES MATCHES ".*x86_64.*")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
  else()#TODO - arm?
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "i386")
  endif()
  set(CPACK_DEBIAN_PACKAGE_SECTION "database")
  FILE(READ "${PROJECT_SOURCE_DIR}/Installation/debian/packagedesc.txt" CPACK_DEBIAN_PACKAGE_DESCRIPTION)
  SET(CPACK_DEBIAN_PACKAGE_CONFLICTS "arangodb")
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
  set(CPACK_DEBIAN_COMPRESSION_TYPE "xz")
  set(CPACK_DEBIAN_PACKAGE_HOMEPAGE ${ARANGODB_URL_INFO_ABOUT})
  set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${PROJECT_BINARY_DIR}/debian-work/debian/${CPACK_PACKAGE_NAME}/DEBIAN/postinst;${PROJECT_BINARY_DIR}/debian-work/debian/${CPACK_PACKAGE_NAME}/DEBIAN/preinst;${PROJECT_BINARY_DIR}/debian-work/debian/${CPACK_PACKAGE_NAME}/DEBIAN/postrm;${PROJECT_BINARY_DIR}/debian-work/debian/${CPACK_PACKAGE_NAME}/DEBIAN/prerm;")
  
elseif ("${PACKAGING}" STREQUAL "RPM")##############################################
  set(CPACK_GENERATOR "RPM")
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Installation/rpm/arangodb.spec.in" "${CMAKE_CURRENT_BINARY_DIR}/arangodb.spec" @ONLY IMMEDIATE)
  set(CPACK_RPM_USER_BINARY_SPECFILE "${CMAKE_CURRENT_BINARY_DIR}/my_project.spec")
  
elseif ("${PACKAGING}" STREQUAL "DARWIN")############################################
  set(CPACK_PACKAGE_NAME "ArangoDB-CLI")
  set(CPACK_BUNDLE_NAME            "${CPACK_PACKAGE_NAME}")
  configure_file("${PROJECT_SOURCE_DIR}/Installation/MacOSX/Bundle/Info.plist.in" "${CMAKE_CURRENT_BINARY_DIR}/Info.plist")
  set(CPACK_BUNDLE_PLIST           "${CMAKE_CURRENT_BINARY_DIR}/Info.plist")
  set(CPACK_BUNDLE_ICON            "${PROJECT_SOURCE_DIR}/Installation/MacOSX/Bundle/icon.icns")
  set(CPACK_BUNDLE_STARTUP_COMMAND "${PROJECT_SOURCE_DIR}/Installation/MacOSX/Bundle/arangodb-cli.sh")
  
elseif (MSVC)#################################################################
  # so we don't need to ship dll's twice, make it one directory:
  set(CMAKE_INSTALL_FULL_SBINDIR     "${CMAKE_INSTALL_FULL_BINDIR}")
  set(W_INSTALL_FILES                "${PROJECT_SOURCE_DIR}/Installation/Windows/")
  set(CPACK_PACKAGE_NAME             "ArangoDB")
  set(CPACK_NSIS_DISPLAY_NAME,       ${ARANGODB_DISPLAY_NAME})
  set(CPACK_NSIS_HELP_LINK           ${ARANGODB_HELP_LINK})
  set(CPACK_NSIS_URL_INFO_ABOUT      ${ARANGODB_URL_INFO_ABOUT})
  set(CPACK_NSIS_CONTACT             ${ARANGODB_CONTACT})
  set(CPACK_NSIS_MODIFY_PATH         ON)
  set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL 1)
  set(CPACK_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Installation/Windows/Templates")
  set(CPACK_PLUGIN_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Installation/Windows/Plugins")
  set(BITS 64)
  if (CMAKE_CL_64)
    SET(CPACK_NSIS_INSTALL_ROOT "${PROGRAMFILES64}")
    SET(BITS 64)
  else ()
    SET(CPACK_NSIS_INSTALL_ROOT "${PROGRAMFILES}")
    SET(BITS 32)
  endif ()
  
  install_readme(README.windows README.windows.txt)
  
  # install the visual studio runtime:
  set(CMAKE_INSTALL_UCRT_LIBRARIES 1)
  include(InstallRequiredSystemLibraries)
  INSTALL(FILES ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION bin COMPONENT Libraries)
  INSTALL(FILES ${CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT} DESTINATION bin COMPONENT Libraries)

  # install openssl
  if (NOT LIB_EAY_RELEASE_DLL OR NOT SSL_EAY_RELEASE_DLL)
    message(FATAL_ERROR, "BUNDLE_OPENSSL set but couldn't locate SSL DLLs. Please set LIB_EAY_RELEASE_DLL and SSL_EAY_RELEASE_DLL")
  endif()
  
  install (FILES "${LIB_EAY_RELEASE_DLL}" DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}/" COMPONENT Libraries)  
  install (FILES "${SSL_EAY_RELEASE_DLL}" DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}/" COMPONENT Libraries)  

  # icon paths 
  set (ICON_PATH "${W_INSTALL_FILES}/Icons/")
  install(DIRECTORY "${ICON_PATH}" DESTINATION "resources")

  file(TO_NATIVE_PATH "resources/Icons/arangodb.ico" RELATIVE_ARANGO_ICON)
  file(TO_NATIVE_PATH "${ICON_PATH}arangodb.bmp" ARANGO_IMG)
  file(TO_NATIVE_PATH "${ICON_PATH}/arangodb.ico" ARANGO_ICON)

  STRING(REGEX REPLACE "\\\\" "\\\\\\\\" RELATIVE_ARANGO_ICON "${RELATIVE_ARANGO_ICON}") 
  STRING(REGEX REPLACE "\\\\" "\\\\\\\\" ARANGO_IMG "${ARANGO_IMG}")
  STRING(REGEX REPLACE "\\\\" "\\\\\\\\" ARANGO_ICON "${ARANGO_ICON}")

  set(CPACK_PACKAGE_ICON             ${ARANGO_ICON})
  set(CPACK_NSIS_MUI_ICON            ${ARANGO_ICON})
  set(CPACK_NSIS_MUI_UNIICON         ${ARANGO_ICON})
  set(CPACK_NSIS_INSTALLED_ICON_NAME ${RELATIVE_ARANGO_ICON})

  message(STATUS "RELATIVE_ARANGO_ICON: ${RELATIVE_ARANGO_ICON}")
  message(STATUS "ARANGO_IMG:  ${ARANGO_IMG}")
  message(STATUS "ARANGO_ICON: ${ARANGO_ICON}")

  # versioning
  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${W_INSTALL_FILES}/version")

  include("${W_INSTALL_FILES}/version/generate_product_version.cmake")

  set(CPACK_ARANGODB_NSIS_DEFINES "
    !define BITS ${BITS}
    !define TRI_FRIENDLY_SVC_NAME '${ARANGODB_FRIENDLY_STRING}'
    !define TRI_AARDVARK_URL 'http://127.0.0.1:8529'
    ")
endif ()

configure_file(
  "${CMAKE_SOURCE_DIR}/Installation/cmake/CMakeCPackOptions.cmake.in"
  "${CMAKE_BINARY_DIR}/CMakeCPackOptions.cmake" @ONLY)
set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_BINARY_DIR}/CMakeCPackOptions.cmake")


# Custom targets ----------------------------------------------------------------
# love
add_custom_target (love
  COMMENT "ArangoDB loves you."
  COMMAND ""
  )
 
# Finally: user cpack
include(CPack)

################################################################################
### @brief install client-side JavaScript files
################################################################################

install(
  DIRECTORY ${PROJECT_SOURCE_DIR}/js/common ${PROJECT_SOURCE_DIR}/js/client 
  DESTINATION share/arangodb3/js
  FILES_MATCHING PATTERN "*.js"
  REGEX "^.*/common/test-data$" EXCLUDE
  REGEX "^.*/common/tests$" EXCLUDE
  REGEX "^.*/client/tests$" EXCLUDE)

################################################################################
### @brief install server-side JavaScript files
################################################################################

install(
  DIRECTORY ${PROJECT_SOURCE_DIR}/js/actions ${PROJECT_SOURCE_DIR}/js/apps ${PROJECT_SOURCE_DIR}/js/contrib ${PROJECT_SOURCE_DIR}/js/node ${PROJECT_SOURCE_DIR}/js/server
  DESTINATION share/arangodb3/js
  REGEX "^.*/server/tests$" EXCLUDE
  REGEX "^.*/aardvark/APP/node_modules$" EXCLUDE
)

################################################################################
### @brief install log directory
################################################################################

install(
  DIRECTORY ${PROJECT_BINARY_DIR}/var/log/arangodb3
  DESTINATION ${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/log)

################################################################################
### @brief install database directory
################################################################################

install(
  DIRECTORY ${PROJECT_BINARY_DIR}/var/lib/arangodb3
  DESTINATION ${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/lib)

################################################################################
### @brief install apps directory
################################################################################

install(
  DIRECTORY ${PROJECT_BINARY_DIR}/var/lib/arangodb3-apps
  DESTINATION ${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/lib)




# sub directories --------------------------------------------------------------

#if(BUILD_STATIC_EXECUTABLES)
#  set(CMAKE_EXE_LINKER_FLAGS -static)
#  set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
#  set(CMAKE_EXE_LINK_DYNAMIC_C_FLAGS)       # remove -Wl,-Bdynamic
#  set(CMAKE_EXE_LINK_DYNAMIC_CXX_FLAGS)
#  set(CMAKE_SHARED_LIBRARY_C_FLAGS)         # remove -fPIC
#  set(CMAKE_SHARED_LIBRARY_CXX_FLAGS)
#  set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS)    # remove -rdynamic
#  set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS)
#  # Maybe this works as well, haven't tried yet.
#  # set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS FALSE)
#else(BUILD_STATIC_EXECUTABLES)
#  # Set RPATH to use for installed targets; append linker search path
#  set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LOFAR_LIBDIR}")
#  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
#endif(BUILD_STATIC_EXECUTABLES) 


#--------------------------------------------------------------------------------
#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach ()
#--------------------------------------------------------------------------------
