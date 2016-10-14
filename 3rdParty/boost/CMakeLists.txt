project(boost)

set(boost_version "1.61.0")
set(boost_src "${CMAKE_CURRENT_SOURCE_DIR}/${boost_version}")
add_definitions(-DARANGODB_BOOST_VERSION=\"${Boost_version}\")

#find_package(Boost 1.61.0 EXACT COMPONENTS system)
#if(Boost_FOUND)
option(USE_BOOST_UNITTESTS "use boost unit-tests" ON)
if(false) #for now we do not use the system's boost but our own instead!
    add_library(boost_boost INTERFACE)
    target_include_directories(boost_boost PUBLIC SYSTEM ${Boost_INCLUDE_DIRS})
    # we are good
    # create imported targets?!
else()
    #create interface target for boost header only libraries
    message(STATUS "using 3rdParty BOOST")
    add_library(boost_boost INTERFACE)
    target_include_directories(boost_boost INTERFACE SYSTEM "${boost_src}")

    #build boost_system - there seems to be just one cpp file:)
    add_library(boost_system STATIC
        ${boost_src}/libs/system/src/error_code.cpp
    )
    target_link_libraries(boost_system PUBLIC boost_boost) #this will add required includes
    target_compile_definitions(boost_system PRIVATE "BOOST_SYSTEM_STATIC_LINK=1")
    target_compile_definitions(boost_system PUBLIC  "BOOST_ALL_NO_LIB=1")

    #set boost include path and add libraries to Boost_LIBRARIES variable
    #set(Boost_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/1.61.0)
    #set(Boost_LIBRARIES boost_system)
endif()

if (NOT USE_BOOST_UNITTESTS)
  message(STATUS "BOOST unit-tests are disabled")
endif ()
