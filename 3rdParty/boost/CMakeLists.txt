project(boost)

#the boost version must also be adjusted in the top-level CMakeLists.txt file
set(boost_version "1.62.0")
set(BOOST_VERSION ${boost_version} PARENT_SCOPE)
set(boost_src "${CMAKE_CURRENT_SOURCE_DIR}/${boost_version}")

#create interface target for boost header only libraries
message(STATUS "using 3rdParty BOOST")
add_library(boost_boost INTERFACE)
target_include_directories(boost_boost SYSTEM INTERFACE "${boost_src}")

#build boost_system - there seems to be just one cpp file:)
add_library(boost_system STATIC
    ${boost_src}/libs/system/src/error_code.cpp
)
target_link_libraries(boost_system PUBLIC boost_boost) #this will add required includes
target_compile_definitions(boost_system PRIVATE "BOOST_SYSTEM_STATIC_LINK=1")
target_compile_definitions(boost_system PUBLIC  "BOOST_ALL_NO_LIB=1")
