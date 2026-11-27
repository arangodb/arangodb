////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Mocks/FakeRegistry.h"
#include "Mocks/TestLogger.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Logger/LogMacros.h"
#include "Logger/LogSecurity.h"
#include "Logger/Logger.h"
#include "RestServer/LogBufferFeature.h"
#include "RestServer/LogBufferFeatureOptions.h"

#include <string>
#include <vector>

using namespace arangodb;

namespace {

class LogBufferFeatureTest : public ::testing::Test {
 protected:
  // the in-memory appender only keeps messages up to INFO. the topic level
  // must be more verbose, so that the appender's own filtering is what is
  // tested. it is set before prepare(), because the appender takes the topic
  // levels at creation.
  LogBufferFeatureTest() : server(nullptr, nullptr) {
    Logger::FIXME.setLogLevel(LogLevel::DEBUG);
    feature = &server.addFeature<LogBufferFeature>(
        registry, LogBufferFeatureOptions{.minInMemoryLogLevel = "info",
                                          .useInMemoryAppender = true});
    feature->prepare();
  }

  ~LogBufferFeatureTest() {
    Logger::setOnDroppedMessage(nullptr);
    Logger::FIXME.setLogLevel(_oldTopicLevel);
  }

  std::vector<std::string> bufferedMessages(std::string const& search) {
    std::vector<std::string> result;
    for (auto const& entry :
         feature->entries(LogLevel::TRACE, 0, /*upToLevel*/ true, search)) {
      result.emplace_back(entry._message);
    }
    return result;
  }

  LogLevel const _oldTopicLevel = Logger::FIXME.level();
  metrics::FakeRegistry registry;
  application_features::ApplicationServer server;
  LogBufferFeature* feature = nullptr;
  // the feature registers global appenders, which refer to `registry`.
  // restarting the logger removes them, so this must be destroyed first.
  tests::ScopedTestLogger _testLogger;
};

}  // namespace

TEST_F(LogBufferFeatureTest, forced_security_events_bypass_min_level) {
  Logger::setAlwaysLogSecurityEvents(true);
  LOG_SECURITY("4c1a0", DEBUG, Logger::FIXME, LogSecurity::Authentication)
      << "forced ring buffer message";
  LOG_TOPIC("4c1a1", DEBUG, Logger::FIXME) << "plain ring buffer message";

  EXPECT_EQ(bufferedMessages("ring buffer message"),
            std::vector<std::string>{
                "{general} [arango-security-event-authentication] "
                "forced ring buffer message\n"});
}

TEST_F(LogBufferFeatureTest, security_events_respect_min_level_by_default) {
  Logger::setAlwaysLogSecurityEvents(false);
  LOG_SECURITY("4c1a2", DEBUG, Logger::FIXME, LogSecurity::Authentication)
      << "filtered ring buffer message";
  // proves that buffering works, so that the filtering above is meaningful
  LOG_SECURITY("4c1a3", INFO, Logger::FIXME, LogSecurity::Authentication)
      << "info ring buffer message";

  EXPECT_EQ(bufferedMessages("ring buffer message"),
            std::vector<std::string>{
                "{general} [arango-security-event-authentication] "
                "info ring buffer message\n"});
}
