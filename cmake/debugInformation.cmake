################################################################################
## INSTALL debug information on *nux systems
################################################################################

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
  endif()
endmacro()
