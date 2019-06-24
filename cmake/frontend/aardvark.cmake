
################################################################################
## frontend build
################################################################################
add_custom_target(frontend
  COMMENT "create frontend build"
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/react
  COMMAND npm install
  COMMAND npm run build
  )

add_custom_target(frontend_clean
  COMMAND ${CMAKE_COMMAND} -E remove_directory ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/react/node_modules
  COMMENT "Removing frontend node modules"
  )

