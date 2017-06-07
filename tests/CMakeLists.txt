foreach (LINK_DIR ${V8_LINK_DIRECTORIES})
  link_directories("${LINK_DIR}")
endforeach()

# linking is a dark art...workmonitordummy is somehow necessary
add_executable(
  arangodbtests
  main.cpp
  Agency/AddFollowerTest.cpp
  Agency/CleanOutServerTest.cpp
  Agency/FailedLeaderTest.cpp
  Agency/FailedServerTest.cpp
  Agency/MoveShardTest.cpp
  Agency/RemoveFollowerTest.cpp
  Basics/icu-helper.cpp
  Basics/AttributeNameParserTest.cpp
  Basics/associative-multi-pointer-test.cpp
  Basics/associative-multi-pointer-nohashcache-test.cpp
  Basics/conversions-test.cpp
  Basics/csv-test.cpp
  Basics/files-test.cpp
  Basics/fpconv-test.cpp
  Basics/json-test.cpp
  Basics/hashes-test.cpp
  Basics/skiplist-test.cpp
  Basics/priorityqueue-test.cpp
  Basics/string-buffer-test.cpp
  Basics/string-utf8-normalize-test.cpp
  Basics/string-utf8-test.cpp
  Basics/string-test.cpp
  Basics/vector-test.cpp
  Basics/structure-size-test.cpp
  Basics/EndpointTest.cpp
  Basics/StringBufferTest.cpp
  Basics/StringUtilsTest.cpp
  Basics/VelocyPackHelper-test.cpp
  Geo/georeg.cpp
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
