
################################################################################
## frontend build
################################################################################
add_custom_target(frontend
  COMMENT "create frontend build"
  COMMAND npm i --prefix ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP
  COMMAND npm install --prefix ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP grunt
  COMMAND ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/node_modules/grunt/bin/grunt --gruntfile ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/GruntFile.js deploy
  )

add_custom_target(frontend_clean
  COMMAND ${CMAKE_COMMAND} -E remove_directory ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/node_modules
  COMMENT "Removing frontend node modules"
  )

