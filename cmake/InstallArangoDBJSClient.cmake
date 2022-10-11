################################################################################
### @brief install client-side JavaScript files
################################################################################

install(
  DIRECTORY
    ${ARANGODB_SOURCE_DIR}/js/common
    ${ARANGODB_SOURCE_DIR}/js/client 
  DESTINATION
    ${CMAKE_INSTALL_DATAROOTDIR_ARANGO}/${ARANGODB_JS_VERSION}
  FILES_MATCHING
    PATTERN "*.js"
  REGEX "^.*/js/client/modules/@arangodb/testsuites" EXCLUDE
  REGEX "^.*/js/client/modules/@arangodb/testutils" EXCLUDE
)

install(
  FILES
    ${ARANGODB_SOURCE_DIR}/js/JS_SHA1SUM.txt
  DESTINATION
    ${CMAKE_INSTALL_DATAROOTDIR_ARANGO}/${ARANGODB_JS_VERSION}
)

if (USE_ENTERPRISE)
  install(
    DIRECTORY
      ${ARANGODB_SOURCE_DIR}/enterprise/js/common
      ${ARANGODB_SOURCE_DIR}/enterprise/js/client 
    DESTINATION    ${CMAKE_INSTALL_DATAROOTDIR_ARANGO}/${ARANGODB_JS_VERSION}
    FILES_MATCHING PATTERN "*.js"
  )
endif ()

# For the node modules we need all files except the following:
install(
  DIRECTORY ${ARANGODB_SOURCE_DIR}/js/node
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR_ARANGO}/${ARANGODB_JS_VERSION}
  REGEX "^.*/ansi_up"                                      EXCLUDE
  REGEX "^.*/eslint"                                       EXCLUDE
  REGEX "^.*/node-netstat"                                 EXCLUDE
  REGEX "^.*/parse-prometheus-text-format"                 EXCLUDE
  REGEX "^.*/@xmldom"                                      EXCLUDE
  REGEX "^.*/.bin"                                         EXCLUDE
  REGEX "^.*/.npmignore"                                   EXCLUDE
  REGEX "^.*/.*-no-eslint"                                 EXCLUDE
  REGEX "^.*js/node/package.*.json"                        EXCLUDE
)

install(
  FILES ${ARANGODB_SOURCE_DIR}/js/node/package.json-no-eslint
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR_ARANGO}/${ARANGODB_JS_VERSION}/node
  RENAME package.json
)

install(
  FILES ${ARANGODB_SOURCE_DIR}/js/node/package-lock.json-no-eslint
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR_ARANGO}/${ARANGODB_JS_VERSION}/node
  RENAME package-lock.json
)
