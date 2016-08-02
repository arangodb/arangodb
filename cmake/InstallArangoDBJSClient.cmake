################################################################################
### @brief install client-side JavaScript files
################################################################################

install(
  DIRECTORY ${ARANGODB_SOURCE_DIR}/js/common ${ARANGODB_SOURCE_DIR}/js/client 
  DESTINATION share/arangodb3/js
  FILES_MATCHING PATTERN "*.js"
  REGEX "^.*/common/test-data$" EXCLUDE
  REGEX "^.*/common/tests$" EXCLUDE
  REGEX "^.*/client/tests$" EXCLUDE)
