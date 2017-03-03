foreach (LINK_DIR ${V8_LINK_DIRECTORIES})
  link_directories("${LINK_DIR}")
endforeach()

# linking is a dark art...workmonitordummy is somehow necessary
add_executable(
  arangodb_catch_tests
  ../lib/Basics/WorkMonitorDummy.cpp
  main.cpp
)

target_link_libraries(
  arangodb_catch_tests
  arangoserver
)

target_include_directories(arangodb_catch_tests PRIVATE
  ${INCLUDE_DIRECTORIES}
  ${CMAKE_SOURCE_DIR}/3rdParty/catch
  ${CMAKE_SOURCE_DIR}/3rdParty/fakeit
)
