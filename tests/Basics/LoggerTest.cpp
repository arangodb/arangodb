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
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/FileUtils.h"
#include "Basics/files.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StringUtils.h"

#include "gtest/gtest.h"

#include "Logger/LogAppenderFile.h"
#include "Logger/LogMessage.h"
#include "Logger/Logger.h"
#include "fmt/format.h"

#include <date/date.h>

#include <regex>
#include <sstream>

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
    std::ignore = FileUtils::remove(logfile1);
    std::ignore = FileUtils::remove(logfile2);
    // remove any previous loggers
    LogAppenderFileFactory::closeAll();
  }

  ~LoggerTest() {
    // restore old state
    LogAppenderFileFactory::setAppenders(backup);
    LogAppenderFileFactory::reopenAll();

    std::ignore = FileUtils::remove(logfile1);
    std::ignore = FileUtils::remove(logfile2);
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
                       fmt::format("Thread {} Message {}\n", i, j), 0, true));
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
