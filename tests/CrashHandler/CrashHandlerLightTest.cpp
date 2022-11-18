#include <gtest/gtest.h>
#include <CrashHandler/CrashHandler.h>

TEST(CrashHandler, crashes) {
    arangodb::CrashHandler::crash("BOOM");
    ASSERT_TRUE(false);
}
