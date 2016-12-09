include(${CMAKE_SOURCE_DIR}/cmake/GNUInstallDirs.cmake)
set(ARANGODB_SOURCE_DIR ${CMAKE_SOURCE_DIR})
set(CMAKE_INSTALL_SYSCONFDIR_ARANGO "${CMAKE_INSTALL_SYSCONFDIR}/${CMAKE_PROJECT_NAME}")
set(CMAKE_INSTALL_FULL_SYSCONFDIR_ARANGO "${CMAKE_INSTALL_FULL_SYSCONFDIR}/${CMAKE_PROJECT_NAME}")

set(CMAKE_INSTALL_DATAROOTDIR_ARANGO "${CMAKE_INSTALL_DATAROOTDIR}/${CMAKE_PROJECT_NAME}")
set(CMAKE_INSTALL_FULL_DATAROOTDIR_ARANGO "${CMAKE_INSTALL_FULL_DATAROOTDIR}/${CMAKE_PROJECT_NAME}")

if (MSVC OR DARWIN)
  set(ENABLE_UID_CFG false)
else ()
  set(ENABLE_UID_CFG true)
endif ()
if (MSVC)
  # if we wouldn't do this, we would have to deploy the DLLs twice.
  set(CMAKE_INSTALL_SBINDIR ${CMAKE_INSTALL_BINDIR})
endif()

set(CMAKE_INSTALL_SYSCONFDIR_ARANGO "${CMAKE_INSTALL_SYSCONFDIR}/${CMAKE_PROJECT_NAME}")
set(CMAKE_INSTALL_FULL_SYSCONFDIR_ARANGO "${CMAKE_INSTALL_FULL_SYSCONFDIR}/${CMAKE_PROJECT_NAME}")

# database directory
set(ARANGODB_DB_DIRECTORY "${PROJECT_BINARY_DIR}/var/lib/${CMAKE_PROJECT_NAME}")
FILE(MAKE_DIRECTORY ${ARANGODB_DB_DIRECTORY})

# apps
set(ARANGODB_APPS_DIRECTORY "${PROJECT_BINARY_DIR}/var/lib/${CMAKE_PROJECT_NAME}-apps")
FILE(MAKE_DIRECTORY "${ARANGODB_APPS_DIRECTORY}")

# logs
FILE(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/var/log/${CMAKE_PROJECT_NAME}")

set(INSTALL_ICU_DT_DEST "${CMAKE_INSTALL_DATAROOTDIR}/arangodb3")

include(InstallMacros)

# install ----------------------------------------------------------------------
install(DIRECTORY ${PROJECT_SOURCE_DIR}/Documentation/man/
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/man)

install_readme(README README.txt)
install_readme(README.md README.md)
install_readme(LICENSES-OTHER-COMPONENTS.md LICENSES-OTHER-COMPONENTS.md)

if (USE_ENTERPRISE)
  install_readme(enterprise/LICENSE LICENSE.txt)
else ()
  install_readme(LICENSE LICENSE.txt)
endif ()

# Custom targets ----------------------------------------------------------------
# love
add_custom_target (love
  COMMENT "ArangoDB loves you."
  COMMAND ""
  )

include(InstallArangoDBJSClient)

################################################################################
### @brief install server-side JavaScript files
################################################################################

install(
  DIRECTORY
    ${PROJECT_SOURCE_DIR}/js/actions
    ${PROJECT_SOURCE_DIR}/js/apps
    ${PROJECT_SOURCE_DIR}/js/contrib
    ${PROJECT_SOURCE_DIR}/js/server
  DESTINATION
    ${CMAKE_INSTALL_DATAROOTDIR_ARANGO}/js
  REGEX
    "^.*/server/tests$" EXCLUDE
  REGEX
    "^.*/aardvark/APP/node_modules$" EXCLUDE
)

if (USE_ENTERPRISE)
  install(
    DIRECTORY
      ${PROJECT_SOURCE_DIR}/enterprise/js/server
    DESTINATION
      ${CMAKE_INSTALL_DATAROOTDIR_ARANGO}/js
    REGEX
      "^.*/server/tests$" EXCLUDE
    REGEX
      "^.*/aardvark/APP/node_modules$" EXCLUDE
  )
endif ()

################################################################################
### @brief install log directory
################################################################################

install(
  DIRECTORY
    ${PROJECT_BINARY_DIR}/var/log/arangodb3
  DESTINATION
    ${CMAKE_INSTALL_LOCALSTATEDIR}/log
)

################################################################################
### @brief install database directory
################################################################################

install(
  DIRECTORY
    ${ARANGODB_DB_DIRECTORY}
  DESTINATION
    ${CMAKE_INSTALL_LOCALSTATEDIR}/lib
)

################################################################################
### @brief install apps directory
################################################################################

install(
  DIRECTORY
    ${ARANGODB_APPS_DIRECTORY}
  DESTINATION
    ${CMAKE_INSTALL_LOCALSTATEDIR}/lib
)

################################################################################
### @brief detect if we're on a systemd enabled system; if install unit file.
################################################################################

set(IS_SYSTEMD_INSTALL 0)
set(SYSTEMD_UNIT_DIR "")
if (UNIX)
  find_package(PkgConfig QUIET)
  pkg_check_modules(SYSTEMD systemd)
  if (SYSTEMD_FOUND)
    # cmake to old: pkg_get_variable(SYSTEMD_UNIT_DIR systemd systemdsystemunitdir)
    execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE} systemd --variable=systemdsystemunitdir
      OUTPUT_VARIABLE SYSTEMD_UNIT_DIR
      OUTPUT_STRIP_TRAILING_WHITESPACE)
    set(IS_SYSTEMD_INSTALL 1)
    configure_file (
        ${ARANGODB_SOURCE_DIR}/Installation/systemd/arangodb3.service.in
        ${PROJECT_BINARY_DIR}${SYSTEMD_UNIT_DIR}/arangodb3.service
        NEWLINE_STYLE UNIX)
      if (${USE_ENTERPRISE})
        install(FILES ${PROJECT_BINARY_DIR}${SYSTEMD_UNIT_DIR}/arangodb3.service
          DESTINATION ${SYSTEMD_UNIT_DIR}/
          RENAME arangodb3e.service)
      else()
        install(FILES ${PROJECT_BINARY_DIR}${SYSTEMD_UNIT_DIR}/arangodb3.service
          DESTINATION ${SYSTEMD_UNIT_DIR}/)
      endif()
  endif()
