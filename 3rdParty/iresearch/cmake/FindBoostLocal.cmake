# - Find Boost

# This module defines
#  target: boost-shared, shared boost libraries as per Boost_REQUIRED_COMPONENTS
#  target: boost-shared-scrt, shared boost libraries (static CRT) as per Boost_REQUIRED_COMPONENTS
#  target: boost-static, static boost libraries as per Boost_REQUIRED_COMPONENTS
#  target: boost-shared-scrt, static boost libraries (static CRT) as per Boost_REQUIRED_COMPONENTS

if("${BOOST_ROOT}" STREQUAL "")
  if(NOT "$ENV{BOOST_ROOT}" STREQUAL "")
    set(BOOST_ROOT "$ENV{BOOST_ROOT}")
  else()
    if(NOT MSVC AND EXISTS "/usr/include/boost/version.hpp")
      set(BOOST_ROOT "/usr")
    endif()
  endif()
endif()

if (MSVC)
  # disable automatic selection of boost library build variant
  add_definitions(-DBOOST_ALL_NO_LIB)
endif()

if (MSVC)
  set(Boost_LIB_PREFIX "lib")
endif()
set(Boost_USE_MULTITHREAD ON)

# define boost required components
set(Boost_REQUIRED_COMPONENTS locale system thread)

if (NOT APPLE AND NOT MSVC AND GCC_VERSION VERSION_LESS 4.9)
  # GCC before 4.9 does not support std::regex
  set(Boost_REQUIRED_COMPONENTS ${Boost_REQUIRED_COMPONENTS} regex)
endif()

# clear Boost cache before invocation
unset(Boost_INCLUDE_DIR CACHE)
unset(Boost_LIBRARIES CACHE)
unset(Boost_LIBRARY_DIRS CACHE)
unset(Boost_LOCALE_LIBRARY_DEBUG CACHE)
unset(Boost_LOCALE_LIBRARY_RELEASE CACHE)
unset(Boost_SYSTEM_LIBRARY_DEBUG CACHE)
unset(Boost_SYSTEM_LIBRARY_RELEASE CACHE)
unset(Boost_THREAD_LIBRARY_DEBUG CACHE)
unset(Boost_THREAD_LIBRARY_RELEASE CACHE)

# find Boost (shared + shared RT)
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost
  1.57.0
  REQUIRED
  COMPONENTS ${Boost_REQUIRED_COMPONENTS}
)

