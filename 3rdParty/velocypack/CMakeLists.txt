# -*- mode: CMAKE; -*-

cmake_minimum_required(VERSION 3.2)
project(velocypack CXX C)

set(CMAKE_CXX_STANDARD 17 CACHE STRING "target C++ standard")
message(STATUS "Require C++${CMAKE_CXX_STANDARD}")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")

option(BuildVelocyPackExamples "Build examples" ON)
option(Maintainer "Build maintainer tools" OFF)

set(HashType "xxhash" CACHE STRING "Hash type (fasthash, xxhash, wyhash)" )

# Set Build Type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

message(STATUS "Build mode: ${CMAKE_BUILD_TYPE}")
message(STATUS "Install base directory: ${CMAKE_INSTALL_PREFIX}")
set(VELOCYPACK_VERSION "0.1.33" CACHE STRING "VelocyPack version")
set(VELOCYPACK_DISPLAY_NAME "Velocypack")
set(VELOCYPACK_URL_INFO_ABOUT "https://github.com/arangodb/velocypack")
set(VELOCYPACK_CONTACT "hackers@arangodb.org")
set(VELOCYPACK_FRIENDLY_STRING "VelocyPack - A fast & compact serialization format")

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

#Compiler Setting
include(AR_CompilerSettings)

set(VELOCY_SOURCE
    src/velocypack-common.cpp
    src/AttributeTranslator.cpp
    src/Builder.cpp
    src/Collection.cpp
    src/Compare.cpp
    src/Dumper.cpp
    src/Exception.cpp
    src/HashedStringRef.cpp
    src/HexDump.cpp
    src/Iterator.cpp
    src/Options.cpp
    src/Parser.cpp
    src/Serializable.cpp
    src/Slice.cpp
    src/StringRef.cpp
    src/Utf8Helper.cpp
    src/Validator.cpp
    src/Value.cpp
    src/ValueType.cpp
    src/Version.cpp
    src/asm-functions.cpp
    src/asm-utf8check.cpp
    src/fpconv.cpp
    src/string-functions.cpp
)

if (CMAKE_CXX_STANDARD GREATER_EQUAL 17)
    list(APPEND VELOCY_SOURCE
        src/SharedSlice.cpp
    )
endif ()

#Use xxhash or fasthash?
if(HashType STREQUAL "xxhash")
    list(APPEND VELOCY_SOURCE src/xxhash.cpp)
    add_definitions("-DVELOCYPACK_XXHASH=1")
elseif(HashType STREQUAL "fasthash")
    list(APPEND VELOCY_SOURCE src/fasthash.cpp)
    add_definitions("-DVELOCYPACK_FASTHASH=1")
elseif(HashType STREQUAL "wyhash")
    add_definitions("-DVELOCYPACK_WYHASH=1")
else()
    message(FATAL_ERROR "invalid HashType value. supported values: xxhash, fasthash, wyhash")
endif()
message(STATUS "Building with hash type: ${HashType}")

add_library(velocypack STATIC ${VELOCY_SOURCE})
target_include_directories(velocypack PRIVATE src)
target_include_directories(velocypack PUBLIC include)

if(Maintainer)
    add_executable(buildVersion scripts/build-version.cpp)
    add_custom_target(buildVersionNumber
        COMMAND ${PROJECT_BINARY_DIR}/scripts/buildVersion
        ${PROJECT_SOURCE_DIR}/include/velocypack/velocypack-version-number.h
        DEPENDS ${PROJECT_BINARY_DIR}/scripts/buildVersion
        VERBATIM
    )
endif()

add_subdirectory(tools)
add_subdirectory(examples)
add_subdirectory(tests)

install(TARGETS velocypack DESTINATION lib)
install(
    DIRECTORY   "include/velocypack"
    DESTINATION include
)
include(CPack)
