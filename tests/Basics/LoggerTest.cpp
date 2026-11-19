////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/FileUtils.h"
#include "Basics/files.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StringUtils.h"

#include "gtest/gtest.h"

#include "Logger/LogAppender.h"
#include "Logger/LogAppenderFile.h"
#include "Logger/LogMacros.h"
#include "Logger/LogMessage.h"
#include "Logger/LogSecurity.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

#include <velocypack/Parser.h>
#include <velocypack/Slice.h>

#include <date/date.h>
#include <format>

#include <absl/strings/str_cat.h>
#include <array>
#include <bit>
#include <filesystem>
#include <mutex>
#include <regex>
#include <set>
#include <sstream>
#include <thread>
#include <vector>

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

using namespace arangodb;
using namespace arangodb::basics;

namespace {
struct Synchronizer {
  std::mutex mutex;
  std::condition_variable cv;
  bool ready = false;

  void waitForStart() {
    std::unique_lock lock(mutex);
    cv.wait(lock, [&] { return ready; });
  }
  void start() {
    {
      std::unique_lock lock(mutex);
      ready = true;
    }
    cv.notify_all();
  }
};

/// @brief log appender which captures all messages it receives.
class CapturingLogAppender final : public LogAppender {
 public:
  std::vector<std::string> take() {
    std::lock_guard guard(_mutex);
    return std::move(_messages);
  }

  std::string details() const override { return "capturing"; }

 protected:
  void logMessage(LogMessage const& message) override {
    std::lock_guard guard(_mutex);
    _messages.emplace_back(message._message);
  }

 private:
  std::mutex _mutex;
  std::vector<std::string> _messages;
};

/// @brief restarts the logger with the given output format. output format
/// settings can only be changed while logging is inactive, and shutting
/// down the logger removes all appenders, so they have to be re-added. this
/// mirrors the logger setup in tests/main.cpp.
void restartLogger(bool useJson,
                   std::shared_ptr<LogAppender> const& captureAppender) {
  Logger::shutdown();
  Logger::setUseJson(useJson);
  Logger::initialize(false, 10000);
  Logger::addAppender(Logger::defaultLogGroup(), "-");
  if (captureAppender != nullptr) {
    Logger::addGlobalAppender(Logger::defaultLogGroup(), captureAppender);
  }
}

constexpr std::array kAllSecurityCategories = {
    Security::Authentication,
    Security::AccessControl,
    Security::AccountManagement,
    Security::SystemAccess,
    Security::ConfigurationChanges,
    Security::PrivilegedOperations,
    Security::DataAccess,
    Security::Violations,
    Security::ApiCalls,
    Security::SessionManagement,
    Security::AuditLogAccess,
    Security::SystemErrors,
};

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

class LoggerTest : public ::testing::Test {
 protected:
  // store old state as backup
  std::vector<std::tuple<int, std::string, std::shared_ptr<LogAppenderFile>>>
      backup;
  std::string const path;
  std::string const logfile1;
  std::string const logfile2;

  LoggerTest()
      : backup(LogAppenderFileFactory::getAppenders()),
        path(TRI_GetTempPath()),
        logfile1(path + "logfile1"),
        logfile2(path + "logfile2") {
    std::error_code removeEc;
    std::filesystem::remove(logfile1, removeEc);
    std::filesystem::remove(logfile2, removeEc);
    // remove any previous loggers
    LogAppenderFileFactory::closeAll();
  }

