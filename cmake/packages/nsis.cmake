set(W_INSTALL_FILES                "${PROJECT_SOURCE_DIR}/Installation/Windows/")
set(CPACK_MONOLITHIC_INSTALL       1)
set(CPACK_NSIS_DISPLAY_NAME,       ${ARANGODB_DISPLAY_NAME})
set(CPACK_NSIS_HELP_LINK           ${ARANGODB_HELP_LINK})
set(CPACK_NSIS_URL_INFO_ABOUT      ${ARANGODB_URL_INFO_ABOUT})
set(CPACK_NSIS_CONTACT             ${ARANGODB_CONTACT})
set(CPACK_NSIS_MODIFY_PATH         ON)
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL 1)
set(CPACK_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Installation/Windows/Templates")
set(CPACK_PLUGIN_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Installation/Windows/Plugins")
set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "${CPACK_PACKAGE_NAME} ${CPACK_PACKAGE_VERSION}_${ARANGODB_PACKAGE_ARCHITECTURE}")
# want @ in @only nsis template:
set(CPACK_ROOTDIR "@ROOTDIR@")
if (CMAKE_CL_64)
  # this needs to remain a $string for the template:
  SET(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
  SET(BITS 64)
else ()
  SET(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES")
  SET(BITS 32)
endif ()


# icon paths 
set (ICON_PATH "${W_INSTALL_FILES}/Icons/")
install(DIRECTORY "${ICON_PATH}" DESTINATION "resources")

file(TO_NATIVE_PATH "resources/arangodb.ico" RELATIVE_ARANGO_ICON)
file(TO_NATIVE_PATH "${ICON_PATH}arangodb.bmp" ARANGO_IMG)
file(TO_NATIVE_PATH "${ICON_PATH}/arangodb.ico" ARANGO_ICON)
file(TO_NATIVE_PATH "${ICON_PATH}/arangodb_big.bmp" ARANGO_BIG_ICON)
file(TO_NATIVE_PATH "${ICON_PATH}/arangodb_welcomefinish.bmp" ARANGO_GRAPH_ICON)

STRING(REGEX REPLACE "/" "\\\\\\\\" W_SBIN_DIR "${CMAKE_INSTALL_SBINDIR}")
STRING(REGEX REPLACE "/" "\\\\\\\\" W_BIN_DIR "${CMAKE_INSTALL_BINDIR}")
STRING(REGEX REPLACE "\\\\" "\\\\\\\\" RELATIVE_ARANGO_ICON "${RELATIVE_ARANGO_ICON}") 
STRING(REGEX REPLACE "\\\\" "\\\\\\\\" ARANGO_IMG "${ARANGO_IMG}")
STRING(REGEX REPLACE "\\\\" "\\\\\\\\" ARANGO_ICON "${ARANGO_ICON}")
STRING(REGEX REPLACE "\\\\" "\\\\\\\\" ARANGO_BIG_ICON "${ARANGO_BIG_ICON}")
STRING(REGEX REPLACE "\\\\" "\\\\\\\\" ARANGO_GRAPH_ICON "${ARANGO_GRAPH_ICON}")

set(CPACK_PACKAGE_ICON             ${ARANGO_ICON})
set(CPACK_NSIS_MUI_ICON            ${ARANGO_ICON})
set(CPACK_NSIS_MUI_UNIICON         ${ARANGO_ICON})
set(CPACK_NSIS_INSTALLED_ICON_NAME ${RELATIVE_ARANGO_ICON})

set(CPACK_NSIS_HEADER_ICON         ${ARANGO_BIG_ICON})
set(CPACK_NSIS_PAGE_ICON           ${ARANGO_GRAPH_ICON})
set(CPACK_NSIS_INSTALLER_ICON_CODE ${ARANGO_BIG_ICON})
message(STATUS "RELATIVE_ARANGO_ICON: ${RELATIVE_ARANGO_ICON}")
message(STATUS "ARANGO_IMG:  ${ARANGO_IMG}")
message(STATUS "ARANGO_ICON: ${ARANGO_ICON}")
message(STATUS "W_SBIN_DIR: ${W_SBIN_DIR} ${CMAKE_INSTALL_SBINDIR} ")
message(STATUS "W_BIN_DIR: ${W_BIN_DIR} ${CMAKE_INSTALL_BINDIR} ")

set(CPACK_ARANGODB_NSIS_DEFINES "
    !define BITS ${BITS}
    !define TRI_FRIENDLY_SVC_NAME '${ARANGODB_FRIENDLY_STRING}'
    !define TRI_AARDVARK_URL 'http://127.0.0.1:8529/_db/_system/_admin/aardvark/index.html'
    !define SBIN_DIR '${W_SBIN_DIR}'
    !define BIN_DIR '${W_BIN_DIR}'
    ")


################################################################################
# hook to build the server package
################################################################################

add_custom_target(package-arangodb-server-nsis
  COMMAND ${CMAKE_COMMAND} .
  COMMAND ${CMAKE_CPACK_COMMAND} -G NSIS -C ${CMAKE_BUILD_TYPE}
  WORKING_DIRECTORY ${PROJECT_BINARY_DIR})

list(APPEND PACKAGES_LIST package-arangodb-server-nsis)

add_custom_target(package-arangodb-server-zip
  COMMAND ${CMAKE_COMMAND} .
  COMMAND ${CMAKE_CPACK_COMMAND} -G ZIP -C ${CMAKE_BUILD_TYPE}
  WORKING_DIRECTORY ${PROJECT_BINARY_DIR})

list(APPEND PACKAGES_LIST package-arangodb-server-zip)

################################################################################
# hook to build the client package
################################################################################
set(CPACK_CLIENT_PACKAGE_NAME "${CPACK_PACKAGE_NAME}-client")

set(ARANGODB_CLIENT_PACKAGE_FILE_NAME "${CPACK_CLIENT_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}_${ARANGODB_PACKAGE_ARCHITECTURE}")

string(LENGTH "${CLIENT_BUILD_DIR}" CLIENT_BUILD_DIR_LEN)
if (${CLIENT_BUILD_DIR_LEN} EQUAL 0)
  set(CLIENT_BUILD_DIR ${CMAKE_BINARY_DIR}/c)
endif()

configure_file(cmake/packages/client/nsis.txt ${CLIENT_BUILD_DIR}/CMakeLists.txt @ONLY)
add_custom_target(package-arangodb-client-nsis
  COMMAND ${CMAKE_COMMAND} .
  COMMENT "configuring client package environment"
  COMMAND ${CMAKE_CPACK_COMMAND} -G NSIS -C ${CMAKE_BUILD_TYPE}
  COMMENT "building client packages"
  COMMAND ${CMAKE_COMMAND} -E copy ${CLIENT_BUILD_DIR}/${ARANGODB_CLIENT_PACKAGE_FILE_NAME}.exe ${PROJECT_BINARY_DIR}
  COMMENT "uploading client packages"
  WORKING_DIRECTORY ${CLIENT_BUILD_DIR})


list(APPEND PACKAGES_LIST package-arangodb-client-nsis)

add_custom_target(copy_client_nsis_package
  COMMAND ${CMAKE_COMMAND} -E copy ${ARANGODB_CLIENT_PACKAGE_FILE_NAME}.exe ${PACKAGE_TARGET_DIR})

list(APPEND COPY_PACKAGES_LIST copy_client_nsis_package)

add_custom_target(copy_nsis_packages
  COMMAND ${CMAKE_COMMAND} -E copy ${CPACK_PACKAGE_FILE_NAME}.exe ${PACKAGE_TARGET_DIR})

list(APPEND COPY_PACKAGES_LIST copy_nsis_packages)

add_custom_target(copy_zip_packages
  COMMAND ${CMAKE_COMMAND} -E copy ${CPACK_PACKAGE_FILE_NAME}.zip ${PACKAGE_TARGET_DIR})

list(APPEND COPY_PACKAGES_LIST copy_zip_packages)

add_custom_target(remove_packages
  COMMAND ${CMAKE_COMMAND} -E remove_directory _CPack_Packages
  COMMENT Removing server packaging build directory
  COMMAND ${CMAKE_COMMAND} -E remove ${CPACK_PACKAGE_FILE_NAME}.zip
  COMMENT Removing local target zip packages
  COMMAND ${CMAKE_COMMAND} -E remove ${CPACK_PACKAGE_FILE_NAME}.exe
  COMMENT Removing local target nsis packages
  COMMAND ${CMAKE_COMMAND} -E remove ${ARANGODB_CLIENT_PACKAGE_FILE_NAME}.exe
  COMMENT Removing local target nsis client packages
  )

list(APPEND CLEAN_PACKAGES_LIST remove_packages)

