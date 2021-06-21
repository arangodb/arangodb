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

# debug info directory:
if (${CMAKE_INSTALL_LIBDIR} STREQUAL "usr/lib64")
  # some systems have weird places for usr/lib: 
  set(CMAKE_INSTALL_DEBINFO_DIR "usr/lib/debug/")
else ()
  set(CMAKE_INSTALL_DEBINFO_DIR "${CMAKE_INSTALL_LIBDIR}/debug/")
endif ()

set(CMAKE_INSTALL_SYSCONFDIR_ARANGO "${CMAKE_INSTALL_SYSCONFDIR}/${CMAKE_PROJECT_NAME}")
set(CMAKE_INSTALL_FULL_SYSCONFDIR_ARANGO "${CMAKE_INSTALL_FULL_SYSCONFDIR}/${CMAKE_PROJECT_NAME}")

# database directory
set(ARANGODB_DB_DIRECTORY "${PROJECT_BINARY_DIR}/var/lib/${CMAKE_PROJECT_NAME}")
FILE(MAKE_DIRECTORY ${ARANGODB_DB_DIRECTORY})

# apps
set(ARANGODB_APPS_DIRECTORY "/var/lib/${CMAKE_PROJECT_NAME}-apps")
set(ARANGODB_FULL_APPS_DIRECTORY "${PROJECT_BINARY_DIR}${ARANGODB_APPS_DIRECTORY}")
FILE(MAKE_DIRECTORY "${ARANGODB_FULL_APPS_DIRECTORY}")

# logs
FILE(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/var/log/${CMAKE_PROJECT_NAME}")

set(INSTALL_ICU_DT_DEST "${CMAKE_INSTALL_DATAROOTDIR}/${CMAKE_PROJECT_NAME}")
set(INSTALL_TZDATA_DEST "${CMAKE_INSTALL_DATAROOTDIR}/${CMAKE_PROJECT_NAME}/tzdata")

set(CMAKE_TEST_DIRECTORY "tests")

include(InstallMacros)

# install ----------------------------------------------------------------------
if(UNIX)
install(DIRECTORY ${ARANGO_MAN_DIR}
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR})
endif()

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

# js/apps/system/_admin/aardvark/APP/manifest.json list files must be included
install(
  DIRECTORY
    ${PROJECT_SOURCE_DIR}/js/actions
    ${PROJECT_SOURCE_DIR}/js/apps
    ${PROJECT_SOURCE_DIR}/js/server
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR_ARANGO}/${ARANGODB_JS_VERSION}
  REGEX       "^.*/aardvark/APP/frontend.*$"               EXCLUDE
  REGEX       "^.*/aardvark/APP/react.*$"                  EXCLUDE
  REGEX       "^.*/js/server/assets/swagger/*.map$"        EXCLUDE
  REGEX       "^.*/.bin"                                   EXCLUDE
)

set(APP_FILES
 "frontend/img"
 "react/build"
)

set(app_files_source_dir ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP)
set(app_files_target_dir ${CMAKE_INSTALL_DATAROOTDIR_ARANGO}/${ARANGODB_JS_VERSION}/apps/system/_admin/aardvark/APP)

foreach (file ${APP_FILES})
    get_filename_component(parent ${file} DIRECTORY)
    if(IS_DIRECTORY ${app_files_source_dir}/${file})
      install(
        DIRECTORY
          ${app_files_source_dir}/${file}
        DESTINATION
          ${app_files_target_dir}/${parent}
        )
    else()
      install(
        FILES
          ${app_files_source_dir}/${file}
        DESTINATION
          ${app_files_target_dir}/${parent}
      )
    endif()
endforeach()

install(
  FILES
    ${ARANGODB_SOURCE_DIR}/js/JS_SHA1SUM.txt
  DESTINATION
    ${CMAKE_INSTALL_DATAROOTDIR_ARANGO}/${ARANGODB_JS_VERSION}
)

if (USE_ENTERPRISE)
  install(
    DIRECTORY   ${PROJECT_SOURCE_DIR}/enterprise/js/server
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR_ARANGO}/${ARANGODB_JS_VERSION}
    REGEX       "^.*/aardvark/APP/node_modules$"           EXCLUDE
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
    ${ARANGODB_FULL_APPS_DIRECTORY}
  DESTINATION
    ${CMAKE_INSTALL_LOCALSTATEDIR}/lib
)

