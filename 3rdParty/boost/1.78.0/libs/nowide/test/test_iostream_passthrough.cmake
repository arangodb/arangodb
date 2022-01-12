# Copyright 2019 - 2021 Alexander Grund
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE or copy at http://boost.org/LICENSE_1_0.txt)

if(NOT TEST_BINARY)
  message(FATAL_ERROR "You need to define TEST_BINARY with the path to the binary")
endif()

set(msg "Hello Boost Nowide")

execute_process(
  COMMAND ${CMAKE_COMMAND} -E echo "${msg}" # This will be stdin of the below cmd
  COMMAND "${TEST_BINARY}" passthrough
  RESULT_VARIABLE result
  OUTPUT_VARIABLE stdout
)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Command ${TEST_BINARY} failed (${result}) with ${stdout}")
endif()
if(NOT stdout MATCHES ".*${msg}")
  message(FATAL_ERROR "Command ${TEST_BINARY} did not output '${msg}' but '${stdout}'")
endif()
message(STATUS "Test OK")
