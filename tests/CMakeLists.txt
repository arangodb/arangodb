foreach (LINK_DIR ${V8_LINK_DIRECTORIES})
  link_directories("${LINK_DIR}")
endforeach()

# also includes our main.cpp so it is actually more than just the tests
file(GLOB_RECURSE TESTS . *.cpp)
# linking is a dark art...workmonitordummy is somehow necessary
add_executable(
  arangodbtests
  ../lib/Basics/WorkMonitorDummy.cpp
  ../UnitTests/Basics/icu-helper.cpp
  ${TESTS}
)

target_link_libraries(
  arangodbtests
  arangoserver
)

target_include_directories(arangodbtests PRIVATE
  ${INCLUDE_DIRECTORIES}
  ${CMAKE_SOURCE_DIR}/3rdParty/catch
  ${CMAKE_SOURCE_DIR}/3rdParty/fakeit
)
