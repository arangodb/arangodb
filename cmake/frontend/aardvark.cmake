################################################################################
## web interface build
################################################################################

find_package(Nodejs 16 REQUIRED)
find_package(Yarn REQUIRED)

set(FRONTEND_DESTINATION ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/react)

if (WIN32)
  add_custom_target(frontend ALL
    COMMENT "create frontend build"
    WORKING_DIRECTORY ${FRONTEND_DESTINATION}
    COMMAND cmd /c "yarn install && yarn build"
  )
else ()
  add_custom_target(frontend ALL
    COMMENT "create frontend build"
    WORKING_DIRECTORY ${FRONTEND_DESTINATION}
    COMMAND yarn install
    COMMAND yarn build
  )
endif ()

add_custom_target(frontend_clean
  COMMAND ${CMAKE_COMMAND} -E remove_directory ${FRONTEND_DESTINATION}/build
  COMMENT "Removing frontend artefacts"
)
