################################################################################
## web interface build
################################################################################

find_package(Nodejs 18 REQUIRED)
find_package(Yarn REQUIRED)

set(FRONTEND_DESTINATION ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/react)

if (WIN32)
  # On Windows cmake creates a cmd script for the custom command. However, yarn itself is
  # also a cmd script. We have to use the CALL command to run a nested batch script, because
  # otherwise the nested batch script will terminate the _parent_ script.
  set(YARN_PREFIX "CALL")
else ()
  set(YARN_PREFIX "")
endif ()

add_custom_target(frontend ALL
  COMMENT "create frontend build"
  WORKING_DIRECTORY ${FRONTEND_DESTINATION}
  COMMAND ${YARN_PREFIX} yarn install
  COMMAND ${YARN_PREFIX} yarn build
)

add_custom_target(frontend_clean
  COMMAND ${CMAKE_COMMAND} -E remove_directory ${FRONTEND_DESTINATION}/build
  COMMENT "Removing frontend artifacts"
)
