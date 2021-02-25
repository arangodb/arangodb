set(SED_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/sed.script.cmake"
  CACHE INTERNAL "path to sed script")

function(sed input_file output_file)
  add_custom_command(OUTPUT "${output_file}"
                     COMMAND ${CMAKE_COMMAND}
                       -Dinput_file="${input_file}"
                       -Doutput_file="${output_file}"
                       -Dreplacement_pairs="${ARGN}"
                       -P ${SED_SCRIPT}
                     DEPENDS "${input_file}")
endfunction()
