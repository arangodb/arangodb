# so we don't need to ship dll's twice, make it one directory:
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/InstallMacros.cmake)
set(CMAKE_INSTALL_FULL_SBINDIR     "${CMAKE_INSTALL_FULL_BINDIR}")
set(W_INSTALL_FILES                "${PROJECT_SOURCE_DIR}/Installation/Windows/")
set(CPACK_PACKAGE_NAME             "ArangoDB")
set(CPACK_NSIS_DISPLAY_NAME,       ${ARANGODB_DISPLAY_NAME})
set(CPACK_NSIS_HELP_LINK           ${ARANGODB_HELP_LINK})
set(CPACK_NSIS_URL_INFO_ABOUT      ${ARANGODB_URL_INFO_ABOUT})
set(CPACK_NSIS_CONTACT             ${ARANGODB_CONTACT})
set(CPACK_NSIS_MODIFY_PATH         ON)
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL 1)
set(CPACK_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Installation/Windows/Templates")
set(CPACK_PLUGIN_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Installation/Windows/Plugins")
set(BITS 64)
if (CMAKE_CL_64)
  SET(CPACK_NSIS_INSTALL_ROOT "${PROGRAMFILES64}")
  SET(ARANGODB_PACKAGE_ARCHITECTURE "win64")
  SET(BITS 64)
else ()
  SET(CPACK_NSIS_INSTALL_ROOT "${PROGRAMFILES}")
  SET(ARANGODB_PACKAGE_ARCHITECTURE "win32")
  SET(BITS 32)
endif ()

install_readme(README.windows README.windows.txt)

# install the visual studio runtime:
set(CMAKE_INSTALL_UCRT_LIBRARIES 1)
include(InstallRequiredSystemLibraries)
INSTALL(FILES ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION bin COMPONENT Libraries)
INSTALL(FILES ${CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT} DESTINATION bin COMPONENT Libraries)

# install openssl
if (NOT LIB_EAY_RELEASE_DLL OR NOT SSL_EAY_RELEASE_DLL)
  message(FATAL_ERROR, "BUNDLE_OPENSSL set but couldn't locate SSL DLLs. Please set LIB_EAY_RELEASE_DLL and SSL_EAY_RELEASE_DLL")
endif()

install (FILES "${LIB_EAY_RELEASE_DLL}" DESTINATION "${CMAKE_INSTALL_BINDIR}/" COMPONENT Libraries)  
install (FILES "${SSL_EAY_RELEASE_DLL}" DESTINATION "${CMAKE_INSTALL_BINDIR}/" COMPONENT Libraries)  

# icon paths 
set (ICON_PATH "${W_INSTALL_FILES}/Icons/")
install(DIRECTORY "${ICON_PATH}" DESTINATION "resources")

file(TO_NATIVE_PATH "resources/Icons/arangodb.ico" RELATIVE_ARANGO_ICON)
file(TO_NATIVE_PATH "${ICON_PATH}arangodb.bmp" ARANGO_IMG)
file(TO_NATIVE_PATH "${ICON_PATH}/arangodb.ico" ARANGO_ICON)

STRING(REGEX REPLACE "\\\\" "\\\\\\\\" RELATIVE_ARANGO_ICON "${RELATIVE_ARANGO_ICON}") 
STRING(REGEX REPLACE "\\\\" "\\\\\\\\" ARANGO_IMG "${ARANGO_IMG}")
STRING(REGEX REPLACE "\\\\" "\\\\\\\\" ARANGO_ICON "${ARANGO_ICON}")

set(CPACK_PACKAGE_ICON             ${ARANGO_ICON})
set(CPACK_NSIS_MUI_ICON            ${ARANGO_ICON})
set(CPACK_NSIS_MUI_UNIICON         ${ARANGO_ICON})
set(CPACK_NSIS_INSTALLED_ICON_NAME ${RELATIVE_ARANGO_ICON})

message(STATUS "RELATIVE_ARANGO_ICON: ${RELATIVE_ARANGO_ICON}")
message(STATUS "ARANGO_IMG:  ${ARANGO_IMG}")
message(STATUS "ARANGO_ICON: ${ARANGO_ICON}")

# versioning
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${W_INSTALL_FILES}/version")

include("${W_INSTALL_FILES}/version/generate_product_version.cmake")

set(CPACK_ARANGODB_NSIS_DEFINES "
    !define BITS ${BITS}
    !define TRI_FRIENDLY_SVC_NAME '${ARANGODB_FRIENDLY_STRING}'
    !define TRI_AARDVARK_URL 'http://127.0.0.1:8529'
    ")

set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${ARANGODB_PACKAGE_REVISION}_${ARANGODB_PACKAGE_ARCHITECTURE}")

################################################################################
# hook to build the server package
################################################################################
add_custom_target(package-arongodb-server-nsis
  COMMAND ${CMAKE_COMMAND} .
  COMMAND ${CMAKE_CPACK_COMMAND} -G NSIS -C ${CMAKE_BUILD_TYPE}
  WORKING_DIRECTORY ${PROJECT_BINARY_DIR})

list(APPEND PACKAGES_LIST package-arongodb-server-nsis)

add_custom_target(package-arongodb-server-zip
  COMMAND ${CMAKE_COMMAND} .
  COMMAND ${CMAKE_CPACK_COMMAND} -G ZIP -C ${CMAKE_BUILD_TYPE}
  WORKING_DIRECTORY ${PROJECT_BINARY_DIR})

list(APPEND PACKAGES_LIST package-arongodb-server-zip)

################################################################################
# hook to build the client package
################################################################################
set(CLIENT_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/../p)
configure_file(cmake/packages/client/nsis.txt ${CLIENT_BUILD_DIR}/CMakeLists.txt @ONLY)
add_custom_target(package-arongodb-client-nsis
  COMMAND ${CMAKE_COMMAND} .
  COMMAND ${CMAKE_CPACK_COMMAND} -G NSIS -V -C ${CMAKE_BUILD_TYPE}
  COMMAND cp *.exe ${PROJECT_BINARY_DIR} 
  WORKING_DIRECTORY ${CLIENT_BUILD_DIR})


list(APPEND PACKAGES_LIST package-arongodb-client-nsis)
