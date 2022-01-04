# -*- mode: CMAKE; -*-
# these are the install targets for the client package.
# we can't use RUNTIME DESTINATION here.

set(STRIP_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_X}/cstrip")
add_custom_target(strip_install_client ALL)
if (${USE_ENTERPRISE})
strip_install_bin_and_config(arangobackup  ${STRIP_DIR} ${CMAKE_INSTALL_BINDIR} strip_install_client)
endif ()
strip_install_bin_and_config(arangobench   ${STRIP_DIR} ${CMAKE_INSTALL_BINDIR} strip_install_client)
strip_install_bin_and_config(arangodump    ${STRIP_DIR} ${CMAKE_INSTALL_BINDIR} strip_install_client)
strip_install_bin_and_config(arangoimport  ${STRIP_DIR} ${CMAKE_INSTALL_BINDIR} strip_install_client)
strip_install_bin_and_config(arangorestore ${STRIP_DIR} ${CMAKE_INSTALL_BINDIR} strip_install_client)
strip_install_bin_and_config(arangoexport  ${STRIP_DIR} ${CMAKE_INSTALL_BINDIR} strip_install_client)
strip_install_bin_and_config(arangosh      ${STRIP_DIR} ${CMAKE_INSTALL_BINDIR} strip_install_client)
strip_install_bin_and_config(arangovpack   ${STRIP_DIR} ${CMAKE_INSTALL_BINDIR} strip_install_client)

install_command_alias(${BIN_ARANGOSH}
  ${CMAKE_INSTALL_BINDIR}
  foxx-manager)
install_config(foxx-manager)

install_command_alias(${BIN_ARANGOSH}
  ${CMAKE_INSTALL_BINDIR}
  arangoinspect)
install_config(arangoinspect)
