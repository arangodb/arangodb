add_executable(fuertetest
  ConnectionBasicTest.cpp
  ConnectionConcurrentTest.cpp
  ConnectionFailuresTest.cpp
  ConnectionTimeoutsTest.cpp
#    test_10000_writes.cpp
  main.cpp
)

target_link_libraries(fuertetest
    arango
    fuerte
    gtest
    boost_boost
    ${OPENSSL_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

target_include_directories(fuertetest SYSTEM PRIVATE
  ${INCLUDE_DIRECTORIES}
  ${CMAKE_SOURCE_DIR}/3rdParty/fuerte/src
  ${PROJECT_SOURCE_DIR}/lib
)
