#add_executable(arangodbtests_crashhandler
#  CrashHandlerTest.cpp)

add_executable(arangodbtests_crashhandler_light
  CrashHandlerLightTest.cpp)

target_link_libraries(arangodbtests_crashhandler_light
  arango_crashhandler_light
  gtest
  gtest_main)
