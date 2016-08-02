set(CPACK_GENERATOR "RPM")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Installation/rpm/arangodb.spec.in" "${CMAKE_CURRENT_BINARY_DIR}/arangodb.spec" @ONLY IMMEDIATE)
set(CPACK_RPM_USER_BINARY_SPECFILE "${CMAKE_CURRENT_BINARY_DIR}/my_project.spec")
