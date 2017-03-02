add_executable(
  arangodb_catch_tests
  main.cpp
)

target_include_directories(arangodb_catch_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/3rdParty/catch
)

target_include_directories(arangodb_catch_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/3rdParty/fakeit
)
