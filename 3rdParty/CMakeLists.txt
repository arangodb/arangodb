# ------------------------------------------------------------------------------
# External Projects used by ArangoDB
# ------------------------------------------------------------------------------

include(ExternalProject)

################################################################################
## ETCD
################################################################################

if (GO_FOUND)
  if (MSVC)
    set (ETCD_BUILD_COMMAND build.bat)
  else ()
    set (ETCD_BUILD_COMMAND ./build)
  endif ()

  ExternalProject_Add(etcd_build
    SOURCE_DIR
      ${CMAKE_CURRENT_SOURCE_DIR}/etcd
    CONFIGURE_COMMAND
      ""
    BUILD_IN_SOURCE
      TRUE
    BUILD_COMMAND
      "${ETCD_BUILD_COMMAND}"
    INSTALL_COMMAND
      "cp"
        ${CMAKE_CURRENT_SOURCE_DIR}/etcd/bin/etcd
        ${CMAKE_BINARY_DIR}/bin/etcd-arango
  )
endif ()

################################################################################
## ZLIB
################################################################################

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/zlib/zlib-1.2.8)

################################################################################
## EV
################################################################################

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libev)

################################################################################
## V8 and ICU
################################################################################

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/V8)
