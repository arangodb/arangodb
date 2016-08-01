include(GNUInstallDirs)

if (MSVC OR DARWIN)
  set(ENABLE_UID_CFG false)
else ()
  set(ENABLE_UID_CFG true)
endif ()

set(CMAKE_INSTALL_SYSCONFDIR_ARANGO "${CMAKE_INSTALL_SYSCONFDIR}/arangodb3" CACHE PATH "read-only single-machine data (etc)")
set(CMAKE_INSTALL_FULL_SYSCONFDIR_ARANGO "${CMAKE_INSTALL_FULL_SYSCONFDIR}/arangodb3" CACHE PATH "read-only single-machine data (etc)")

file(TO_NATIVE_PATH "${CMAKE_INSTALL_FULL_SYSCONFDIR_ARANGO}" ETCDIR_NATIVE)
STRING(REGEX REPLACE "\\\\" "\\\\\\\\" ETCDIR_ESCAPED "${ETCDIR_NATIVE}")
add_definitions("-D_SYSCONFDIR_=\"${ETCDIR_ESCAPED}\"")

# database directory 
FILE(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/var/lib/arangodb3")

# apps
FILE(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/var/lib/arangodb3-apps")

# logs
FILE(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/var/log/arangodb3")

include(ArangoDBinstallCfg)

# install ----------------------------------------------------------------------
install(DIRECTORY ${PROJECT_SOURCE_DIR}/Documentation/man/
  DESTINATION share/man)

install_readme(README README.txt)
install_readme(README.md README.md)
install_readme(LICENSE LICENSE.txt)
install_readme(LICENSES-OTHER-COMPONENTS.md LICENSES-OTHER-COMPONENTS.md)


# Build package ----------------------------------------------------------------

if (NOT(MSVC))
  set(CPACK_SET_DESTDIR ON)
endif()

set(CPACK_PACKAGE_VENDOR  "ArangoDB GmbH")
set(CPACK_PACKAGE_CONTACT "info@arangodb.com")
set(CPACK_PACKAGE_VERSION "${ARANGODB_VERSION}")

set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")

set(CPACK_STRIP_FILES "ON")

set(CPACK_PACKAGE_NAME "arangodb3")

set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}.${CMAKE_SYSTEM_PROCESSOR}")


if ("${PACKAGING}" STREQUAL "DEB")
  include(packaging_deb)
elseif ("${PACKAGING}" STREQUAL "RPM")
  include(packaging_rpm)
elseif ("${PACKAGING}" STREQUAL "Bundle")
  include(packaging_bundle)
elseif (MSVC)
  include(packaging_nsis)
endif ()

configure_file(
  "${CMAKE_SOURCE_DIR}/Installation/cmake/CMakeCPackOptions.cmake.in"
  "${CMAKE_BINARY_DIR}/CMakeCPackOptions.cmake" @ONLY)
set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_BINARY_DIR}/CMakeCPackOptions.cmake")

# Finally: user cpack
include(CPack)

# Custom targets ----------------------------------------------------------------
# love
add_custom_target (love
  COMMENT "ArangoDB loves you."
  COMMAND ""
  )


################################################################################
### @brief install client-side JavaScript files
################################################################################

install(
  DIRECTORY ${PROJECT_SOURCE_DIR}/js/common ${PROJECT_SOURCE_DIR}/js/client 
  DESTINATION share/arangodb3/js
  FILES_MATCHING PATTERN "*.js"
  REGEX "^.*/common/test-data$" EXCLUDE
  REGEX "^.*/common/tests$" EXCLUDE
  REGEX "^.*/client/tests$" EXCLUDE)

################################################################################
### @brief install server-side JavaScript files
################################################################################

install(
  DIRECTORY ${PROJECT_SOURCE_DIR}/js/actions ${PROJECT_SOURCE_DIR}/js/apps ${PROJECT_SOURCE_DIR}/js/contrib ${PROJECT_SOURCE_DIR}/js/node ${PROJECT_SOURCE_DIR}/js/server
  DESTINATION share/arangodb3/js
  REGEX "^.*/server/tests$" EXCLUDE
  REGEX "^.*/aardvark/APP/node_modules$" EXCLUDE
)

################################################################################
### @brief install log directory
################################################################################

install(
  DIRECTORY ${PROJECT_BINARY_DIR}/var/log/arangodb3
  DESTINATION ${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/log)

################################################################################
### @brief install database directory
################################################################################

install(
  DIRECTORY ${PROJECT_BINARY_DIR}/var/lib/arangodb3
  DESTINATION ${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/lib)

################################################################################
### @brief install apps directory
################################################################################

install(
  DIRECTORY ${PROJECT_BINARY_DIR}/var/lib/arangodb3-apps
  DESTINATION ${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/lib)


# sub directories --------------------------------------------------------------

#if(BUILD_STATIC_EXECUTABLES)
#  set(CMAKE_EXE_LINKER_FLAGS -static)
#  set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
#  set(CMAKE_EXE_LINK_DYNAMIC_C_FLAGS)       # remove -Wl,-Bdynamic
#  set(CMAKE_EXE_LINK_DYNAMIC_CXX_FLAGS)
#  set(CMAKE_SHARED_LIBRARY_C_FLAGS)         # remove -fPIC
#  set(CMAKE_SHARED_LIBRARY_CXX_FLAGS)
#  set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS)    # remove -rdynamic
#  set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS)
#  # Maybe this works as well, haven't tried yet.
#  # set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS FALSE)
#else(BUILD_STATIC_EXECUTABLES)
#  # Set RPATH to use for installed targets; append linker search path
#  set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LOFAR_LIBDIR}")
#  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
#endif(BUILD_STATIC_EXECUTABLES) 


#--------------------------------------------------------------------------------
#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
##--------------------------------------------------------------------------------
