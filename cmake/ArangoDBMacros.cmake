include(GNUInstallDirs)

# etc -------------------------------
set(ETCDIR "" CACHE path "System configuration directory (defaults to prefix/etc)")

# /etc -------------------------------
if (ETCDIR STREQUAL "")
  set(ETCDIR_NATIVE "${CMAKE_INSTALL_PREFIX}/etc/arangodb3")
  set(ETCDIR_INSTALL "etc/arangodb3")
else ()
  set(ETCDIR_NATIVE "${ETCDIR}/arangodb3")
  set(ETCDIR_INSTALL "${ETCDIR}/arangodb3")
endif ()

# MS stuff ---------------------------
if (MSVC)
  file(TO_NATIVE_PATH "${ETCDIR_INSTALL}" ETCDIR_INSTALL)
  STRING(REGEX REPLACE "\\\\" "\\\\\\\\" ETCDIR_ESCAPED "${ETCDIR_INSTALL}")
else ()
  file(TO_NATIVE_PATH "${ETCDIR_NATIVE}" ETCDIR_NATIVE)
  STRING(REGEX REPLACE "\\\\" "\\\\\\\\" ETCDIR_ESCAPED "${ETCDIR_NATIVE}")
endif ()

add_definitions("-D_SYSCONFDIR_=\"${ETCDIR_ESCAPED}\"")

# /var
set(VARDIR ""
  CACHE path
  "System configuration directory (defaults to prefix/var/arangodb3)"
)

if (VARDIR STREQUAL "")
  set(VARDIR_NATIVE "${CMAKE_INSTALL_PREFIX}/var")
  set(VARDIR_INSTALL "var")
else ()
  set(VARDIR_NATIVE "${VARDIR}")
  set(VARDIR_INSTALL "${VARDIR}")
endif ()

file(TO_NATIVE_PATH "${VARDIR_NATIVE}" VARDIR_NATIVE)

