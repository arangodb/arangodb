# -*- mode: CMAKE; -*-

## -----------------------------------------------------------------------------
## --SECTION--                                                   COMMON INCLUDES
## -----------------------------------------------------------------------------

## -----------------------------------------------------------------------------
## --SECTION--                                                           SCRIPTS
## -----------------------------------------------------------------------------

################################################################################
### @brief output directory
################################################################################

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/")

################################################################################
### @brief arango-dfdb
################################################################################

FILE(READ ${PROJECT_SOURCE_DIR}/utils/${SCRIPT_ARANGO_DFDB}.in FileContent)

STRING(REPLACE "@SBINDIR@" "${TRI_SBINDIR}" 
       FileContent "${FileContent}")

STRING(REPLACE "@LOCALSTATEDIR@" "${VARDIR}" 
       FileContent "${FileContent}")

STRING(REPLACE "@PKGDATADIR@" "${TRI_PKGDATADIR}" 
       FileContent "${FileContent}")

FILE(WRITE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${SCRIPT_ARANGO_DFDB} "${FileContent}")

install(
  PROGRAMS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${SCRIPT_ARANGO_DFDB}
  DESTINATION bin)

################################################################################
### @brief foxx-manager
################################################################################

FILE(READ ${PROJECT_SOURCE_DIR}/utils/${SCRIPT_FOXX_MANAGER}.in FileContent)

STRING(REPLACE "@BINDIR@" "${TRI_BINDIR}" 
       FileContent "${FileContent}")

FILE(WRITE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${SCRIPT_FOXX_MANAGER} "${FileContent}")

install(
  PROGRAMS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${SCRIPT_FOXX_MANAGER}
  DESTINATION bin)

## -----------------------------------------------------------------------------
## --SECTION--                                                       END-OF-FILE
## -----------------------------------------------------------------------------

## Local Variables:
## mode: outline-minor
## outline-regexp: "^\\(### @brief\\|## --SECTION--\\|# -\\*- \\)"
## End:
