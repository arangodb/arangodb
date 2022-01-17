include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Install all passed libraries including <PROJECT_NAME>Config.cmake and <PROJECT_NAME>ConfigVersion.cmake
# Assumes that your headers are in 'include/'
# Uses GNUInstallDirs to determine install locations
# Note: Use of BUILD_INTERFACE for headers is not required as it will be fixed up
# Arguments:
#   - TARGETS: List of targets to install
#   - NAMESPACE: Namespace to use (Installed libraries will be available as "Namespace::Name")
#   - CONFIG_FILE: If passed, this will be used to configure the *Config.cmake,
#                  else a reasonable default will be used which is enough if there are no dependencies
#   - VERSION_COMPATIBILITY: Compatibility with requested version. Defaults to SemVer semantics (SameMajorVersion)
function(install_targets)
  cmake_parse_arguments(PARSE_ARGV 0 ARG "" "NAMESPACE;CONFIG_FILE;VERSION_COMPATIBILITY" "TARGETS")
  if(ARG_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "Invalid argument(s): ${ARG_UNPARSED_ARGUMENTS}")
  endif()
  if(ARG_NAMESPACE)
    string(APPEND ARG_NAMESPACE "::")
  endif()
  if(NOT ARG_CONFIG_FILE)
    set(ARG_CONFIG_FILE "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake.in")
    file(WRITE ${ARG_CONFIG_FILE}
      "@PACKAGE_INIT@\n"
      "include(\"\${CMAKE_CURRENT_LIST_DIR}/@PROJECT_NAME@Targets.cmake\")\n"
      "check_required_components(\"@PROJECT_NAME@\")\n"
    )
  endif()
  if(NOT ARG_VERSION_COMPATIBILITY)
    set(ARG_VERSION_COMPATIBILITY SameMajorVersion)
  endif()

  # Fixup INTERFACE_INCLUDE_DIRECTORIES:
  # Wrap source includes into BUILD_INTERFACE
  foreach(tgt IN LISTS ARG_TARGETS)
    get_target_property(old_inc_dirs ${tgt} INTERFACE_INCLUDE_DIRECTORIES)
    set(inc_dirs "")
    foreach(dir IN LISTS old_inc_dirs)
      string(FIND "${dir}" "${PROJECT_SOURCE_DIR}" pos)
      string(FIND "${dir}" "${PROJECT_BINARY_DIR}" pos2)
      if(pos EQUAL 0 OR pos2 EQUAL 0)
        set(dir "$<BUILD_INTERFACE:${dir}>")
      endif()
      list(APPEND inc_dirs "${dir}")
    endforeach()
    set_target_properties(${tgt} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${inc_dirs}")
  endforeach()

  install(TARGETS ${ARG_TARGETS}
    EXPORT ${PROJECT_NAME}Targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )
  install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

  set(configFile ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake)
  set(versionFile ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake)
  set(configInstallDestination lib/cmake/${PROJECT_NAME})

  configure_package_config_file(${ARG_CONFIG_FILE} ${configFile} INSTALL_DESTINATION ${configInstallDestination})
  write_basic_package_version_file(${versionFile} COMPATIBILITY ${ARG_VERSION_COMPATIBILITY})

  install(FILES ${configFile} ${versionFile} DESTINATION ${configInstallDestination})

  install(
    EXPORT ${PROJECT_NAME}Targets
    NAMESPACE ${ARG_NAMESPACE}
    DESTINATION ${configInstallDestination}
  )
endfunction()
