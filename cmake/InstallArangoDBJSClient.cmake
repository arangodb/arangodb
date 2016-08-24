################################################################################
### @brief install client-side JavaScript files
################################################################################

install(
  DIRECTORY ${ARANGODB_SOURCE_DIR}/js/common ${ARANGODB_SOURCE_DIR}/js/client 
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR_ARANGO}/js
  FILES_MATCHING PATTERN "*.js"
  REGEX "^.*/common/test-data$" EXCLUDE
  REGEX "^.*/common/tests$" EXCLUDE
  REGEX "^.*/client/tests$" EXCLUDE)

# For the node modules we need all files:
install(
  DIRECTORY ${ARANGODB_SOURCE_DIR}/js/node
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR_ARANGO}/js
)