if (Boost_FOUND)
  if (NOT TARGET boost-shared)
    add_library(boost-shared IMPORTED SHARED)
    set_target_properties(boost-shared PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}"
      IMPORTED_LOCATION "${Boost_LIBRARIES}"
    )
  endif()

  set(Boost_SHARED_sharedRT_LIBRARIES ${Boost_LIBRARIES})
  set(Boost_SHARED_sharedRT_LOCALE_LIBRARY_DEBUG ${Boost_LOCALE_LIBRARY_DEBUG})
  set(Boost_SHARED_sharedRT_LOCALE_LIBRARY_RELEASE ${Boost_LOCALE_LIBRARY_RELEASE})
  set(Boost_SHARED_sharedRT_REGEX_LIBRARY_DEBUG ${Boost_REGEX_LIBRARY_DEBUG})
  set(Boost_SHARED_sharedRT_REGEX_LIBRARY_RELEASE ${Boost_REGEX_LIBRARY_RELEASE})
  set(Boost_SHARED_sharedRT_SYSTEM_LIBRARY_DEBUG ${Boost_SYSTEM_LIBRARY_DEBUG})
  set(Boost_SHARED_sharedRT_SYSTEM_LIBRARY_RELEASE ${Boost_SYSTEM_LIBRARY_RELEASE})
  set(Boost_SHARED_sharedRT_THREAD_LIBRARY_DEBUG ${Boost_THREAD_LIBRARY_DEBUG})
  set(Boost_SHARED_sharedRT_THREAD_LIBRARY_RELEASE ${Boost_THREAD_LIBRARY_RELEASE})

  # build a list of shared libraries (sharedRT)
  foreach(ELEMENT ${Boost_SHARED_sharedRT_LIBRARIES})
    get_filename_component(ELEMENT_FILENAME ${ELEMENT} NAME)
    string(REGEX MATCH "^(.*)\\.(lib|so)$" ELEMENT_MATCHES ${ELEMENT_FILENAME})

    if(NOT ELEMENT_MATCHES)
      continue()
    endif()

    get_filename_component(ELEMENT_DIRECTORY ${ELEMENT} DIRECTORY)
    file(GLOB ELEMENT_LIB
      "${ELEMENT_DIRECTORY}/${CMAKE_MATCH_1}.so"
      "${ELEMENT_DIRECTORY}/lib${CMAKE_MATCH_1}.so"
      "${ELEMENT_DIRECTORY}/${CMAKE_MATCH_1}.so.*"
      "${ELEMENT_DIRECTORY}/lib${CMAKE_MATCH_1}.so.*"
      "${ELEMENT_DIRECTORY}/${CMAKE_MATCH_1}.dll"
      "${ELEMENT_DIRECTORY}/lib${CMAKE_MATCH_1}.dll"
      "${ELEMENT_DIRECTORY}/${CMAKE_MATCH_1}.pdb"
      "${ELEMENT_DIRECTORY}/lib${CMAKE_MATCH_1}.pdb"
    )

    if(ELEMENT_LIB)
      list(APPEND Boost_SHARED_sharedRT_LIB_RESOURCES ${ELEMENT_LIB})
    endif()
  endforeach()

  message("Boost_SHARED_sharedRT_VERSION: " ${Boost_VERSION})
  message("Boost_SHARED_sharedRT_LIBRARIES: " ${Boost_SHARED_sharedRT_LIBRARIES})
  message("Boost_SHARED_sharedRT_LOCALE_LIBRARY_DEBUG: " ${Boost_SHARED_sharedRT_LOCALE_LIBRARY_DEBUG})
  message("Boost_SHARED_sharedRT_LOCALE_LIBRARY_RELEASE: " ${Boost_SHARED_sharedRT_LOCALE_LIBRARY_RELEASE})
  message("Boost_SHARED_sharedRT_REGEX_LIBRARY_DEBUG: " ${Boost_SHARED_sharedRT_REGEX_LIBRARY_DEBUG})
  message("Boost_SHARED_sharedRT_REGEX_LIBRARY_RELEASE: " ${Boost_SHARED_sharedRT_REGEX_LIBRARY_RELEASE})
  message("Boost_SHARED_sharedRT_SYSTEM_LIBRARY_DEBUG: " ${Boost_SHARED_sharedRT_SYSTEM_LIBRARY_DEBUG})
  message("Boost_SHARED_sharedRT_SYSTEM_LIBRARY_RELEASE: " ${Boost_SHARED_sharedRT_SYSTEM_LIBRARY_RELEASE})
  message("Boost_SHARED_sharedRT_THREAD_LIBRARY_DEBUG: " ${Boost_SHARED_sharedRT_THREAD_LIBRARY_DEBUG})
  message("Boost_SHARED_sharedRT_THREAD_LIBRARY_RELEASE: " ${Boost_SHARED_sharedRT_THREAD_LIBRARY_RELEASE})
  message("Boost_SHARED_sharedRT_LIB_RESOURCES: " ${Boost_SHARED_sharedRT_LIB_RESOURCES})
endif()


# clear Boost cache before invocation
unset(Boost_INCLUDE_DIR CACHE)
unset(Boost_LIBRARIES CACHE)
unset(Boost_LIBRARY_DIRS CACHE)
unset(Boost_LOCALE_LIBRARY_DEBUG CACHE)
unset(Boost_LOCALE_LIBRARY_RELEASE CACHE)
unset(Boost_REGEX_LIBRARY_DEBUG CACHE)
unset(Boost_REGEX_LIBRARY_RELEASE CACHE)
unset(Boost_SYSTEM_LIBRARY_DEBUG CACHE)
unset(Boost_SYSTEM_LIBRARY_RELEASE CACHE)
unset(Boost_THREAD_LIBRARY_DEBUG CACHE)
unset(Boost_THREAD_LIBRARY_RELEASE CACHE)

# find Boost (shared + static RT)
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_STATIC_RUNTIME ON)
find_package(Boost
  1.57.0
  REQUIRED
  COMPONENTS ${Boost_REQUIRED_COMPONENTS}
)

