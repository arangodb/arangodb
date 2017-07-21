################################################################################
## INSTALL
################################################################################

if (NOT CMAKE_INSTALL_SYSCONFDIR_ARANGO
    OR NOT CMAKE_INSTALL_FULL_SYSCONFDIR_ARANGO
    OR NOT CMAKE_INSTALL_DATAROOTDIR_ARANGO
    OR NOT CMAKE_INSTALL_FULL_DATAROOTDIR_ARANGO)
  message(FATAL_ERROR, "CMAKE_INSTALL_DATAROOTDIR or CMAKE_INSTALL_SYSCONFDIR not set!")
endif()

# Global macros ----------------------------------------------------------------
# installs a config file -------------------------------------------------------
macro (install_config name)
  if (MSVC OR (DARWIN AND NOT HOMEBREW))
    set(PKGDATADIR "@ROOTDIR@/${CMAKE_INSTALL_DATAROOTDIR_ARANGO}")
    if (DARWIN)
      # var will be redirected to ~ for the macos bundle
      set(LOCALSTATEDIR "@HOME@${INC_CPACK_ARANGO_STATE_DIR}")
    else ()
      set(LOCALSTATEDIR "@ROOTDIR@${CMAKE_INSTALL_LOCALSTATEDIR}")
    endif ()
    set(SBINDIR "@ROOTDIR@/${CMAKE_INSTALL_SBINDIR}")  
    set(SYSCONFDIR "@ROOTDIR@/${CMAKE_INSTALL_SYSCONFDIR_ARANGO}")
  else ()
    set(PKGDATADIR "${CMAKE_INSTALL_FULL_DATAROOTDIR_ARANGO}")
    set(LOCALSTATEDIR "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}")
  endif()
  
  if (ENABLE_UID_CFG)
      set(DEFINEUID "")
    else ()
    set(DEFINEUID "# ")
  endif ()
  
  if (MSVC)
    set(PROGRAM_SUFFIX ".exe")
    set(CRLFSTYLE "CRLF")
    set(COMMENT_LOGFILE "# ")
  else()
    set(CRLFSTYLE "UNIX")
    set(COMMENT_LOGFILE "")
    set(PROGRAM_SUFFIX "")
  endif ()

  configure_file(
    "${PROJECT_SOURCE_DIR}/etc/arangodb3/${name}.conf.in"
    "${PROJECT_BINARY_DIR}/${CMAKE_INSTALL_SYSCONFDIR_ARANGO}/${name}.conf"
    NEWLINE_STYLE ${CRLFSTYLE}
    @ONLY)

  install(
    FILES ${PROJECT_BINARY_DIR}/${CMAKE_INSTALL_SYSCONFDIR_ARANGO}/${name}.conf
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR_ARANGO})

  set(INSTALL_CONFIGFILES_LIST
    "${INSTALL_CONFIGFILES_LIST};${CMAKE_INSTALL_SYSCONFDIR_ARANGO}/${name}.conf"
    CACHE INTERNAL "INSTALL_CONFIGFILES_LIST")
endmacro ()

# installs a readme file converting EOL ----------------------------------------
macro (install_readme input output)
  set(where "${CMAKE_INSTALL_DOCDIR}")
  if (MSVC)
    # the windows installer contains the readme in the top level directory:
    set(where ".")
  endif ()

  set(PKG_VERSION "")
  if (${USE_VERSION_IN_LICENSEDIR})
    set(PKG_VERSION "-${ARANGODB_VERSION}")
  endif ()
  set(CRLFSTYLE "UNIX")
  if (MSVC)
    set(CRLFSTYLE "CRLF")
  endif ()

  install(
    CODE "configure_file(${PROJECT_SOURCE_DIR}/${input} \"${PROJECT_BINARY_DIR}/${output}\" NEWLINE_STYLE ${CRLFSTYLE})")
  install(
    FILES "${PROJECT_BINARY_DIR}/${output}"
    DESTINATION "${where}"
    )
endmacro ()

# installs a link to an executable ---------------------------------------------
if (INSTALL_MACROS_NO_TARGET_INSTALL)
  macro (install_command_alias name where alias)
    if (MSVC)
      add_custom_command(
        OUTPUT ${name}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${name}>
	${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/${alias}${CMAKE_EXECUTABLE_SUFFIX})
      install(
        PROGRAMS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/${alias}${CMAKE_EXECUTABLE_SUFFIX}
        DESTINATION ${where})
    else ()
      add_custom_command(
        OUTPUT ${name}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink ${name}
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${alias}) 
      install(
        PROGRAMS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${alias}
        DESTINATION ${where})
    endif ()
  endmacro ()
else ()
  macro (install_command_alias name where alias)
    if (MSVC)
      add_custom_command(
        TARGET ${name}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${name}>
	${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/${alias}${CMAKE_EXECUTABLE_SUFFIX})
      install(
        PROGRAMS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/${alias}${CMAKE_EXECUTABLE_SUFFIX}
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
endif()

macro(to_native_path sourceVarName)
  if (MSVC)
    string(REGEX REPLACE "/" "\\\\\\\\" "myVar" "${${sourceVarName}}" )
  else()
    string(REGEX REPLACE "//*" "/" "myVar" "${${sourceVarName}}" )
  endif()
  set("INC_${sourceVarName}" ${myVar})
endmacro()