################################################################################
### @brief detect if we're on a systemd enabled system; if install unit file.
################################################################################

set(SYSTEMD_FOUND false)
set(IS_SYSTEMD_INSTALL 0)

if (UNIX)
  if (${USE_ENTERPRISE})
    set(SERVICE_NAME "arangodb3e")
  else ()
    set(SERVICE_NAME "arangodb3")
  endif ()

  # use pkgconfig for systemd detection
  find_package(PkgConfig QUIET)
  if(NOT PKG_CONFIG_FOUND)
    message(STATUS "pkg-config not found - skipping systemd detection")
  else()
    set(SYSTEMD_UNIT_DIR "")
    message(STATUS "detecting systemd")
    pkg_check_modules(SYSTEMD systemd)

    if (SYSTEMD_FOUND)
      message(STATUS "-- systemd found")

      # get systemd_unit_dir -- e.g /lib/systemd/system/
      # cmake to old: pkg_get_variable(SYSTEMD_UNIT_DIR systemd systemdsystemunitdir)
      execute_process(
        COMMAND ${PKG_CONFIG_EXECUTABLE} systemd --variable=systemdsystemunitdir
        OUTPUT_VARIABLE SYSTEMD_UNIT_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
      )
      set(IS_SYSTEMD_INSTALL 1)
      
      # set prefix
      if (CMAKE_INSTALL_PREFIX AND NOT "${CMAKE_INSTALL_PREFIX}" STREQUAL "/")
        set(SYSTEMD_UNIT_DIR "${CMAKE_INSTALL_PREFIX}/${SYSTEMD_UNIT_DIR}/")
      endif()

      # configure and install systemd service
      configure_file (
        ${ARANGODB_SOURCE_DIR}/Installation/systemd/arangodb3.service.in
        ${PROJECT_BINARY_DIR}/arangodb3.service
        NEWLINE_STYLE UNIX
      )
      install(
        FILES ${PROJECT_BINARY_DIR}/arangodb3.service
        DESTINATION ${SYSTEMD_UNIT_DIR}/
        RENAME ${SERVICE_NAME}.service
      )
    else ()
      message(STATUS "-- systemd not found")
    endif(SYSTEMD_FOUND)
  endif(NOT PKG_CONFIG_FOUND) 
endif(UNIX)
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
to_native_path("CMAKE_TEST_DIRECTORY")
to_native_path("INSTALL_TZDATA_DEST")

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/lib/Basics/directories.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/lib/Basics/directories.h"
  NEWLINE_STYLE UNIX
)

install(FILES ${ICU_DT}
  DESTINATION "${INSTALL_ICU_DT_DEST}"
  RENAME ${ICU_DT_DEST})

install(FILES "${CMAKE_SOURCE_DIR}/lib/Basics/exitcodes.dat"
  DESTINATION "${INSTALL_ICU_DT_DEST}"
  RENAME exitcodes.dat)

install(FILES "${CMAKE_SOURCE_DIR}/Installation/arangodb-helper"
  DESTINATION "${INSTALL_ICU_DT_DEST}"
  RENAME arangodb-helper)

install(FILES "${CMAKE_SOURCE_DIR}/Installation/arangodb-helper"
  DESTINATION "${INSTALL_ICU_DT_DEST}"
  RENAME arangodb-update-db)
  
install(FILES ${TZ_DATA_FILES}
  DESTINATION "${INSTALL_TZDATA_DEST}")

if (MSVC AND NOT(SKIP_PACKAGING))
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/InstallMacros.cmake)
  # Make it the same directory so we don't ship DLLs twice (in bin/ on top of usr/bin/):
  set(CMAKE_INSTALL_FULL_SBINDIR     "${CMAKE_INSTALL_FULL_BINDIR}")

  install_readme(README.windows README.windows.txt)

  # install the visual studio runtime:
  set(CMAKE_INSTALL_UCRT_LIBRARIES 1)
  set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ${CMAKE_INSTALL_BINDIR})
  include(InstallRequiredSystemLibraries)
  INSTALL(FILES ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Libraries)
  INSTALL(FILES ${CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Libraries)
endif()

if (THIRDPARTY_SBIN)
  install(FILES ${THIRDPARTY_SBIN}
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    DESTINATION "${CMAKE_INSTALL_SBINDIR}")
endif()

if (THIRDPARTY_BIN)
  install(FILES ${THIRDPARTY_BIN}
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()
