if (USE_ENTERPRISE)

  add_executable(${BIN_ARANGOBACKUP}
    ${ProductVersionFiles_arangobackup}
    BackupFeature.cpp
    arangobackup.cpp
  )
  target_include_directories(${BIN_ARANGOBACKUP} PRIVATE ${PROJECT_SOURCE_DIR}/client-tools)
  target_include_directories(${BIN_ARANGOBACKUP} PRIVATE ${PROJECT_SOURCE_DIR}/arangod)

  target_link_libraries(${BIN_ARANGOBACKUP}
    arango
    ${SYSTEM_LIBRARIES}
    boost_system
    boost_boost
    arango_shell
    clienttools_utils
    fuerte
  )

  install(
    TARGETS ${BIN_ARANGOBACKUP}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

  install_config(${BIN_ARANGOBACKUP})

  add_dependencies(${BIN_ARANGOBACKUP} zlibstatic)

  if (NOT USE_PRECOMPILED_V8)
    add_dependencies(${BIN_ARANGOBACKUP} v8_build) # v8_build includes ICU build
  endif ()

  if (USE_JEMALLOC)
    add_dependencies(${BIN_ARANGOBACKUP} jemalloc_build)
  endif ()

  add_dependencies(${BIN_ARANGOBACKUP} man)
endif () # USE_ENTERPRISE

