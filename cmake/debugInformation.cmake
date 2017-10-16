################################################################################
## INSTALL debug information on *nux systems
################################################################################

macro(install_debinfo
    STRIP_DIR
    USER_SUB_DEBINFO_DIR
    USER_OUTPUT_DIRECTORY
    USER_TARGET)

  string(LENGTH "${USER_TARGET}" TLEN)
  if (TLEN EQUAL 0)
    message(FATAL_ERROR "empty target specified for creating debug file")
  endif()

  set(SUB_DEBINFO_DIR ${USER_SUB_DEBINFO_DIR})
  set(FILE ${USER_OUTPUT_DIRECTORY}/${USER_TARGET}${CMAKE_EXECUTABLE_SUFFIX})
  set(STRIP_FILE ${STRIP_DIR}/${USER_TARGET}${CMAKE_EXECUTABLE_SUFFIX})

  if (NOT MSVC AND
      CMAKE_STRIP AND
      READELF_EXECUTABLE AND
      STRIP_FILE AND
      EXISTS ${FILE})
    execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${STRIP_DIR})
    execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${STRIP_FILE})

    execute_process(
      COMMAND ${READELF_EXECUTABLE} -n ${FILE}
      OUTPUT_VARIABLE READELF_RESULT)

    string(REGEX
      REPLACE ".*ID: *([a-f0-9]*).*" "\\1"
      FILE_CHECKSUM
      "${READELF_RESULT}"
      )
    string(LENGTH ${FILE_CHECKSUM} FILE_CHECKSUM_LEN)

    if (FILE_CHECKSUM_LEN EQUAL 40)
      string(SUBSTRING ${FILE_CHECKSUM} 0 2 SUB_DIR)
      string(SUBSTRING ${FILE_CHECKSUM} 2 -1 STRIP_FILE)
      set(SUB_DEBINFO_DIR .build-id/${SUB_DIR})
      set(STRIP_FILE "${STRIP_FILE}.debug")
    else ()
      set(STRIP_FILE ${USER_TARGET}${CMAKE_EXECUTABLE_SUFFIX})
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
  if (NOT MSVC AND CMAKE_STRIP AND READELF_EXECUTABLE)
    execute_process(
      COMMAND ${READELF_EXECUTABLE} -n ${READELF_EXECUTABLE}
      OUTPUT_VARIABLE READELF_RESULT)

    string(REGEX
      REPLACE ".*ID: *([a-f0-9]*).*" "\\1"
      FILE_CHECKSUM
      "${READELF_RESULT}"
      )
    string(LENGTH "${FILE_CHECKSUM}" FILE_CHECKSUM_LEN)
    if (FILE_CHECKSUM_LEN EQUAL 40)
      set(${sourceVar} true)
    endif()
  endif()
endmacro()

macro(strip_install_bin_and_config
    TARGET
    INTERMEDIATE_STRIP_DIR
    TARGET_DIR
    BIND_TARGET)

  string(LENGTH "${TARGET}" TLEN)
  if (TLEN EQUAL 0)
    message(FATAL_ERROR "empty target specified for creating stripped file")
  endif()
  set(FILE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_X}/${TARGET}${CMAKE_EXECUTABLE_SUFFIX})
  set(STRIP_FILE ${INTERMEDIATE_STRIP_DIR}/${TARGET}${CMAKE_EXECUTABLE_SUFFIX})
  if (NOT MSVC AND CMAKE_STRIP)
    set(TARGET_NAME "${BIND_TARGET}_${TARGET}")
    ExternalProject_Add("${TARGET_NAME}"
      DEPENDS ${BIND_TARGET}
      SOURCE_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_X}

      CONFIGURE_COMMAND ${CMAKE_COMMAND} -E make_directory ${INTERMEDIATE_STRIP_DIR}
      COMMENT "creating strip directory"

      BUILD_COMMAND ${CMAKE_STRIP} ${FILE} -o ${STRIP_FILE}
      COMMENT "stripping binary"
      INSTALL_COMMAND ""
      )
    install(PROGRAMS ${STRIP_FILE}
      DESTINATION ${TARGET_DIR})
  else ()
    install(
      PROGRAMS ${FILE}
      DESTINATION ${TARGET_DIR})
  endif()
  install_config(${TARGET})

endmacro()
