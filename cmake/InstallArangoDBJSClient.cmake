################################################################################
### @brief install client-side JavaScript files
################################################################################

install(
  DIRECTORY
    ${ARANGODB_SOURCE_DIR}/js/common
    ${ARANGODB_SOURCE_DIR}/js/client 
  DESTINATION
    ${CMAKE_INSTALL_DATAROOTDIR_ARANGO}/js
  FILES_MATCHING
    PATTERN "*.js"
  REGEX
    "^.*/common/test-data$" EXCLUDE
  REGEX
    "^.*/common/tests$" EXCLUDE
  REGEX
    "^.*/client/tests$" EXCLUDE
)

install(
  FILES
    ${ARANGODB_SOURCE_DIR}/js/JS_SHA1SUM.txt
  DESTINATION
    ${CMAKE_INSTALL_DATAROOTDIR_ARANGO}/JS_SHA1SUM.txt
)

if (USE_ENTERPRISE)
  install(
    DIRECTORY
      ${ARANGODB_SOURCE_DIR}/enterprise/js/common
      ${ARANGODB_SOURCE_DIR}/enterprise/js/client 
    DESTINATION    ${CMAKE_INSTALL_DATAROOTDIR_ARANGO}/js
    FILES_MATCHING PATTERN "*.js"
    REGEX          "^.*/common/test-data$"                 EXCLUDE
    REGEX          "^.*/common/tests$"                     EXCLUDE
    REGEX          "^.*/client/tests$"                     EXCLUDE
  )
endif ()

# For the node modules we need all files:
install(
  DIRECTORY ${ARANGODB_SOURCE_DIR}/js/node
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR_ARANGO}/js
  REGEX "^.*/eslint"                                       EXCLUDE
  REGEX "^.*/.npmignore"                                   EXCLUDE
  REGEX "^.*/.bin"                                         EXCLUDE
  REGEX "^.*/_admin/aardvark/APP/frontend/html/"           EXCLUDE
  REGEX "^.*/_admin/aardvark/APP/frontend/img/"            EXCLUDE
  REGEX "^.*/_admin/aardvark/APP/frontend/js/"             EXCLUDE
  REGEX "^.*/_admin/aardvark/APP/frontend/scss/"           EXCLUDE
  REGEX "^.*/_admin/aardvark/APP/frontend/src/"            EXCLUDE
)
