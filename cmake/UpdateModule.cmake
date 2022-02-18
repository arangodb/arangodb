macro(UpdateModule _GIT _SUBMODULE _CWD _SENTINEL)
  execute_process(
    COMMAND ${_GIT} rev-parse @:./${_SUBMODULE}
    WORKING_DIRECTORY ${_CWD}
    OUTPUT_VARIABLE _EXPECTED_SUBMODULE_HASH)

  execute_process(
    COMMAND ${_GIT} rev-parse HEAD
    WORKING_DIRECTORY ${_CWD}/${_SUBMODULE}
    OUTPUT_VARIABLE _ACTUAL_SUBMODULE_HASH)

  if (NOT EXISTS ${_CWD}/${_SUBMODULE}/${_SENTINEL} OR
      NOT ${_EXPECTED_SUBMODULE_HASH} EQUAL ${_ACTUAL_SUBMODULE_HASH})
    execute_process(
      COMMAND ${_GIT} submodule update --init --force -- ${_SUBMODULE}
      RESULT_VARIABLE _INIT_RESULT
      WORKING_DIRECTORY ${_CWD})

    if (NOT ${_INIT_RESULT} EQUAL "0")
      message(WARNING "FAILED: ${_GIT} submodule update --init -- ${_SUBMODULE}")
    endif()
  endif()
endmacro(UpdateModule)