if (Boost_FOUND)
  if (NOT TARGET boost-shared-scrt)
    add_library(boost-shared-scrt IMPORTED SHARED)
    set_target_properties(boost-shared-scrt PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}"
      IMPORTED_LOCATION "${Boost_LIBRARIES}"
    )
  endif()

  set(Boost_SHARED_staticRT_LIBRARIES ${Boost_LIBRARIES})
  set(Boost_SHARED_staticRT_LOCALE_LIBRARY_DEBUG ${Boost_LOCALE_LIBRARY_DEBUG})
  set(Boost_SHARED_staticRT_LOCALE_LIBRARY_RELEASE ${Boost_LOCALE_LIBRARY_RELEASE})
  set(Boost_SHARED_staticRT_REGEX_LIBRARY_DEBUG ${Boost_REGEX_LIBRARY_DEBUG})
  set(Boost_SHARED_staticRT_REGEX_LIBRARY_RELEASE ${Boost_REGEX_LIBRARY_RELEASE})
  set(Boost_SHARED_staticRT_SYSTEM_LIBRARY_DEBUG ${Boost_SYSTEM_LIBRARY_DEBUG})
  set(Boost_SHARED_staticRT_SYSTEM_LIBRARY_RELEASE ${Boost_SYSTEM_LIBRARY_RELEASE})
  set(Boost_SHARED_staticRT_THREAD_LIBRARY_DEBUG ${Boost_THREAD_LIBRARY_DEBUG})
  set(Boost_SHARED_staticRT_THREAD_LIBRARY_RELEASE ${Boost_THREAD_LIBRARY_RELEASE})

  # build a list of shared libraries (staticRT)
  foreach(ELEMENT ${Boost_SHARED_staticRT_LIBRARIES})
    get_filename_component(ELEMENT_FILENAME ${ELEMENT} NAME)
    string(REGEX MATCH "^(.*)\\.(lib|so)$" ELEMENT_MATCHES ${ELEMENT_FILENAME})

    if(NOT ELEMENT_MATCHES)
      continue()
    endif()

    get_filename_component(ELEMENT_DIRECTORY ${ELEMENT} DIRECTORY)
    file(GLOB ELEMENT_LIB
      "${SEARCH_LIB_PATH}/${CMAKE_MATCH_1}.so"
      "${SEARCH_LIB_PATH}/lib${CMAKE_MATCH_1}.so"
      "${SEARCH_LIB_PATH}/${CMAKE_MATCH_1}.so.*"
      "${SEARCH_LIB_PATH}/lib${CMAKE_MATCH_1}.so.*"
      "${SEARCH_LIB_PATH}/${CMAKE_MATCH_1}.dll"
      "${SEARCH_LIB_PATH}/lib${CMAKE_MATCH_1}.dll"
      "${SEARCH_LIB_PATH}/${CMAKE_MATCH_1}.pdb"
      "${SEARCH_LIB_PATH}/lib${CMAKE_MATCH_1}.pdb"
    )

    if(ELEMENT_LIB)
      list(APPEND Boost_SHARED_staticRT_LIB_RESOURCES ${ELEMENT_LIB})
    endif()
  endforeach()

  message("Boost_SHARED_staticRT_VERSION: " ${Boost_VERSION})
  message("Boost_SHARED_staticRT_LIBRARIES: " ${Boost_SHARED_staticRT_LIBRARIES})
  message("Boost_SHARED_staticRT_LOCALE_LIBRARY_DEBUG: " ${Boost_SHARED_staticRT_LOCALE_LIBRARY_DEBUG})
  message("Boost_SHARED_staticRT_LOCALE_LIBRARY_RELEASE: " ${Boost_SHARED_staticRT_LOCALE_LIBRARY_RELEASE})
  message("Boost_SHARED_staticRT_REGEX_LIBRARY_DEBUG: " ${Boost_SHARED_staticRT_REGEX_LIBRARY_DEBUG})
  message("Boost_SHARED_staticRT_REGEX_LIBRARY_RELEASE: " ${Boost_SHARED_staticRT_REGEX_LIBRARY_RELEASE})
  message("Boost_SHARED_staticRT_SYSTEM_LIBRARY_DEBUG: " ${Boost_SHARED_staticRT_SYSTEM_LIBRARY_DEBUG})
  message("Boost_SHARED_staticRT_SYSTEM_LIBRARY_RELEASE: " ${Boost_SHARED_staticRT_SYSTEM_LIBRARY_RELEASE})
  message("Boost_SHARED_staticRT_THREAD_LIBRARY_DEBUG: " ${Boost_SHARED_staticRT_THREAD_LIBRARY_DEBUG})
  message("Boost_SHARED_staticRT_THREAD_LIBRARY_RELEASE: " ${Boost_SHARED_staticRT_THREAD_LIBRARY_RELEASE})
  message("Boost_SHARED_staticRT_LIB_RESOURCES: " ${Boost_SHARED_staticRT_LIB_RESOURCES})