# database directory 
FILE(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/var/lib/arangodb3")

# apps
FILE(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/var/lib/arangodb3-apps")

# logs
FILE(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/var/log/arangodb")

# package
set(TRI_PKGDATADIR "${CMAKE_INSTALL_PREFIX}/share/arangodb3")

# resources
set(TRI_RESOURCEDIR "resources")

# sbinaries
if (MSVC)
  set(ARANGODB_INSTALL_SBIN "bin")
  set(TRI_SBINDIR "${CMAKE_INSTALL_PREFIX}/bin")
else ()
  set(ARANGODB_INSTALL_SBIN "sbin")
  set(TRI_SBINDIR "${CMAKE_INSTALL_PREFIX}/sbin")
endif ()

# MS Windows -------------------------------------------------------------------
if (MSVC)
  # icon paths 
  file(TO_NATIVE_PATH
    "${TRI_RESOURCEDIR}/Icons/arangodb.ico"
    RELATIVE_ARANGO_ICON
  )

  file(TO_NATIVE_PATH
    "${PROJECT_SOURCE_DIR}/Installation/Windows/Icons/arangodb.bmp"
    ARANGO_IMG
  )

  file(TO_NATIVE_PATH
    "${PROJECT_SOURCE_DIR}/Installation/Windows/Icons/arangodb.ico"
    ARANGO_ICON
  )

  STRING(REGEX REPLACE "\\\\" "\\\\\\\\" ARANGO_IMG "${ARANGO_IMG}")
  STRING(REGEX REPLACE "\\\\" "\\\\\\\\" ARANGO_ICON "${ARANGO_ICON}")
  STRING(REGEX REPLACE "\\\\" "\\\\\\\\" RELATIVE_ARANGO_ICON "${RELATIVE_ARANGO_ICON}") 

  # versioning
  set(CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH}
    ${PROJECT_SOURCE_DIR}/Installation/Windows/version
  )

  include("${PROJECT_SOURCE_DIR}/Installation/Windows/version/generate_product_version.cmake")
endif ()

################################################################################
## INSTALL
################################################################################

# Global macros ----------------------------------------------------------------
macro (generate_root_config name)
  FILE(READ ${PROJECT_SOURCE_DIR}/etc/arangodb3/${name}.conf.in FileContent)
  STRING(REPLACE "@PKGDATADIR@" "@ROOTDIR@/share/arangodb3"
    FileContent "${FileContent}") 
  STRING(REPLACE "@LOCALSTATEDIR@" "@ROOTDIR@/var"
    FileContent "${FileContent}")
  STRING(REPLACE "@SBINDIR@" "@ROOTDIR@/bin"
    FileContent "${FileContent}")
  STRING(REPLACE "@LIBEXECDIR@/arangodb3" "@ROOTDIR@/bin"
    FileContent "${FileContent}")
  STRING(REPLACE "@SYSCONFDIR@" "@ROOTDIR@/etc/arangodb3"
    FileContent "${FileContent}")
  if (MSVC)
    STRING(REPLACE "@PROGRAM_SUFFIX@" ".exe"
      FileContent "${FileContent}")
    STRING(REGEX REPLACE "[\r\n]file =" "\n# file =" 
      FileContent "${FileContent}")
  endif ()
  FILE(WRITE ${PROJECT_BINARY_DIR}/etc/arangodb3/${name}.conf "${FileContent}")
endmacro ()

#  generates config file using the configured paths ----------------------------
macro (generate_path_config name)
  FILE(READ ${PROJECT_SOURCE_DIR}/etc/arangodb3/${name}.conf.in FileContent)
  STRING(REPLACE "@PKGDATADIR@" "${TRI_PKGDATADIR}" 
    FileContent "${FileContent}")
  STRING(REPLACE "@LOCALSTATEDIR@" "${VARDIR_NATIVE}" 
    FileContent "${FileContent}")
  FILE(WRITE ${PROJECT_BINARY_DIR}/etc/arangodb3/${name}.conf "${FileContent}")
endmacro ()

# installs a config file -------------------------------------------------------
macro (install_config name)
  if (MSVC OR DARWIN)
    generate_root_config(${name})
  else ()
    generate_path_config(${name})
  endif ()
  install(
    FILES ${PROJECT_BINARY_DIR}/etc/arangodb3/${name}.conf
    DESTINATION ${ETCDIR_INSTALL})
endmacro ()

# installs a readme file converting EOL ----------------------------------------
macro (install_readme input where output)
  FILE(READ ${PROJECT_SOURCE_DIR}/${input} FileContent)
  STRING(REPLACE "\r" "" FileContent "${FileContent}")
  if (MSVC)
    STRING(REPLACE "\n" "\r\n" FileContent "${FileContent}")
  endif ()
  FILE(WRITE ${PROJECT_BINARY_DIR}/${output} "${FileContent}")
  install(
    FILES ${PROJECT_BINARY_DIR}/${output}
    DESTINATION ${where})
endmacro ()

# installs a link to an executable ---------------------------------------------
macro (install_command_alias name where alias)
  if (MSVC)
    add_custom_command(
      TARGET ${name}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${name}>
	      ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${alias}.exe)
    install(
      PROGRAMS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${alias}.exe
      DESTINATION ${where})
  else ()
    add_custom_command(
      TARGET ${name}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E create_symlink ${name}
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${alias}) 
    install(
      PROGRAMS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${alias}
      DESTINATION ${where})
  endif ()
endmacro ()

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

# install ----------------------------------------------------------------------
install(DIRECTORY ${PROJECT_SOURCE_DIR}/Documentation/man/
  DESTINATION share/man)

if (MSVC)
  install_readme(README . README.txt)
  install_readme(README.md . README.md)
  install_readme(README.windows . README.windows.txt)
endif ()

if (MSVC)
  install_readme(LICENSE . LICENSE.txt)
  install_readme(LICENSES-OTHER-COMPONENTS.md . LICENSES-OTHER-COMPONENTS.md)
else ()
  install_readme(README share/doc/arangodb3 README)
  install_readme(README.md share/doc/arangodb3 README.md)
  install_readme(LICENSE share/doc/arangodb3 LICENSE)
  install_readme(LICENSES-OTHER-COMPONENTS.md share/doc/arangodb3 LICENSES-OTHER-COMPONENTS.md)
endif ()

# Build package ----------------------------------------------------------------
if (NOT(MSVC))
  set(CPACK_SET_DESTDIR ON)
endif()

find_program(DH_INSTALLINIT dh_installinit)
find_program(FAKEROOT fakeroot)

if (DH_INSTALLINIT AND FAKEROOT)
  add_custom_target(prepare_debian)
  SET(DEBIAN_CONTROL_EXTRA_BASENAMES
    postinst
    preinst
    postrm
    prerm
  )
  add_custom_command(TARGET prepare_debian POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E
    remove_directory "${PROJECT_BINARY_DIR}/debian"
  )
  foreach (_DEBIAN_CONTROL_EXTRA_BASENAME ${DEBIAN_CONTROL_EXTRA_BASENAMES})
    SET(RELATIVE_NAME "debian/${_DEBIAN_CONTROL_EXTRA_BASENAME}")
    SET(SRCFILE "${PROJECT_SOURCE_DIR}/Installation/${RELATIVE_NAME}")
    SET(DESTFILE "${PROJECT_BINARY_DIR}/${RELATIVE_NAME}")

    list(APPEND DEBIAN_CONTROL_EXTRA_SRC "${SRCFILE}")
    list(APPEND DEBIAN_CONTROL_EXTRA_DEST "${DESTFILE}")
    
    add_custom_command(TARGET prepare_debian POST_BUILD
                     COMMAND ${CMAKE_COMMAND} -E
                     copy ${SRCFILE} ${DESTFILE})
  endforeach()
  
  SET(DEBIAN_WORK_DIR "${PROJECT_BINARY_DIR}/debian")
  add_custom_command(TARGET prepare_debian POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E
    copy "${PROJECT_SOURCE_DIR}/Installation/debian/control" "${PROJECT_BINARY_DIR}/debian/control"
  )
  add_custom_command(TARGET prepare_debian POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E
    copy "${PROJECT_SOURCE_DIR}/Installation/debian/compat" "${PROJECT_BINARY_DIR}/debian/compat"
  )
  add_custom_command(TARGET prepare_debian POST_BUILD
    COMMAND fakeroot "${DH_INSTALLINIT}" -o 2>/dev/null
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
  )
  add_custom_command(TARGET prepare_debian POST_BUILD
    COMMAND fakeroot dh_installdeb
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
  )
endif()

# General
set(CPACK_PACKAGE_NAME "arangodb3")
set(CPACK_PACKAGE_VENDOR  "ArangoDB GmbH")
set(CPACK_PACKAGE_CONTACT "info@arangodb.com")
set(CPACK_PACKAGE_VERSION "${ARANGODB_VERSION}")

set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")

set(CPACK_STRIP_FILES "ON")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
set(CPACK_DEBIAN_PACKAGE_SECTION "database")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "a multi-purpose NoSQL database
 A distributed free and open-source database with a flexible data model for documents,
 graphs, and key-values. Build high performance applications using a convenient
 SQL-like query language or JavaScript extensions.
 .
 Copyright: 2014-2016 by ArangoDB GmbH
 Copyright: 2012-2013 by triAGENS GmbH
 ArangoDB Software
 www.arangodb.com
")
SET(CPACK_DEBIAN_PACKAGE_CONFLICTS "arangodb")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_COMPRESSION_TYPE "xz")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://www.arangodb.com/")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${PROJECT_BINARY_DIR}/debian/${CPACK_PACKAGE_NAME}/DEBIAN/postinst;${PROJECT_BINARY_DIR}/debian/${CPACK_PACKAGE_NAME}/DEBIAN/preinst;${PROJECT_BINARY_DIR}/debian/${CPACK_PACKAGE_NAME}/DEBIAN/postrm;${PROJECT_BINARY_DIR}/debian/${CPACK_PACKAGE_NAME}/DEBIAN/prerm;")
set(CPACK_BUNDLE_NAME            "${CPACK_PACKAGE_NAME}")
configure_file("${PROJECT_SOURCE_DIR}/Installation/MacOSX/Bundle/Info.plist.in" "${CMAKE_CURRENT_BINARY_DIR}/Info.plist")
set(CPACK_BUNDLE_PLIST           "${CMAKE_CURRENT_BINARY_DIR}/Info.plist")
set(CPACK_BUNDLE_ICON            "${PROJECT_SOURCE_DIR}/Installation/MacOSX/Bundle/icon.icns")
set(CPACK_BUNDLE_STARTUP_COMMAND "${PROJECT_SOURCE_DIR}/Installation/MacOSX/Bundle/arangodb-cli.sh")

# MS installer
if (MSVC)
  set(CPACK_PACKAGE_NAME "ArangoDB")
  set(CPACK_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Installation/Windows/Templates")
  set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL 1)
  set(BITS 64)

  if (CMAKE_CL_64)
    SET(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    SET(BITS 64)
  else ()
    SET(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES")
    SET(BITS 32)
  endif ()

  message(STATUS "ARANGO_IMG:  ${ARANGO_IMG}")
  message(STATUS "ARANGO_ICON: ${ARANGO_ICON}")
  message(STATUS "RELATIVE_ARANGO_ICON: ${RELATIVE_ARANGO_ICON}")

  install(
    DIRECTORY "${PROJECT_SOURCE_DIR}/Installation/Windows/Icons"
    DESTINATION ${TRI_RESOURCEDIR})

  set(CPACK_NSIS_DEFINES "
    !define BITS ${BITS}
    !define TRI_FRIENDLY_SVC_NAME '${ARANGODB_FRIENDLY_STRING}'
    !define TRI_AARDVARK_URL 'http://127.0.0.1:8529'
    ")

  set(CPACK_PACKAGE_ICON             ${ARANGO_ICON})

  set(CPACK_NSIS_MODIFY_PATH         ON)
  set(CPACK_NSIS_MUI_ICON            ${ARANGO_ICON})
  set(CPACK_NSIS_MUI_UNIICON         ${ARANGO_ICON})
  set(CPACK_NSIS_INSTALLED_ICON_NAME ${RELATIVE_ARANGO_ICON})
  set(CPACK_NSIS_DISPLAY_NAME,       ${ARANGODB_DISPLAY_NAME})
  set(CPACK_NSIS_HELP_LINK           ${ARANGODB_HELP_LINK})
  set(CPACK_NSIS_URL_INFO_ABOUT      ${ARANGODB_URL_INFO_ABOUT})
  set(CPACK_NSIS_CONTACT             ${ARANGODB_CONTACT})
endif ()

configure_file("${CMAKE_SOURCE_DIR}/Installation/cmake/CMakeCPackOptions.cmake.in"
    "${CMAKE_BINARY_DIR}/CMakeCPackOptions.cmake" @ONLY)
set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_BINARY_DIR}/CMakeCPackOptions.cmake")

if (NOT(MSVC))
  # components
  install(
    FILES ${PROJECT_SOURCE_DIR}/Installation/debian/arangodb.init
    PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    DESTINATION ${ETCDIR}/init.d
    RENAME arangodb3
    COMPONENT debian-extras
  )
endif()

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
)

################################################################################
### @brief install log directory
################################################################################

install(
  DIRECTORY ${PROJECT_BINARY_DIR}/var/log/arangodb
  DESTINATION ${VARDIR_INSTALL}/log)

################################################################################
### @brief install database directory
################################################################################

install(
  DIRECTORY ${PROJECT_BINARY_DIR}/var/lib/arangodb3
  DESTINATION ${VARDIR_INSTALL}/lib)

################################################################################
### @brief install apps directory
################################################################################

install(
  DIRECTORY ${PROJECT_BINARY_DIR}/var/lib/arangodb3-apps
  DESTINATION ${VARDIR_INSTALL}/lib)
