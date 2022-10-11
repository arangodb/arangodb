target_sources(arangodbtests PRIVATE
  DurationTest.cpp
  typedbuffer.cpp
  StatusTest.cpp)

set(USE_SEPARATE_PREGEL_TESTS_BINARY FALSE CACHE BOOL
    "Write a separate binary 'arangodbtests_pregel', containing only the pregel tests.")

if (USE_SEPARATE_PREGEL_TESTS_BINARY)

  add_executable(
    arangodbtests_pregel
    ${PROJECT_SOURCE_DIR}/tests/main.cpp
		${PROJECT_SOURCE_DIR}/tests/Basics/icu-helper.cpp
    DurationTest.cpp
    typedbuffer.cpp
    StatusTest.cpp)

  target_link_libraries(arangodbtests_pregel
    gtest
    arango_agency)

  target_include_directories(arangodbtests_pregel PRIVATE
    ${PROJECT_SOURCE_DIR}/arangod
    ${CMAKE_SOURCE_DIR}/${GOOGLETEST_ROOT}/include)
 
endif()

add_subdirectory(Messaging)
