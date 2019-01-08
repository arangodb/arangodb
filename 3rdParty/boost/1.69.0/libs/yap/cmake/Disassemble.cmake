# Copyright Louis Dionne 2016
# Copyright Zach Laine 2016
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
#
#
# This CMake module provides a way to get the disassembly of a function within
# an executable created with `add_executable`. The module provides a `disassemble`
# function that creates a target which, when built, outputs the disassembly of
# the given function within an executable to standard output.
#
# Parameters
# ----------
# target:
#   The name of the target to create. Building this target will generate the
#   requested disassembly.
#
# EXECUTABLE executable:
#   The name of an executable to disassemble. This must be the name of a valid
#   executable that was created with `add_executable`. The disassembly target
#   thus created will be made dependent on the executable, so that it is built
#   automatically when the disassembly is requested.
#
# FUNCTION function-name:
#   The name of the function to disassemble in the executable.
#
# [ALL]:
#   If provided, the generated target is included in the 'all' target.
#
function(disassemble target)
  cmake_parse_arguments(ARGS "ALL"                 # options
                             "EXECUTABLE;FUNCTION" # 1 value args
                             ""                    # multivalued args
                             ${ARGN})

  if (NOT ARGS_EXECUTABLE)
    message(FATAL_ERROR "The `EXECUTABLE` argument must be provided.")
  endif()
  if (NOT TARGET ${ARGS_EXECUTABLE})
    message(FATAL_ERROR "The `EXECUTABLE` argument must be the name of a valid "
                        "executable created with `add_executable`.")
  endif()

  if (NOT ARGS_FUNCTION)
    message(FATAL_ERROR "The `FUNCTION` argument must be provided.")
  endif()

  if (ARGS_ALL)
    set(ARGS_ALL "ALL")
  else()
    set(ARGS_ALL "")
  endif()

  if (DISASSEMBLE_lldb)
    add_custom_target(${target} ${ARGS_ALL}
      COMMAND ${DISASSEMBLE_lldb} -f $<TARGET_FILE:${ARGS_EXECUTABLE}>
                                  -o "disassemble --name ${ARGS_FUNCTION}"
                                  -o quit
      DEPENDS ${ARGS_EXECUTABLE}
    )
  elseif(DISASSEMBLE_gdb)
    add_custom_target(${target} ${ARGS_ALL}
      COMMAND ${DISASSEMBLE_gdb} -batch -se $<TARGET_FILE:${ARGS_EXECUTABLE}>
                                 -ex "disassemble ${ARGS_FUNCTION}"
      DEPENDS ${ARGS_EXECUTABLE}
    )
  endif()
endfunction()

find_program(DISASSEMBLE_gdb gdb)
find_program(DISASSEMBLE_lldb lldb)
