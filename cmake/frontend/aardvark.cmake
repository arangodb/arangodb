################################################################################
## web interface build
################################################################################

find_package(Nodejs 18 REQUIRED)
find_package(Yarn REQUIRED)

set(FRONTEND_DESTINATION ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/react)

set(NODE_MODULES_BUNDLE /node_modules.tar.xz)


if (EXISTS "${NODE_MODULES_BUNDLE}")
  add_custom_target(frontend ALL
    COMMENT "extracting node modules"
    WORKING_DIRECTORY ${FRONTEND_DESTINATION}
    COMMAND ${CMAKE_COMMAND} -E tar xvJf "${NODE_MODULES_BUNDLE}"
    )
else()
  add_custom_target(frontend ALL
    COMMENT "install node modules"
    WORKING_DIRECTORY ${FRONTEND_DESTINATION}
    COMMAND yarn install
    )
endif()


add_custom_target(frontend ALL
  COMMENT "create frontend build"
  WORKING_DIRECTORY ${FRONTEND_DESTINATION}
  COMMAND yarn build
)

add_custom_target(frontend_clean
  COMMAND ${CMAKE_COMMAND} -E remove_directory ${FRONTEND_DESTINATION}/build
  COMMENT "Removing frontend artifacts"
)
