add_executable(fuertetest
  VstTest.cpp
  ConnectionBasicTest.cpp
  ConnectionConcurrentTest.cpp
  ConnectionFailuresTest.cpp
  ConnectionTimeoutsTest.cpp
  main.cpp)

target_link_libraries(fuertetest
  fuerte
  gtest
  boost_boost
  arango
  arango_v8server
  arango_v8
  arangoserver
  ${MSVC_LIBS}
  ${OPENSSL_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT})

target_include_directories(fuertetest SYSTEM PRIVATE
  ${V8_INTERNAL_INCLUDE_DIR}
  ${INCLUDE_DIRECTORIES}
  ${CMAKE_SOURCE_DIR}/3rdParty/fakeit-gtest
  ${CMAKE_SOURCE_DIR}/${GOOGLETEST_ROOT}/include
  ${CMAKE_SOURCE_DIR}/3rdParty/fuerte/src
  )