  ~LoggerTest() {
    // restore old state
    LogAppenderFileFactory::setAppenders(backup);
    LogAppenderFileFactory::reopenAll();

    std::error_code removeEc;
    std::filesystem::remove(logfile1, removeEc);
    std::filesystem::remove(logfile2, removeEc);
  }
};

TEST_F(LoggerTest, test_fds) {
  auto logger1 = LogAppenderFileFactory::getFileAppender(logfile1);
  auto logger2 = LogAppenderFileFactory::getFileAppender(logfile2);

  auto fds = LogAppenderFileFactory::getAppenders();
  EXPECT_EQ(fds.size(), 2);

  EXPECT_EQ(std::get<1>(fds[0]), logfile1);
  EXPECT_EQ(std::get<2>(fds[0])->fd(), std::get<0>(fds[0]));

  logger1->logMessageGuarded(LogMessage(__FUNCTION__, __FILE__, __LINE__,
                                        LogLevel::ERR, 0, "some error message",
                                        0, true));
  logger2->logMessageGuarded(LogMessage(__FUNCTION__, __FILE__, __LINE__,
                                        LogLevel::WARN, 0,
                                        "some warning message", 0, true));

  std::string content = FileUtils::slurp(logfile1);
  EXPECT_NE(content.find("some error message"), std::string::npos);
  EXPECT_EQ(content.find("some warning message"), std::string::npos);

  content = FileUtils::slurp(logfile2);
  EXPECT_EQ(content.find("some error message"), std::string::npos);
  EXPECT_NE(content.find("some warning message"), std::string::npos);

  LogAppenderFileFactory::closeAll();
}

TEST_F(LoggerTest, test_fds_after_reopen) {
  auto logger1 = LogAppenderFileFactory::getFileAppender(logfile1);
  auto logger2 = LogAppenderFileFactory::getFileAppender(logfile2);

  auto fds = LogAppenderFileFactory::getAppenders();
  EXPECT_EQ(fds.size(), 2);

  EXPECT_EQ(std::get<1>(fds[0]), logfile1);
  EXPECT_EQ(std::get<2>(fds[0])->fd(), std::get<0>(fds[0]));

  logger1->logMessageGuarded(LogMessage(__FUNCTION__, __FILE__, __LINE__,
                                        LogLevel::ERR, 0, "some error message",
                                        0, true));
  logger2->logMessageGuarded(LogMessage(__FUNCTION__, __FILE__, __LINE__,
                                        LogLevel::WARN, 0,
                                        "some warning message", 0, true));

  std::string content = FileUtils::slurp(logfile1);

  EXPECT_NE(content.find("some error message"), std::string::npos);
  EXPECT_EQ(content.find("some warning message"), std::string::npos);

  content = FileUtils::slurp(logfile2);
  EXPECT_EQ(content.find("some error message"), std::string::npos);
  EXPECT_NE(content.find("some warning message"), std::string::npos);

  LogAppenderFileFactory::reopenAll();

  fds = LogAppenderFileFactory::getAppenders();
  EXPECT_EQ(fds.size(), 2);

  EXPECT_TRUE(std::get<0>(fds[0]) > STDERR_FILENO);
  EXPECT_EQ(std::get<1>(fds[0]), logfile1);
  EXPECT_EQ(std::get<2>(fds[0])->fd(), std::get<0>(fds[0]));

  logger1->logMessageGuarded(LogMessage(__FUNCTION__, __FILE__, __LINE__,
                                        LogLevel::ERR, 0,
                                        "some other error message", 0, true));
  logger2->logMessageGuarded(LogMessage(__FUNCTION__, __FILE__, __LINE__,
                                        LogLevel::WARN, 0,
                                        "some other warning message", 0, true));

  content = FileUtils::slurp(logfile1);
  EXPECT_EQ(content.find("some error message"), std::string::npos);
  EXPECT_EQ(content.find("some warning message"), std::string::npos);
  EXPECT_NE(content.find("some other error message"), std::string::npos);

  content = FileUtils::slurp(logfile2);
  EXPECT_EQ(content.find("some error message"), std::string::npos);
  EXPECT_EQ(content.find("some warning message"), std::string::npos);
  EXPECT_NE(content.find("some other warning message"), std::string::npos);

  LogAppenderFileFactory::closeAll();
}

TEST_F(LoggerTest, testTimeFormats) {
  using namespace std::chrono;

  {
    // server start time point
    sys_seconds startTp;
    {
      std::istringstream in{"2016-12-11 13:59:55"};
      in >> date::parse("%F %T", startTp);
    }

    // time point we are testing
    sys_seconds tp;
    {
      std::istringstream in{"2016-12-11 14:02:43"};
      in >> date::parse("%F %T", tp);
    }

    std::string out;

    out.clear();
    LogTimeFormats::writeTime(out, LogTimeFormats::TimeFormat::Uptime, tp,
                              startTp);
    EXPECT_EQ("168", out);

    ASSERT_TRUE(std::regex_match(out, std::regex("^[0-9]+$")));

    out.clear();
    LogTimeFormats::writeTime(out, LogTimeFormats::TimeFormat::UptimeMillis, tp,
                              startTp);
    EXPECT_EQ("168.000", out);
    ASSERT_TRUE(std::regex_match(out, std::regex("^[0-9]+\\.[0-9]{3}$")));

    out.clear();
    LogTimeFormats::writeTime(out, LogTimeFormats::TimeFormat::UptimeMicros, tp,
                              startTp);
    EXPECT_EQ("168.000000", out);
    ASSERT_TRUE(std::regex_match(out, std::regex("^[0-9]+\\.[0-9]{6}$")));

    out.clear();
    LogTimeFormats::writeTime(out, LogTimeFormats::TimeFormat::UnixTimestamp,
                              tp, startTp);
    EXPECT_EQ("1481464963", out);

    out.clear();
    LogTimeFormats::writeTime(
        out, LogTimeFormats::TimeFormat::UnixTimestampMillis, tp, startTp);
    EXPECT_EQ("1481464963.000", out);

    out.clear();
    LogTimeFormats::writeTime(
        out, LogTimeFormats::TimeFormat::UnixTimestampMicros, tp, startTp);
    EXPECT_EQ("1481464963.000000", out);

    out.clear();
    LogTimeFormats::writeTime(out, LogTimeFormats::TimeFormat::UTCDateString,
                              tp, startTp);
    EXPECT_EQ("2016-12-11T14:02:43Z", out);

    out.clear();
    LogTimeFormats::writeTime(
        out, LogTimeFormats::TimeFormat::UTCDateStringMillis, tp, startTp);
    EXPECT_EQ("2016-12-11T14:02:43.000Z", out);

    out.clear();
    LogTimeFormats::writeTime(
        out, LogTimeFormats::TimeFormat::UTCDateStringMicros, tp, startTp);
    EXPECT_EQ("2016-12-11T14:02:43.000000Z", out);

    out.clear();
    LogTimeFormats::writeTime(out, LogTimeFormats::TimeFormat::LocalDateString,
                              tp, startTp);
    ASSERT_TRUE(std::regex_match(
        out,
        std::regex("^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}$")));
  }

  {
    // server start time point
    date::sys_time<milliseconds> startTp;
    {
      std::istringstream in{"2020-12-02 11:57:02.701"};
      in >> date::parse("%F %T", startTp);
    }

    // time point we are testing
    date::sys_time<milliseconds> tp;
    {
      std::istringstream in{"2020-12-02 11:57:26.004"};
      in >> date::parse("%F %T", tp);
    }

    std::string out;

    out.clear();
    LogTimeFormats::writeTime(out, LogTimeFormats::TimeFormat::Uptime, tp,
                              startTp);
    EXPECT_EQ("23", out);
    ASSERT_TRUE(std::regex_match(out, std::regex("^[0-9]+$")));

    out.clear();
    LogTimeFormats::writeTime(out, LogTimeFormats::TimeFormat::UptimeMillis, tp,
                              startTp);
    EXPECT_EQ("23.303", out);
    ASSERT_TRUE(std::regex_match(out, std::regex("^[0-9]+\\.[0-9]{3}$")));

    out.clear();
    LogTimeFormats::writeTime(out, LogTimeFormats::TimeFormat::UptimeMicros, tp,
                              startTp);
    EXPECT_EQ("23.303000", out);
    ASSERT_TRUE(std::regex_match(out, std::regex("^[0-9]+\\.[0-9]{6}$")));

    out.clear();
    LogTimeFormats::writeTime(out, LogTimeFormats::TimeFormat::UnixTimestamp,
                              tp, startTp);
    EXPECT_EQ("1606910246", out);

    out.clear();
    LogTimeFormats::writeTime(
        out, LogTimeFormats::TimeFormat::UnixTimestampMillis, tp, startTp);
    EXPECT_EQ("1606910246.004", out);

    out.clear();
    LogTimeFormats::writeTime(
        out, LogTimeFormats::TimeFormat::UnixTimestampMicros, tp, startTp);
    EXPECT_EQ("1606910246.004000", out);

    out.clear();
    LogTimeFormats::writeTime(out, LogTimeFormats::TimeFormat::UTCDateString,
                              tp, startTp);
    EXPECT_EQ("2020-12-02T11:57:26Z", out);

    out.clear();
    LogTimeFormats::writeTime(
        out, LogTimeFormats::TimeFormat::UTCDateStringMillis, tp, startTp);
    EXPECT_EQ("2020-12-02T11:57:26.004Z", out);

    out.clear();
    LogTimeFormats::writeTime(
        out, LogTimeFormats::TimeFormat::UTCDateStringMicros, tp, startTp);
    EXPECT_EQ("2020-12-02T11:57:26.004000Z", out);

    out.clear();
    LogTimeFormats::writeTime(out, LogTimeFormats::TimeFormat::LocalDateString,
                              tp, startTp);
    ASSERT_TRUE(std::regex_match(
        out,
        std::regex("^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}$")));
  }
}

TEST_F(LoggerTest, test_many_loggers_same_file) {
  {
    auto logger1 = LogAppenderFileFactory::getFileAppender(logfile1);
    auto logger2 = LogAppenderFileFactory::getFileAppender(logfile1);

    // Both loggers need to have the same pointer
    EXPECT_EQ(logger1.get(), logger2.get());
  }
  // Now test concurrent usage
  constexpr size_t n = 4;
  std::vector<std::thread> threads;
  threads.reserve(n);

  auto guard = arangodb::scopeGuard([&]() noexcept {
    for (auto& t : threads) {
      t.join();
    }
  });

  Synchronizer s;
  constexpr size_t iterations = 100;

  std::vector<uint64_t> expectedValues;
  for (size_t i = 0; i < n; ++i) {
    expectedValues.emplace_back(0);
    threads.emplace_back([logfile1 = this->logfile1, &s, i]() {
      auto logger = LogAppenderFileFactory::getFileAppender(logfile1);

      s.waitForStart();

      for (size_t j = 0; j < iterations; ++j) {
        logger->logMessageGuarded(
            LogMessage(__FUNCTION__, __FILE__, __LINE__, LogLevel::ERR, 0,
                       std::format("Thread {} Message {}\n", i, j), 0, true));
      }
    });
  }

  s.start();

  guard.fire();

  // All Messages are written to the same file, let's check if they are in
  // correct ordering
  std::string content = FileUtils::slurp(logfile1);
  auto stream = std::stringstream{content};

  // Let us read file from top to bottom.
  // In each line we should have exactly one message from one thread
  // For every thread the messages have to be strictly ordered
  // The messages from different threads can be interleaved
  // Every thread needs to have exactly iterations messages
  for (std::string line; std::getline(stream, line, '\n');) {
    std::vector<std::string> splits;
    auto streamLine = std::stringstream{line};
    for (std::string segment; std::getline(streamLine, segment, ' ');) {
      splits.push_back(segment);
    }
    auto tId = StringUtils::uint64(splits.at(1));
    auto messageId = StringUtils::uint64(splits.at(3));
    EXPECT_EQ(expectedValues.at(tId), messageId);
    expectedValues[tId]++;
  }

  for (size_t i = 0; i < n; ++i) {
    EXPECT_EQ(expectedValues.at(i), iterations)
        << "Thread " << i << " did not have enough messages";
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                            security event logging
// -----------------------------------------------------------------------------

class LoggerSecurityTest : public ::testing::Test {
 protected:
  LoggerSecurityTest() : appender(std::make_shared<CapturingLogAppender>()) {}

  ~LoggerSecurityTest() {
    Logger::setAlwaysLogSecurityEvents(false);
    // restore the default test logger setup
    restartLogger(/*useJson*/ false, nullptr);
  }

  void startCapturing(bool useJson) { restartLogger(useJson, appender); }

  std::vector<std::string> collect() { return appender->take(); }

  std::shared_ptr<CapturingLogAppender> appender;
};

TEST_F(LoggerSecurityTest, classifiers_are_distinct_and_canonical) {
  std::set<std::string_view> seen;
  for (auto s : kAllSecurityCategories) {
    auto classifier = securityClassifier(s);
    EXPECT_TRUE(classifier.starts_with("arango-security-event-")) << classifier;
    EXPECT_TRUE(seen.insert(classifier).second) << classifier;
  }
}

TEST_F(LoggerSecurityTest, categories_are_distinct_single_bits) {
  std::uint32_t seen = 0;
  for (auto s : kAllSecurityCategories) {
    auto bits = static_cast<std::uint16_t>(s);
    EXPECT_TRUE(std::has_single_bit(bits)) << bits;
    EXPECT_EQ(seen & bits, 0u) << bits;
    seen |= bits;
  }
}

TEST_F(LoggerSecurityTest, single_category_classifiers) {
  for (auto s : kAllSecurityCategories) {
    auto classifier = securityClassifier(s);
    EXPECT_FALSE(classifier.empty());
    // single categories are served from static storage, i.e. repeated
    // lookups return the very same characters
    EXPECT_EQ(securityClassifier(s).data(), classifier.data());
    EXPECT_EQ(classifier.find(kSecurityClassifierSeparator),
              std::string_view::npos);
    std::ostringstream os;
    os << s;
    EXPECT_EQ(os.str(), classifier);
  }
}

TEST_F(LoggerSecurityTest, two_category_classifiers) {
  auto s = Security::SessionManagement | Security::Authentication;
  EXPECT_TRUE(hasSecurityCategory(s, Security::Authentication));
  EXPECT_TRUE(hasSecurityCategory(s, Security::SessionManagement));
  EXPECT_FALSE(hasSecurityCategory(s, Security::DataAccess));
  // ordered by bit position, not by order of OR-ing
  std::string_view expected =
      "arango-security-event-authentication,"
      "arango-security-event-session-management";
  auto classifier = securityClassifier(s);
  EXPECT_EQ(classifier, expected);
  // the same combination in a different OR order is the same cache entry,
  // and the cached characters never move
  auto again = securityClassifier(Security::Authentication |
                                  Security::SessionManagement);
  EXPECT_EQ(again.data(), classifier.data());
  std::ostringstream os;
  os << s;
  EXPECT_EQ(os.str(), expected);
}

TEST_F(LoggerSecurityTest, all_category_classifiers) {
  Security all = kAllSecurityCategories[0];
  std::string expected;
  for (auto s : kAllSecurityCategories) {
    all |= s;
    if (!expected.empty()) {
      expected += kSecurityClassifierSeparator;
    }
    expected += securityClassifier(s);
  }
  EXPECT_EQ(securityClassifier(all), expected);
}

TEST_F(LoggerSecurityTest, message_is_enabled_for_level) {
  LogMessage plain("f", "file", 1, LogLevel::DEBUG, 0, "msg", 0, false);
  EXPECT_TRUE(plain.isEnabledFor(LogLevel::TRACE));
  EXPECT_TRUE(plain.isEnabledFor(LogLevel::DEBUG));
  EXPECT_FALSE(plain.isEnabledFor(LogLevel::INFO));
  EXPECT_FALSE(plain.isEnabledFor(LogLevel::FATAL));

  LogMessage forced("f", "file", 1, LogLevel::TRACE, 0, "msg", 0, false,
                    /*forceOutput*/ true);
  EXPECT_TRUE(forced.isEnabledFor(LogLevel::TRACE));
  EXPECT_TRUE(forced.isEnabledFor(LogLevel::INFO));
  EXPECT_TRUE(forced.isEnabledFor(LogLevel::FATAL));
}

TEST_F(LoggerSecurityTest, empty_and_unknown_bits) {
  EXPECT_TRUE(securityClassifier(static_cast<Security>(0)).empty());
  // bits above the known categories are ignored
  EXPECT_TRUE(securityClassifier(static_cast<Security>(1u << 15)).empty());
  EXPECT_EQ(securityClassifier(Security::DataAccess |
                               static_cast<Security>(1u << 15)),
            securityClassifier(Security::DataAccess));
}

TEST_F(LoggerSecurityTest, combination_cache_is_stable_under_growth) {
  // populate the cache with many combinations (forcing rehashes) and check
  // that string_views handed out earlier are still valid and unchanged
  auto first = securityClassifier(Security::ApiCalls | Security::Violations);
  std::string firstCopy(first);
  std::vector<std::string_view> views;
  for (std::uint16_t bits = 3; bits < (1u << 12); ++bits) {
    if (!std::has_single_bit(bits)) {
      views.push_back(securityClassifier(static_cast<Security>(bits)));
    }
  }
  EXPECT_EQ(first, firstCopy);
  EXPECT_EQ(
      securityClassifier(Security::ApiCalls | Security::Violations).data(),
      first.data());
  size_t i = 0;
  for (std::uint16_t bits = 3; bits < (1u << 12); ++bits) {
    if (!std::has_single_bit(bits)) {
      EXPECT_EQ(securityClassifier(static_cast<Security>(bits)).data(),
                views[i].data());
      ++i;
    }
  }
}

TEST_F(LoggerSecurityTest, combination_cache_is_thread_safe) {
  constexpr std::uint16_t kCombinations = 64;
  std::vector<std::thread> threads;
  std::vector<std::vector<std::string_view>> results(8);
  for (auto& result : results) {
    threads.emplace_back([&result] {
      for (std::uint16_t i = 0; i < kCombinations; ++i) {
        // two bits each: bit 11 plus a varying lower part
        auto bits = static_cast<std::uint16_t>((1u << 11) | (i + 1));
        result.push_back(securityClassifier(static_cast<Security>(bits)));
      }
    });
  }
  for (auto& t : threads) {
    t.join();
  }
  for (auto const& result : results) {
    ASSERT_EQ(result.size(), kCombinations);
    for (std::uint16_t i = 0; i < kCombinations; ++i) {
      // all threads must observe the very same cached string
      EXPECT_EQ(result[i].data(), results[0][i].data());
      EXPECT_TRUE(result[i].ends_with("arango-security-event-system-errors"))
          << result[i];
    }
  }
}

TEST_F(LoggerSecurityTest, text_format_contains_multiple_classifiers) {
  startCapturing(/*useJson*/ false);
  LOG_SECURITY("8d2fd", INFO, Logger::FIXME,
               Security::Authentication | Security::Violations)
      << "multi security test message";

  auto messages = collect();
  ASSERT_EQ(messages.size(), 1);
  EXPECT_NE(messages[0].find("[arango-security-event-authentication,"
                             "arango-security-event-violations] "),
            std::string::npos)
      << messages[0];
}

TEST_F(LoggerSecurityTest, json_format_contains_multiple_classifiers) {
  startCapturing(/*useJson*/ true);
  LOG_SECURITY("8d2fe", INFO, Logger::FIXME,
               Security::DataAccess | Security::AccessControl)
      << "multi json security test message";

  auto messages = collect();
  ASSERT_EQ(messages.size(), 1);
  auto builder = velocypack::Parser::fromJson(messages[0]);
  auto slice = builder->slice();
  ASSERT_TRUE(slice.isObject()) << messages[0];
  ASSERT_TRUE(slice.get("security").isString()) << messages[0];
  EXPECT_EQ(slice.get("security").stringView(),
            "arango-security-event-access-control,"
            "arango-security-event-data-access");
}

TEST_F(LoggerSecurityTest, text_format_contains_classifier) {
  startCapturing(/*useJson*/ false);
  LOG_SECURITY("8d2f1", INFO, Logger::FIXME, Security::Authentication)
      << "security test message";
  LOG_TOPIC("8d2f2", INFO, Logger::FIXME) << "plain test message";
  LOG_SECURITY_IF("8d2f3", INFO, Logger::FIXME, Security::AccessControl, true)
      << "conditional security test message";
  LOG_SECURITY_IF("8d2f4", INFO, Logger::FIXME, Security::AccessControl, false)
      << "suppressed security test message";

  auto messages = collect();
  ASSERT_EQ(messages.size(), 3);

  EXPECT_NE(messages[0].find("security test message"), std::string::npos);
  EXPECT_NE(
      messages[0].find("{general} [arango-security-event-authentication] "),
      std::string::npos)
      << messages[0];

  EXPECT_NE(messages[1].find("plain test message"), std::string::npos);
  EXPECT_EQ(messages[1].find("arango-security-event-"), std::string::npos)
      << messages[1];

  EXPECT_NE(messages[2].find("conditional security test message"),
            std::string::npos);
  EXPECT_NE(messages[2].find("[arango-security-event-access-control] "),
            std::string::npos)
      << messages[2];
}

TEST_F(LoggerSecurityTest, json_format_contains_classifier) {
  startCapturing(/*useJson*/ true);
  LOG_SECURITY("8d2f5", INFO, Logger::FIXME, Security::DataAccess)
      << "json \"security\" test message";
  LOG_TOPIC("8d2f6", INFO, Logger::FIXME) << "plain json test message";

  auto messages = collect();
  ASSERT_EQ(messages.size(), 2);

  {
    auto builder = velocypack::Parser::fromJson(messages[0]);
    auto slice = builder->slice();
    ASSERT_TRUE(slice.isObject()) << messages[0];
    ASSERT_TRUE(slice.get("security").isString()) << messages[0];
    EXPECT_EQ(slice.get("security").stringView(),
              "arango-security-event-data-access");
    EXPECT_EQ(slice.get("message").stringView(),
              "json \"security\" test message");
  }
  {
    auto builder = velocypack::Parser::fromJson(messages[1]);
    auto slice = builder->slice();
    ASSERT_TRUE(slice.isObject()) << messages[1];
    EXPECT_TRUE(slice.get("security").isNone()) << messages[1];
    EXPECT_EQ(messages[1].find("arango-security-event-"), std::string::npos);
  }
}

TEST_F(LoggerSecurityTest, all_categories_are_emitted) {
  for (bool useJson : {false, true}) {
    startCapturing(useJson);
    for (auto s : kAllSecurityCategories) {
      Logger::log("8d2f7", __FUNCTION__, __FILE__, __LINE__, LogLevel::INFO,
                  Logger::FIXME.id(), "category test message", s);
    }
    auto messages = collect();
    ASSERT_EQ(messages.size(), kAllSecurityCategories.size());
    for (size_t i = 0; i < messages.size(); ++i) {
      EXPECT_NE(messages[i].find(securityClassifier(kAllSecurityCategories[i])),
                std::string::npos)
          << "json: " << useJson << ", message: " << messages[i];
    }
  }
}

TEST_F(LoggerSecurityTest, security_events_respect_log_levels_by_default) {
  startCapturing(/*useJson*/ false);
  Logger::setAlwaysLogSecurityEvents(false);
  // DEBUG is below the default level of both the topic and the appender
  LOG_SECURITY("8d2f8", DEBUG, Logger::FIXME, Security::Authentication)
      << "debug security message";
  EXPECT_TRUE(collect().empty());
}

TEST_F(LoggerSecurityTest, security_events_are_forced_if_requested) {
  startCapturing(/*useJson*/ false);
  Logger::setAlwaysLogSecurityEvents(true);
  // DEBUG is below the default level of both the topic and the appender
  LOG_SECURITY("8d2f9", DEBUG, Logger::FIXME, Security::Authentication)
      << "forced debug security message";
  LOG_TOPIC("8d2fa", DEBUG, Logger::FIXME) << "plain debug message";
  LOG_SECURITY_IF("8d2fb", TRACE, Logger::FIXME, Security::AccessControl, true)
      << "forced trace security message";
  LOG_SECURITY_IF("8d2fc", TRACE, Logger::FIXME, Security::AccessControl, false)
      << "suppressed trace security message";

  auto messages = collect();
  ASSERT_EQ(messages.size(), 2);
  EXPECT_NE(messages[0].find("forced debug security message"),
            std::string::npos);
  EXPECT_NE(messages[0].find("[arango-security-event-authentication] "),
            std::string::npos)
      << messages[0];
  EXPECT_NE(messages[1].find("forced trace security message"),
            std::string::npos);
}
