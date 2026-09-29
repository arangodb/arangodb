################################################################################
## INSTALL binaries and their configuration files on *nix systems
################################################################################

# Installs a binary from the build tree together with its config file.
# The binary is installed UNSTRIPPED: stripping is a packaging-time concern
# (scripts/packaging/strip-install-tree.sh), which first extracts the debug
# information of every binary it strips into the debug-symbols bundle —
# that only works if the install tree still carries the symbols.
macro(install_bin_and_config TARGET TARGET_DIR)
  string(LENGTH "${TARGET}" TLEN)
  if (TLEN EQUAL 0)
    message(FATAL_ERROR "empty target specified for installation")
  endif()
  install(
    PROGRAMS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_X}/${TARGET}${CMAKE_EXECUTABLE_SUFFIX}
    DESTINATION ${TARGET_DIR})
  install_config(${TARGET})
endmacro()