endif()
################################################################################
### @brief propagate the locations into our programms:
################################################################################

set(PATH_SEP "/")

to_native_path("PATH_SEP")
to_native_path("CMAKE_INSTALL_FULL_LOCALSTATEDIR")
to_native_path("CMAKE_INSTALL_FULL_SYSCONFDIR_ARANGO")
to_native_path("PKGDATADIR")
to_native_path("CMAKE_INSTALL_DATAROOTDIR_ARANGO")
to_native_path("ICU_DT_DEST")
to_native_path("CMAKE_INSTALL_SBINDIR")
to_native_path("CMAKE_INSTALL_BINDIR")
to_native_path("INSTALL_ICU_DT_DEST")

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/lib/Basics/directories.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/lib/Basics/directories.h"
  NEWLINE_STYLE UNIX
)


if (MSVC)
  # so we don't need to ship dll's twice, make it one directory:
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/InstallMacros.cmake)
  set(CMAKE_INSTALL_FULL_SBINDIR     "${CMAKE_INSTALL_FULL_BINDIR}")

  # other platforms link the file into the binary
  install(FILES ${ICU_DT}
    DESTINATION "${INSTALL_ICU_DT_DEST}"
    RENAME ${ICU_DT_DEST})

  install_readme(README.windows README.windows.txt)

  # install the visual studio runtime:
  set(CMAKE_INSTALL_UCRT_LIBRARIES 1)
  include(InstallRequiredSystemLibraries)
  INSTALL(FILES ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION ${CMAKE_INSTALL_SBINDIR} COMPONENT Libraries)
  INSTALL(FILES ${CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT} DESTINATION ${CMAKE_INSTALL_SBINDIR} COMPONENT Libraries)

  # install openssl
  if (NOT LIB_EAY_RELEASE_DLL OR NOT SSL_EAY_RELEASE_DLL)
    message(FATAL_ERROR, "BUNDLE_OPENSSL set but couldn't locate SSL DLLs. Please set LIB_EAY_RELEASE_DLL and SSL_EAY_RELEASE_DLL")
  endif()

  install (FILES "${LIB_EAY_RELEASE_DLL}" DESTINATION "${CMAKE_INSTALL_BINDIR}/" COMPONENT Libraries)  
  install (FILES "${SSL_EAY_RELEASE_DLL}" DESTINATION "${CMAKE_INSTALL_BINDIR}/" COMPONENT Libraries)  
endif()
