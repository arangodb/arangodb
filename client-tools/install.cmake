# -*- mode: CMAKE; -*-
# these are the install targets for the client package.
# we can't use RUNTIME DESTINATION here.
#
# Binaries are installed UNSTRIPPED: stripping happens at packaging time
# (scripts/packaging/strip-install-tree.sh), which first extracts the debug
# information into the debug-symbols bundle.

if (${USE_ENTERPRISE})
install_bin_and_config(arangobackup  ${CMAKE_INSTALL_BINDIR})
endif ()
install_bin_and_config(arangodump    ${CMAKE_INSTALL_BINDIR})
install_bin_and_config(arangoimport  ${CMAKE_INSTALL_BINDIR})
install_bin_and_config(arangorestore ${CMAKE_INSTALL_BINDIR})
install_bin_and_config(arangoexport  ${CMAKE_INSTALL_BINDIR})
install_bin_and_config(arangovpack   ${CMAKE_INSTALL_BINDIR})

# arangosh keeps its client-side V8 shell in 4.0 (only the server dropped
# V8) and is built unconditionally, so it is installed unconditionally too:
# the old USE_V8 guard referenced a cache variable that no longer exists,
# which silently dropped arangosh from every install tree.
install_bin_and_config(arangosh ${CMAKE_INSTALL_BINDIR})

install_command_alias(${BIN_ARANGOSH}
  ${CMAKE_INSTALL_BINDIR}
  arangoinspect)
install_config(arangoinspect)
