# -*- mode: CMAKE; -*-
# these are the install targets for the client package.
# we can't use RUNTIME DESTINATION here.
# include(/tmp/dump_vars.cmake)
message( "CMAKE_PROJECT_NAME ${CMAKE_PROJECT_NAME}/ CMAKE_INSTALL_SBINDIR ${CMAKE_INSTALL_SBINDIR}")

install_debinfo(
  "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_X}/strip"
  "${CMAKE_PROJECT_NAME}/${CMAKE_INSTALL_SBINDIR}"
  "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_X}"
  "${BIN_ARANGOD}"
  )
