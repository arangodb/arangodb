################################################################################
## web interface build
################################################################################

find_package(Nodejs 20 REQUIRED)
find_package(Yarn REQUIRED)

set(FRONTEND_DESTINATION ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/react)

option(NODE_MODULES_BUNDLE "specify pre-downloaded node modules for aardvark" "")


if (NODE_MODULES_BUNDLE AND EXISTS "${NODE_MODULES_BUNDLE}")
  add_custom_target(node_cache ALL
    COMMENT "extracting node modules"
    WORKING_DIRECTORY ${FRONTEND_DESTINATION}
    COMMAND ${CMAKE_COMMAND} -E tar xvJf "${NODE_MODULES_BUNDLE}"
    )
else()
  add_custom_target(node_cache ALL
    COMMENT "not extracting node modules"
    )
endif()


add_custom_target(frontend ALL
  DEPENDS node_cache
  COMMENT "create frontend build"
  DEPENDS arango_basic_utils
  WORKING_DIRECTORY ${FRONTEND_DESTINATION}
  COMMAND yarn install
  COMMAND yarn build
)

add_custom_target(frontend_clean
  COMMAND ${CMAKE_COMMAND} -E remove_directory ${FRONTEND_DESTINATION}/build
  COMMENT "Removing frontend artifacts"
)