endif()


# clear Boost cache before invocation
unset(Boost_INCLUDE_DIR CACHE)
unset(Boost_LIBRARIES CACHE)
unset(Boost_LIBRARY_DIRS CACHE)
unset(Boost_LOCALE_LIBRARY_DEBUG CACHE)
unset(Boost_LOCALE_LIBRARY_RELEASE CACHE)
unset(Boost_REGEX_LIBRARY_DEBUG CACHE)
unset(Boost_REGEX_LIBRARY_RELEASE CACHE)
unset(Boost_SYSTEM_LIBRARY_DEBUG CACHE)
unset(Boost_SYSTEM_LIBRARY_RELEASE CACHE)
unset(Boost_THREAD_LIBRARY_DEBUG CACHE)
unset(Boost_THREAD_LIBRARY_RELEASE CACHE)

# find Boost (static + shared RT)
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost
  1.57.0
  REQUIRED
  COMPONENTS ${Boost_REQUIRED_COMPONENTS}
)

if (Boost_FOUND)
  if (NOT TARGET boost-static)
    add_library(boost-static IMPORTED SHARED)
    set_target_properties(boost-static PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}"
      IMPORTED_LOCATION "${Boost_LIBRARIES}"
    )
  endif()

  set(Boost_STATIC_sharedRT_LIBRARIES ${Boost_LIBRARIES})
  set(Boost_STATIC_sharedRT_LOCALE_LIBRARY_DEBUG ${Boost_LOCALE_LIBRARY_DEBUG})
  set(Boost_STATIC_sharedRT_LOCALE_LIBRARY_RELEASE ${Boost_LOCALE_LIBRARY_RELEASE})
  set(Boost_STATIC_sharedRT_REGEX_LIBRARY_DEBUG ${Boost_REGEX_LIBRARY_DEBUG})
  set(Boost_STATIC_sharedRT_REGEX_LIBRARY_RELEASE ${Boost_REGEX_LIBRARY_RELEASE})
  set(Boost_STATIC_sharedRT_SYSTEM_LIBRARY_DEBUG ${Boost_SYSTEM_LIBRARY_DEBUG})
  set(Boost_STATIC_sharedRT_SYSTEM_LIBRARY_RELEASE ${Boost_SYSTEM_LIBRARY_RELEASE})
  set(Boost_STATIC_sharedRT_THREAD_LIBRARY_DEBUG ${Boost_THREAD_LIBRARY_DEBUG})
  set(Boost_STATIC_sharedRT_THREAD_LIBRARY_RELEASE ${Boost_THREAD_LIBRARY_RELEASE})

  message("Boost_STATIC_sharedRT_VERSION: " ${Boost_VERSION})
  message("Boost_STATIC_sharedRT_LIBRARIES: " ${Boost_STATIC_sharedRT_LIBRARIES})
  message("Boost_STATIC_sharedRT_LOCALE_LIBRARY_DEBUG: " ${Boost_STATIC_sharedRT_LOCALE_LIBRARY_DEBUG})
  message("Boost_STATIC_sharedRT_LOCALE_LIBRARY_RELEASE: " ${Boost_STATIC_sharedRT_LOCALE_LIBRARY_RELEASE})
  message("Boost_STATIC_sharedRT_REGEX_LIBRARY_DEBUG: " ${Boost_STATIC_sharedRT_REGEX_LIBRARY_DEBUG})
  message("Boost_STATIC_sharedRT_REGEX_LIBRARY_RELEASE: " ${Boost_STATIC_sharedRT_REGEX_LIBRARY_RELEASE})
  message("Boost_STATIC_sharedRT_SYSTEM_LIBRARY_DEBUG: " ${Boost_STATIC_sharedRT_SYSTEM_LIBRARY_DEBUG})
  message("Boost_STATIC_sharedRT_SYSTEM_LIBRARY_RELEASE: " ${Boost_STATIC_sharedRT_SYSTEM_LIBRARY_RELEASE})
  message("Boost_STATIC_sharedRT_THREAD_LIBRARY_DEBUG: " ${Boost_STATIC_sharedRT_THREAD_LIBRARY_DEBUG})
  message("Boost_STATIC_sharedRT_THREAD_LIBRARY_RELEASE: " ${Boost_STATIC_sharedRT_THREAD_LIBRARY_RELEASE})
