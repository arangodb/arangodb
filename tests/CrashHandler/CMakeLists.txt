#add_executable(arangodbtests_crashhandler
#  CrashHandlerTest.cpp)

add_executable(arangodbtests_crashhandler_light
  EXCLUDE_FROM_ALL
  CrashHandlerLightTest.cpp)

target_link_libraries(arangodbtests_crashhandler_light
  arango_crashhandler_light
  gtest
  gtest_main)

add_test(crashhandler_light
  COMMAND arangodbtests_crashhandler_light)
