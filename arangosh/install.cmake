# -*- mode: CMAKE; -*-
# these are the install targets for the client package.
# we can't use RUNTIME DESTINATION here.

install(
  FILES ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BIN_ARANGOBENCH}
  DESTINATION ${CMAKE_INSTALL_FULL_BINDIR})
install_config(arangobench)

install(
  FILES ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BIN_ARANGODUMP}
  DESTINATION ${CMAKE_INSTALL_FULL_BINDIR})
install_config(arangodump)

install(
  FILES ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BIN_ARANGOIMP}
  DESTINATION ${CMAKE_INSTALL_FULL_BINDIR})
install_config(arangoimp)

install(
  FILES ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BIN_ARANGORESTORE}
  DESTINATION ${CMAKE_INSTALL_FULL_BINDIR})
install_config(arangorestore)

install(
  FILES ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BIN_ARANGOSH}
  DESTINATION ${CMAKE_INSTALL_FULL_BINDIR})
install_config(arangosh)

install(
  FILES ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BIN_ARANGOVPACK}
  DESTINATION ${CMAKE_INSTALL_FULL_BINDIR})

install_command_alias(${BIN_ARANGOSH}
  ${CMAKE_INSTALL_FULL_BINDIR}
  foxx-manager)
install_config(foxx-manager)