endif()

# clear Boost cache before invocation
unset(Boost_INCLUDE_DIR CACHE)
unset(Boost_LIBRARIES CACHE)
unset(Boost_LIBRARY_DIRS CACHE)
unset(Boost_LOCALE_LIBRARY_DEBUG CACHE)
unset(Boost_LOCALE_LIBRARY_RELEASE CACHE)
unset(Boost_REGEX_LIBRARY_DEBUG CACHE)
unset(Boost_REGEX_LIBRARY_RELEASE CACHE)
unset(Boost_SYSTEM_LIBRARY_DEBUG CACHE)
unset(Boost_SYSTEM_LIBRARY_RELEASE CACHE)
unset(Boost_THREAD_LIBRARY_DEBUG CACHE)
unset(Boost_THREAD_LIBRARY_RELEASE CACHE)


# find Boost (static + static RT)
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)
find_package(Boost
  1.57.0
  REQUIRED
  COMPONENTS ${Boost_REQUIRED_COMPONENTS}
)

if (Boost_FOUND)
  if (NOT TARGET boost-static-scrt)
    add_library(boost-static-scrt IMPORTED SHARED)
    set_target_properties(boost-static-scrt PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}"
      IMPORTED_LOCATION "${Boost_LIBRARIES}"
    )
  endif()

  set(Boost_STATIC_staticRT_LIBRARIES ${Boost_LIBRARIES})
  set(Boost_STATIC_staticRT_LOCALE_LIBRARY_DEBUG ${Boost_LOCALE_LIBRARY_DEBUG})
  set(Boost_STATIC_staticRT_LOCALE_LIBRARY_RELEASE ${Boost_LOCALE_LIBRARY_RELEASE})
  set(Boost_STATIC_staticRT_REGEX_LIBRARY_DEBUG ${Boost_REGEX_LIBRARY_DEBUG})
  set(Boost_STATIC_staticRT_REGEX_LIBRARY_RELEASE ${Boost_REGEX_LIBRARY_RELEASE})
  set(Boost_STATIC_staticRT_SYSTEM_LIBRARY_DEBUG ${Boost_SYSTEM_LIBRARY_DEBUG})
  set(Boost_STATIC_staticRT_SYSTEM_LIBRARY_RELEASE ${Boost_SYSTEM_LIBRARY_RELEASE})
  set(Boost_STATIC_staticRT_THREAD_LIBRARY_DEBUG ${Boost_THREAD_LIBRARY_DEBUG})
  set(Boost_STATIC_staticRT_THREAD_LIBRARY_RELEASE ${Boost_THREAD_LIBRARY_RELEASE})

  message("Boost_STATIC_staticRT_VERSION: " ${Boost_VERSION})
  message("Boost_STATIC_staticRT_LIBRARIES: " ${Boost_STATIC_staticRT_LIBRARIES})
  message("Boost_STATIC_staticRT_LOCALE_LIBRARY_DEBUG: " ${Boost_STATIC_staticRT_LOCALE_LIBRARY_DEBUG})
  message("Boost_STATIC_staticRT_LOCALE_LIBRARY_RELEASE: " ${Boost_STATIC_staticRT_LOCALE_LIBRARY_RELEASE})
  message("Boost_STATIC_staticRT_REGEX_LIBRARY_DEBUG: " ${Boost_STATIC_staticRT_REGEX_LIBRARY_DEBUG})
  message("Boost_STATIC_staticRT_REGEX_LIBRARY_RELEASE: " ${Boost_STATIC_staticRT_REGEX_LIBRARY_RELEASE})
  message("Boost_STATIC_staticRT_SYSTEM_LIBRARY_DEBUG: " ${Boost_STATIC_staticRT_SYSTEM_LIBRARY_DEBUG})
  message("Boost_STATIC_staticRT_SYSTEM_LIBRARY_RELEASE: " ${Boost_STATIC_staticRT_SYSTEM_LIBRARY_RELEASE})
  message("Boost_STATIC_staticRT_THREAD_LIBRARY_DEBUG: " ${Boost_STATIC_staticRT_THREAD_LIBRARY_DEBUG})
  message("Boost_STATIC_staticRT_THREAD_LIBRARY_RELEASE: " ${Boost_STATIC_staticRT_THREAD_LIBRARY_RELEASE})
endif()
