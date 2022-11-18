#include <gtest/gtest.h>

#include <CrashHandler/CrashHandler.h>

using namespace arangodb;

TEST(CrashHandler, crashes) {
  EXPECT_EXIT(CrashHandler::crash("BOOM"), testing::KilledBySignal(SIGABRT),
              "BOOM");
}

TEST(CrashHandler, asserts) {
  EXPECT_EXIT(CrashHandler::assertionFailure(__FILE__, __LINE__, "asserts",
                                             "no context", "zebras"),
              testing::KilledBySignal(SIGABRT),
              testing::MatchesRegex("\\[LightCrashHandler\\] Assertion failed in file.*"));
}
