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
macro (generate_root_config name)
  FILE(READ ${PROJECT_SOURCE_DIR}/etc/arangodb3/${name}.conf.in FileContent)
  
  STRING(REPLACE "@PKGDATADIR@" "@ROOTDIR@/${CMAKE_INSTALL_DATAROOTDIR_ARANGO}"
    FileContent "${FileContent}")
  if (DARWIN)
    # var will be redirected to ~ for the macos bundle
    STRING(REPLACE "@LOCALSTATEDIR@/" "@HOME@${INC_CPACK_ARANGO_STATE_DIR}/"
      FileContent "${FileContent}")
  else ()
    STRING(REPLACE "@LOCALSTATEDIR@/" "@ROOTDIR@${CMAKE_INSTALL_LOCALSTATEDIR}/"
      FileContent "${FileContent}")
  endif ()
  
  STRING(REPLACE "@SBINDIR@" "@ROOTDIR@/${CMAKE_INSTALL_SBINDIR}"
    FileContent "${FileContent}")
  STRING(REPLACE "@LIBEXECDIR@/arangodb3" "@ROOTDIR@/${CMAKE_INSTALL_BINDIR}"
    FileContent "${FileContent}")
  STRING(REPLACE "@SYSCONFDIR@" "@ROOTDIR@/${CMAKE_INSTALL_SYSCONFDIR_ARANGO}" 
    FileContent "${FileContent}")
  if (ENABLE_UID_CFG)
    STRING(REPLACE "@DEFINEUID@" ""
      FileContent "${FileContent}")
  else ()
    STRING(REPLACE "@DEFINEUID@" "# "
      FileContent "${FileContent}")
  endif ()
  if (MSVC)
    STRING(REPLACE "@PROGRAM_SUFFIX@" ".exe"
      FileContent "${FileContent}")
    STRING(REGEX REPLACE "[\r\n]file =" "\n# file =" 
      FileContent "${FileContent}")
  endif ()
    
  FILE(WRITE ${PROJECT_BINARY_DIR}/${CMAKE_INSTALL_SYSCONFDIR_ARANGO}/${name}.conf "${FileContent}")
endmacro ()

#  generates config file using the configured paths ----------------------------
macro (generate_path_config name)
  FILE(READ "${PROJECT_SOURCE_DIR}/etc/arangodb3/${name}.conf.in" FileContent)
  STRING(REPLACE "@PKGDATADIR@" "${CMAKE_INSTALL_DATAROOTDIR_ARANGO}"
    FileContent "${FileContent}")
  STRING(REPLACE "@LOCALSTATEDIR@" "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}" 
    FileContent "${FileContent}")
  if (ENABLE_UID_CFG)
    STRING(REPLACE "@DEFINEUID@" ""
      FileContent "${FileContent}")
  else ()
    STRING(REPLACE "@DEFINEUID@" "# "
      FileContent "${FileContent}")
  endif ()
  FILE(WRITE ${PROJECT_BINARY_DIR}/${CMAKE_INSTALL_SYSCONFDIR_ARANGO}/${name}.conf "${FileContent}")
endmacro ()



# installs a config file -------------------------------------------------------
macro (install_config name)
  if (MSVC OR (DARWIN AND NOT HOMEBREW))
    generate_root_config(${name})
  else ()
    generate_path_config(${name})
  endif ()
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
    CODE "configure_file(${PROJECT_SOURCE_DIR}/${input} \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${where}${PKG_VERSION}/${output} NEWLINE_STYLE ${CRLFSTYLE})"
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

macro(install_debinfo
    STRIP_DIR
    USER_SUB_DEBINFO_DIR
    USER_FILE
    USER_STRIP_FILE)

  set(SUB_DEBINFO_DIR ${USER_SUB_DEBINFO_DIR})
  set(FILE ${USER_FILE})
  set(STRIP_FILE ${STRIP_DIR}/${USER_STRIP_FILE})

  execute_process(COMMAND mkdir -p ${STRIP_DIR})
  if (NOT MSVC AND CMAKE_STRIP AND FILE_EXECUTABLE)
    execute_process(COMMAND "rm" -f ${STRIP_FILE})
    
    execute_process(
      COMMAND ${FILE_EXECUTABLE} ${FILE}
      OUTPUT_VARIABLE FILE_RESULT)
    
    string(REGEX
      REPLACE ".*=([a-z0-9]*),.*" "\\1"
      FILE_CHECKSUM
      ${FILE_RESULT}
      )
    string(LENGTH ${FILE_CHECKSUM} FILE_CHECKSUM_LEN)

    if (FILE_CHECKSUM_LEN EQUAL 40)
      string(SUBSTRING ${FILE_CHECKSUM} 0 2 SUB_DIR)
      string(SUBSTRING ${FILE_CHECKSUM} 2 -1 STRIP_FILE)
      set(SUB_DEBINFO_DIR .build-id/${SUB_DIR})
      set(STRIP_FILE "${STRIP_FILE}.debug")
    else ()
      set(STRIP_FILE ${USER_STRIP_FILE})
    endif()
    execute_process(COMMAND ${CMAKE_OBJCOPY} --only-keep-debug ${FILE} ${STRIP_DIR}/${STRIP_FILE})
    set(FILE ${STRIP_DIR}/${STRIP_FILE})
    install(
      PROGRAMS ${FILE}
      DESTINATION ${CMAKE_INSTALL_DEBINFO_DIR}/${SUB_DEBINFO_DIR})
  endif()
endmacro()


# Detect whether this system has SHA checksums
macro(detect_binary_id_type sourceVar)
  set(${sourceVar} false)
  if (NOT MSVC AND CMAKE_STRIP AND FILE_EXECUTABLE)
    execute_process(
      COMMAND ${FILE_EXECUTABLE} ${FILE_EXECUTABLE}
      OUTPUT_VARIABLE FILE_RESULT)
    
    string(REGEX
      REPLACE ".*=([a-z0-9]*),.*" "\\1"
      FILE_CHECKSUM
      ${FILE_RESULT}
      )
    string(LENGTH ${FILE_CHECKSUM} FILE_CHECKSUM_LEN)
    if (FILE_CHECKSUM_LEN EQUAL 40)
      set(${sourceVar} true)
    endif()
<<<<<<< HEAD
    execute_process(COMMAND ${CMAKE_OBJCOPY} --only-keep-debug ${FILE} ${STRIP_DIR}/${STRIP_FILE})
    set(FILE ${STRIP_DIR}/${STRIP_FILE})
=======
>>>>>>> ac5207cf7d44f27960813ce5cde91f67bba48ccc
  endif()
endmacro()
