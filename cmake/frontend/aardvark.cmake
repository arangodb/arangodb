################################################################################
## web interface build
################################################################################

find_package(Nodejs 16 REQUIRED)
find_package(Yarn REQUIRED)

set(FRONTEND_DESTINATION ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/react/build)

add_custom_target(frontend ALL
  DEPENDS ${FRONTEND_DESTINATION}
)
add_custom_command(# frontend
  COMMENT "create frontend build"
  OUTPUT ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/react/build
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/react
  COMMAND yarn install
  COMMAND yarn build
)

add_custom_target(frontend_clean
  COMMAND ${CMAKE_COMMAND} -E remove_directory ${FRONTEND_DESTINATION}
  COMMENT "Removing frontend artefacts"
  )
