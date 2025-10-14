add_library(arango_shell STATIC
  ClientFeature.cpp
  ShellConsoleFeature.cpp
)

target_link_libraries(arango_shell
  arango_basic_utils
  ${SYSTEM_LIBRARIES}
  boost_system
  boost_boost
  fuerte
)

target_link_libraries(arango_shell v8_interface)

target_include_directories(arango_shell PRIVATE ${CMAKE_SOURCE_DIR}/client-tools)

add_executable(${BIN_ARANGOSH}
  ${ProductVersionFiles_arangosh}

  ShellFeature.cpp
  TelemetricsHandler.cpp
  V8ClientConnection.cpp
  V8ShellFeature.cpp
  ProcessMonitoringFeature.cpp
  arangosh.cpp
  v8-deadline.cpp
  )

if (USE_FAILURE_TESTS)
  target_sources(${BIN_ARANGOSH} PRIVATE RequestFuzzer.cpp)
endif()

target_include_directories(${BIN_ARANGOSH} PRIVATE ${PROJECT_SOURCE_DIR}/client-tools)

target_link_libraries(${BIN_ARANGOSH}
  arango_v8
  v8_interface
  arango_basic_http_client
  ${V8_LIBS}
  fuerte
  ${SYSTEM_LIBRARIES}
  boost_system
  boost_boost
  arango_shell
  arangoimport_utils
  clienttools_utils
  ${ICU64_LIBS}
  ${ICU_STATIC_LIBS}
  )

install(FILES ${ICU_DT}
  DESTINATION "${INSTALL_ICU_DT_DEST}"
  RENAME ${ICU_DT_DEST})

install(
  TARGETS ${BIN_ARANGOSH}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install_config(${BIN_ARANGOSH})

add_dependencies(${BIN_ARANGOSH} zlibstatic)

# use ICU 64 for the client tools
add_dependencies(${BIN_ARANGOSH} iculegacy_build)

if (NOT USE_PRECOMPILED_V8)
  add_dependencies(${BIN_ARANGOSH} v8_build)
endif ()

if (USE_JEMALLOC)
  add_dependencies(${BIN_ARANGOSH} jemalloc_build)
endif ()

add_dependencies(${BIN_ARANGOSH} man)

################################################################################
## foxx-manager
################################################################################
install_command_alias(${BIN_ARANGOSH}
  ${CMAKE_INSTALL_BINDIR}
  foxx-manager)

install_config(foxx-manager)

################################################################################
## arangoinspect
################################################################################
install_command_alias(${BIN_ARANGOSH}
  ${CMAKE_INSTALL_BINDIR}
  arangoinspect)

install_config(arangoinspect)


################################################################################
## check all dependencies on arangosh
################################################################################
function(get_all_transitive_deps target_to_check)
  # Initialize variables if this is the first call (from the main script)
  if(NOT DEFINED collected_deps)
    set(collected_deps "")
  endif()
  if(NOT DEFINED visited_targets)
    set(visited_targets "")
  endif()

  # Avoid infinite recursion from circular dependencies
  list(FIND visited_targets "${target_to_check}" is_visited)
  if (is_visited GREATER_EQUAL 0)
    return()
  endif()

  # Add the current target to the visited list
  list(APPEND visited_targets "${target_to_check}")

  # 1. Get the target's direct private and public dependencies
  get_target_property(direct_link_libs "${target_to_check}" LINK_LIBRARIES)
  if (direct_link_libs)
    list(APPEND collected_deps ${direct_link_libs})
  endif()

  # 2. Get the target's public interface dependencies
  get_target_property(interface_link_libs "${target_to_check}" INTERFACE_LINK_LIBRARIES)
  if (interface_link_libs)
    list(APPEND collected_deps ${interface_link_libs})
  endif()

  # Now, recurse on all unique targets found in both direct and interface lists
  set(all_deps_to_recurse ${direct_link_libs} ${interface_link_libs})
  list(REMOVE_DUPLICATES all_deps_to_recurse)

  foreach(dep IN LISTS all_deps_to_recurse)
    # Check if the dependency is a known target in the project
    if (TARGET ${dep})
      get_all_transitive_deps("${dep}")
    endif()
  endforeach()

  # Export the modified lists to the parent scope
  set(collected_deps ${collected_deps} PARENT_SCOPE)
  set(visited_targets ${visited_targets} PARENT_SCOPE)
endfunction()

# ==============================================================================
# MAIN CHECK LOGIC
# This part of the script calls the function and performs the final check.
# ==============================================================================

# Define the target to check.
set(TARGET_TO_CHECK "${BIN_ARANGOSH}")

# Initialize the variables for the first recursive call.
set(collected_deps "")
set(visited_targets "")

# Call the recursive function.
get_all_transitive_deps(${TARGET_TO_CHECK})

# Clean up and normalize the list (remove duplicates, etc.)
list(REMOVE_DUPLICATES collected_deps)

# Perform the check for the unwanted dependency.
list(FIND collected_deps "faiss" FAISS_FOUND_INDEX)

if (FAISS_FOUND_INDEX GREATER_EQUAL 0)
  message(FATAL_ERROR
    "Unwanted transitive dependency on FAISS found in target \"${TARGET_TO_CHECK}\".\n"
    "Full dependency list: ${collected_deps}"
    )
else ()
  message(STATUS
    "No unwanted transitive dependency on FAISS found."
    )
endif ()

