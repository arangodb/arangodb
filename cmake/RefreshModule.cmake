macro(RefreshModule _SUBMODULE _CWD)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse @:./${_SUBMODULE}
    WORKING_DIRECTORY ${_CWD}
    OUTPUT_VARIABLE _EXPECTED_SUBMODULE_HASH)

  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
    WORKING_DIRECTORY ${_CWD}/${_SUBMODULE}
    OUTPUT_VARIABLE _ACTUAL_SUBMODULE_HASH)

  if (NOT ${_EXPECTED_SUBMODULE_HASH} EQUAL ${_ACTUAL_SUBMODULE_HASH})
    execute_process(
      COMMAND ${GIT_EXECUTABLE} submodule update --init -- ${_SUBMODULE}
      RESULT_VARIABLE _INIT_RESULT
      WORKING_DIRECTORY ${_CWD})

    if (NOT ${_INIT_RESULT} EQUAL "0")
      message(WARNING "FAILED: ${GIT_EXECUTABLE} submodule update --init -- ${_SUBMODULE}")
    endif()
  endif()
endmacro(RefreshModule)
