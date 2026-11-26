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
#include "Mocks/Death_Test.h"
#include "Mocks/LogLevels.h"

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>

#include <format>

#include <absl/strings/str_cat.h>
#include <array>
#include <bit>
#include <filesystem>
#include <mutex>
#include <regex>
#include <sstream>
#include <string_view>
#include <thread>
#include <utility>
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

/// @brief log appender which captures the messages logged by the thread that
/// created it. only the part from the message offset onwards is kept, i.e.
/// without timestamp, process and thread identifiers, so that tests can
/// compare complete messages.
class CapturingLogAppender final : public LogAppender {
 public:
  std::vector<std::string> take() {
    std::lock_guard guard(_mutex);
    return std::move(_messages);
  }

  std::string details() const override { return "capturing"; }

 protected:
  void logMessage(LogMessage const& message) override {
    // global appenders receive messages from all threads, and leftover
    // threads of other tests must not influence the captured messages
    if (std::this_thread::get_id() != _owner) {
      return;
    }
    std::lock_guard guard(_mutex);
    _messages.emplace_back(
        std::string_view(message._message).substr(message._offset));
  }

 private:
  std::thread::id const _owner = std::this_thread::get_id();
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

// the classifiers are an external contract (ADR LOG-001), on which
// downstream log processing relies, so they are spelled out literally here.
constexpr std::array<std::pair<LogSecurity, std::string_view>, 12>
    kExpectedClassifiers = {{
        {LogSecurity::Authentication, "arango-security-event-authentication"},
        {LogSecurity::AccessControl, "arango-security-event-access-control"},
        {LogSecurity::AccountManagement,
         "arango-security-event-account-management"},
        {LogSecurity::SystemAccess, "arango-security-event-system-access"},
        {LogSecurity::ConfigurationChanges,
         "arango-security-event-configuration-changes"},
        {LogSecurity::PrivilegedOperations,
         "arango-security-event-privileged-operations"},
        {LogSecurity::DataAccess, "arango-security-event-data-access"},
        {LogSecurity::Violations, "arango-security-event-violations"},
        {LogSecurity::ApiCalls, "arango-security-event-api-calls"},
        {LogSecurity::SessionManagement,
         "arango-security-event-session-management"},
        {LogSecurity::AuditLogAccess, "arango-security-event-audit-log-access"},
        {LogSecurity::SystemErrors, "arango-security-event-system-errors"},
    }};
static_assert(kExpectedClassifiers.size() == std::popcount(kAllSecurityBits),
              "kExpectedClassifiers must list every LogSecurity category");

std::string classifierOf(LogSecurity security) {
  std::string result;
  appendSecurityClassifier(result, security);
  return result;
}

/// @brief parses a captured JSON log message, which must be an object.
std::shared_ptr<velocypack::Builder> parseJsonObject(std::string const& json) {
  auto builder = velocypack::Parser::fromJson(json);
  EXPECT_TRUE(builder->slice().isObject()) << json;
  return builder;
}

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
      in >> std::chrono::parse("%F %T", startTp);
    }

    // time point we are testing
    sys_seconds tp;
    {
      std::istringstream in{"2016-12-11 14:02:43"};
      in >> std::chrono::parse("%F %T", tp);
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
    std::chrono::sys_time<milliseconds> startTp;
    {
      std::istringstream in{"2020-12-02 11:57:02.701"};
      in >> std::chrono::parse("%F %T", startTp);
    }

    // time point we are testing
    std::chrono::sys_time<milliseconds> tp;
    {
      std::istringstream in{"2020-12-02 11:57:26.004"};
      in >> std::chrono::parse("%F %T", tp);
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
// --SECTION--                                     security event classification
// -----------------------------------------------------------------------------

TEST(LogSecurityTest, categories_are_distinct_single_bits) {
  std::uint32_t seen = 0;
  for (auto [s, _] : kExpectedClassifiers) {
    auto bits = static_cast<std::uint16_t>(s);
    EXPECT_TRUE(std::has_single_bit(bits)) << bits;
    EXPECT_EQ(seen & bits, 0u) << bits;
    seen |= bits;
  }
}

TEST(LogSecurityTest, each_category_has_its_canonical_classifier) {
  for (auto [s, expected] : kExpectedClassifiers) {
    EXPECT_EQ(classifierOf(s), expected);
    std::ostringstream os;
    os << s;
    EXPECT_EQ(os.str(), expected);
  }
}

TEST(LogSecurityTest, multiple_categories_are_joined_in_bit_order) {
  std::string_view expected =
      "arango-security-event-authentication,"
      "arango-security-event-session-management";
  // the order of OR-ing does not matter
  EXPECT_EQ(classifierOf(LogSecurity::SessionManagement |
                         LogSecurity::Authentication),
            expected);
  EXPECT_EQ(classifierOf(LogSecurity::Authentication |
                         LogSecurity::SessionManagement),
            expected);
  std::ostringstream os;
  os << (LogSecurity::SessionManagement | LogSecurity::Authentication);
  EXPECT_EQ(os.str(), expected);
}

TEST(LogSecurityTest, all_categories_are_joined_in_bit_order) {
  auto all = static_cast<LogSecurity>(kAllSecurityBits);
  std::string_view expected =
      "arango-security-event-authentication,"
      "arango-security-event-access-control,"
      "arango-security-event-account-management,"
      "arango-security-event-system-access,"
      "arango-security-event-configuration-changes,"
      "arango-security-event-privileged-operations,"
      "arango-security-event-data-access,"
      "arango-security-event-violations,"
      "arango-security-event-api-calls,"
      "arango-security-event-session-management,"
      "arango-security-event-audit-log-access,"
      "arango-security-event-system-errors";
  EXPECT_EQ(classifierOf(all), expected);
  std::ostringstream os;
  os << all;
  EXPECT_EQ(os.str(), expected);
}

TEST(LogSecurityTest, classifier_is_empty_without_known_category) {
  EXPECT_EQ(classifierOf(static_cast<LogSecurity>(0)), "");
  EXPECT_EQ(classifierOf(static_cast<LogSecurity>(1u << 15)), "");
}

TEST(LogSecurityTest, classifier_ignores_unknown_bits) {
  EXPECT_EQ(classifierOf(LogSecurity::DataAccess |
                         static_cast<LogSecurity>(1u << 15)),
            "arango-security-event-data-access");
}

TEST(LogSecurityTest, append_security_classifier_keeps_existing_content) {
  std::string out = "prefix:";
  appendSecurityClassifier(out, LogSecurity::Violations);
  EXPECT_EQ(out, "prefix:arango-security-event-violations");
}

TEST(LogSecurityTest, valid_security_requires_nonempty_mask_of_known_bits) {
  EXPECT_TRUE(isValidSecurity(LogSecurity::Authentication));
  EXPECT_TRUE(isValidSecurity(LogSecurity::SystemErrors));
  EXPECT_TRUE(isValidSecurity(LogSecurity::DataAccess | LogSecurity::ApiCalls));
  EXPECT_TRUE(isValidSecurity(static_cast<LogSecurity>(kAllSecurityBits)));
  EXPECT_FALSE(isValidSecurity(static_cast<LogSecurity>(0)));
  EXPECT_FALSE(isValidSecurity(static_cast<LogSecurity>(1u << 12)));
  EXPECT_FALSE(isValidSecurity(static_cast<LogSecurity>(1u << 15)));
  EXPECT_FALSE(isValidSecurity(LogSecurity::DataAccess |
                               static_cast<LogSecurity>(1u << 15)));
}

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
// an empty mask would produce an empty classifier. std::nullopt is the way to
// mark a message as "not a security event".
TEST(LogSecurityDeathTest, empty_security_mask_is_rejected) {
  EXPECT_DEATH_CORE_FREE(
      LOG_SECURITY("8d304", INFO, Logger::FIXME, LogSecurity{}) << "invalid",
      "invalid security category mask");
}
#endif

TEST(LogMessageTest, is_enabled_for_level_unless_output_is_forced) {
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

// -----------------------------------------------------------------------------
// --SECTION--                                            security event logging
// -----------------------------------------------------------------------------

class LoggerSecurityTest : public ::testing::Test {
 protected:
  LoggerSecurityTest() : appender(std::make_shared<CapturingLogAppender>()) {}

  ~LoggerSecurityTest() {
    // restore the default test logger setup
    restartLogger(/*useJson*/ false, nullptr);
    Logger::FIXME.setLogLevel(_oldTopicLevel);
  }

  // restarting the logger resets the topic levels to those of the appenders,
  // so the level of the topic used in the tests is set afterwards.
  // messages at INFO pass the topic level, those at DEBUG do not.
  void startCapturing(bool useJson) {
    restartLogger(useJson, appender);
    Logger::FIXME.setLogLevel(LogLevel::INFO);
  }

  std::vector<std::string> collect() { return appender->take(); }

  tests::ScopedAlwaysLogSecurityEvents _alwaysLogSecurityEventsGuard;
  LogLevel const _oldTopicLevel = Logger::FIXME.level();
  std::shared_ptr<CapturingLogAppender> appender;
};

TEST_F(LoggerSecurityTest, security_message_carries_classifier_in_text) {
  startCapturing(/*useJson*/ false);
  LOG_SECURITY("8d2f1", INFO, Logger::FIXME, LogSecurity::Authentication)
      << "security test message";
  EXPECT_EQ(collect(), std::vector<std::string>{
                           "{general} [arango-security-event-authentication] "
                           "security test message\n"});
}

TEST_F(LoggerSecurityTest, plain_message_has_no_classifier_in_text) {
  startCapturing(/*useJson*/ false);
  LOG_TOPIC("8d2f2", INFO, Logger::FIXME) << "plain test message";
  EXPECT_EQ(collect(),
            std::vector<std::string>{"{general} plain test message\n"});
}

TEST_F(LoggerSecurityTest, multiple_classifiers_in_text) {
  startCapturing(/*useJson*/ false);
  LOG_SECURITY("8d2fd", INFO, Logger::FIXME,
               LogSecurity::Authentication | LogSecurity::Violations)
      << "multi security test message";
  EXPECT_EQ(collect(), std::vector<std::string>{
                           "{general} [arango-security-event-authentication,"
                           "arango-security-event-violations] "
                           "multi security test message\n"});
}

TEST_F(LoggerSecurityTest, log_security_if_emits_only_if_condition_holds) {
  startCapturing(/*useJson*/ false);
  LOG_SECURITY_IF("8d2f3", INFO, Logger::FIXME, LogSecurity::AccessControl,
                  true)
      << "conditional security test message";
  LOG_SECURITY_IF("8d2f4", INFO, Logger::FIXME, LogSecurity::AccessControl,
                  false)
      << "suppressed security test message";
  EXPECT_EQ(collect(), std::vector<std::string>{
                           "{general} [arango-security-event-access-control] "
                           "conditional security test message\n"});
}

TEST_F(LoggerSecurityTest, security_message_carries_classifier_in_json) {
  startCapturing(/*useJson*/ true);
  LOG_SECURITY("8d2f5", INFO, Logger::FIXME, LogSecurity::DataAccess)
      << "json \"security\" test message";

  auto messages = collect();
  ASSERT_EQ(messages.size(), 1);
  auto builder = parseJsonObject(messages[0]);
  auto slice = builder->slice();
  ASSERT_TRUE(slice.get("security").isString()) << messages[0];
  EXPECT_EQ(slice.get("security").stringView(),
            "arango-security-event-data-access");
  EXPECT_EQ(slice.get("message").stringView(),
            "json \"security\" test message");
}

TEST_F(LoggerSecurityTest, plain_message_has_no_classifier_in_json) {
  startCapturing(/*useJson*/ true);
  LOG_TOPIC("8d2f6", INFO, Logger::FIXME) << "plain json test message";

  auto messages = collect();
  ASSERT_EQ(messages.size(), 1);
  auto builder = parseJsonObject(messages[0]);
  EXPECT_TRUE(builder->slice().get("security").isNone()) << messages[0];
  EXPECT_EQ(messages[0].find("arango-security-event-"), std::string::npos);
}

TEST_F(LoggerSecurityTest, multiple_classifiers_in_json) {
  startCapturing(/*useJson*/ true);
  LOG_SECURITY("8d2fe", INFO, Logger::FIXME,
               LogSecurity::DataAccess | LogSecurity::AccessControl)
      << "multi json security test message";

  auto messages = collect();
  ASSERT_EQ(messages.size(), 1);
  auto builder = parseJsonObject(messages[0]);
  auto security = builder->slice().get("security");
  ASSERT_TRUE(security.isString()) << messages[0];
  EXPECT_EQ(security.stringView(),
            "arango-security-event-access-control,"
            "arango-security-event-data-access");
}

TEST_F(LoggerSecurityTest, all_categories_are_emitted) {
  for (bool useJson : {false, true}) {
    startCapturing(useJson);
    for (auto [s, _] : kExpectedClassifiers) {
      Logger::log("8d2f7", __FUNCTION__, __FILE__, __LINE__, LogLevel::INFO,
                  Logger::FIXME.id(), "category test message", s);
    }
    auto messages = collect();
    ASSERT_EQ(messages.size(), kExpectedClassifiers.size());
    for (size_t i = 0; i < messages.size(); ++i) {
      auto expected = kExpectedClassifiers[i].second;
      if (useJson) {
        auto builder = parseJsonObject(messages[i]);
        auto security = builder->slice().get("security");
        ASSERT_TRUE(security.isString()) << messages[i];
        EXPECT_EQ(security.stringView(), expected);
      } else {
        EXPECT_EQ(messages[i], absl::StrCat("{general} [", expected,
                                            "] category test message\n"));
      }
    }
  }
}

TEST_F(LoggerSecurityTest, security_events_respect_log_levels_by_default) {
  startCapturing(/*useJson*/ false);
  Logger::setAlwaysLogSecurityEvents(false);
  LOG_SECURITY("8d2f8", DEBUG, Logger::FIXME, LogSecurity::Authentication)
      << "debug security message";
  // proves that capturing works, so that the filtering above is meaningful
  LOG_SECURITY("8d2ff", INFO, Logger::FIXME, LogSecurity::Authentication)
      << "info security message";
  EXPECT_EQ(collect(), std::vector<std::string>{
                           "{general} [arango-security-event-authentication] "
                           "info security message\n"});
}

TEST_F(LoggerSecurityTest, security_events_are_forced_if_requested) {
  startCapturing(/*useJson*/ false);
  Logger::setAlwaysLogSecurityEvents(true);
  LOG_SECURITY("8d2f9", DEBUG, Logger::FIXME, LogSecurity::Authentication)
      << "forced debug security message";
  LOG_TOPIC("8d2fa", DEBUG, Logger::FIXME) << "plain debug message";
  LOG_SECURITY_IF("8d2fb", TRACE, Logger::FIXME, LogSecurity::AccessControl,
                  true)
      << "forced trace security message";
  LOG_SECURITY_IF("8d2fc", TRACE, Logger::FIXME, LogSecurity::AccessControl,
                  false)
      << "suppressed trace security message";

  EXPECT_EQ(collect(), (std::vector<std::string>{
                           "{general} [arango-security-event-authentication] "
                           "forced debug security message\n",
                           "{general} [arango-security-event-access-control] "
                           "forced trace security message\n"}));
}

// the topic level and the appender level are separate filters. here only the
// appender is stricter than the message level, so the topic level does not
// interfere.
TEST_F(LoggerSecurityTest, forced_security_events_bypass_appender_level) {
  startCapturing(/*useJson*/ false);
  appender->setLogLevel(Logger::FIXME, LogLevel::ERR);
  Logger::setAlwaysLogSecurityEvents(true);
  LOG_SECURITY("8d300", INFO, Logger::FIXME, LogSecurity::Authentication)
      << "forced security message";
  LOG_TOPIC("8d301", INFO, Logger::FIXME) << "plain info message";
  EXPECT_EQ(collect(), std::vector<std::string>{
                           "{general} [arango-security-event-authentication] "
                           "forced security message\n"});
}

TEST_F(LoggerSecurityTest, security_events_respect_appender_level_by_default) {
  startCapturing(/*useJson*/ false);
  appender->setLogLevel(Logger::FIXME, LogLevel::ERR);
  Logger::setAlwaysLogSecurityEvents(false);
  LOG_SECURITY("8d302", INFO, Logger::FIXME, LogSecurity::Authentication)
      << "filtered security message";
  // proves that capturing works, so that the filtering above is meaningful
  LOG_SECURITY("8d303", ERR, Logger::FIXME, LogSecurity::Authentication)
      << "error security message";
  EXPECT_EQ(collect(), std::vector<std::string>{
                           "{general} [arango-security-event-authentication] "
                           "error security message\n"});
}
