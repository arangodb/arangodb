////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for Logger
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"
#include "Basics/FileUtils.h"

#include "gtest/gtest.h"

#include "Logger/LogAppenderFile.h"
#include "Logger/Logger.h"

using namespace arangodb;
using namespace arangodb::basics;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

class LoggerTest : public ::testing::Test {
 protected:
  // store old state as backup
  std::vector<std::tuple<int, std::string, LogAppenderFile*>> backup;
  std::string const path;
  std::string const logfile1;
  std::string const logfile2;

  LoggerTest()
      : backup(LogAppenderFile::getFds()),
        path(TRI_GetTempPath()),
        logfile1(path + "logfile1"),
        logfile2(path + "logfile2") {
    FileUtils::remove(logfile1);
    FileUtils::remove(logfile2);
    // remove any previous loggers
    LogAppenderFile::clear();
  }

  ~LoggerTest() {
    // restore old state
    LogAppenderFile::setFds(backup);
    LogAppenderFile::reopenAll();

    FileUtils::remove(logfile1);
    FileUtils::remove(logfile2);
  }
};

TEST_F(LoggerTest, test_fds) {
  LogAppenderFile logger1(logfile1, "");
  LogAppenderFile logger2(logfile2, "");

  auto fds = LogAppenderFile::getFds();
  EXPECT_TRUE(fds.size() == 2);

  EXPECT_TRUE(std::get<1>(fds[0]) == logfile1);
  EXPECT_TRUE(std::get<2>(fds[0])->fd() == std::get<0>(fds[0]));

  logger1.logMessage(LogLevel::ERR, "some error message", 0);
  logger2.logMessage(LogLevel::WARN, "some warning message", 0);

  std::string content = FileUtils::slurp(logfile1);
  EXPECT_TRUE(content.find("some error message") != std::string::npos);
  EXPECT_TRUE(content.find("some warning message") == std::string::npos);

  content = FileUtils::slurp(logfile2);
  EXPECT_TRUE(content.find("some error message") == std::string::npos);
  EXPECT_TRUE(content.find("some warning message") != std::string::npos);

  LogAppenderFile::clear();
}

TEST_F(LoggerTest, test_fds_after_reopen) {
  LogAppenderFile logger1(logfile1, "");
  LogAppenderFile logger2(logfile2, "");

  auto fds = LogAppenderFile::getFds();
  EXPECT_TRUE(fds.size() == 2);

  EXPECT_TRUE(std::get<1>(fds[0]) == logfile1);
  EXPECT_TRUE(std::get<2>(fds[0])->fd() == std::get<0>(fds[0]));

  logger1.logMessage(LogLevel::ERR, "some error message", 0);
  logger2.logMessage(LogLevel::WARN, "some warning message", 0);

  std::string content = FileUtils::slurp(logfile1);
  EXPECT_TRUE(content.find("some error message") != std::string::npos);
  EXPECT_TRUE(content.find("some warning message") == std::string::npos);

  content = FileUtils::slurp(logfile2);
  EXPECT_TRUE(content.find("some error message") == std::string::npos);
  EXPECT_TRUE(content.find("some warning message") != std::string::npos);

  LogAppenderFile::reopenAll();

  fds = LogAppenderFile::getFds();
  EXPECT_TRUE(fds.size() == 2);

  EXPECT_TRUE(std::get<0>(fds[0]) > STDERR_FILENO);
  EXPECT_TRUE(std::get<1>(fds[0]) == logfile1);
  EXPECT_TRUE(std::get<2>(fds[0])->fd() == std::get<0>(fds[0]));

  logger1.logMessage(LogLevel::ERR, "some other error message", 0);
  logger2.logMessage(LogLevel::WARN, "some other warning message", 0);

  content = FileUtils::slurp(logfile1);
  EXPECT_TRUE(content.find("some error message") == std::string::npos);
  EXPECT_TRUE(content.find("some warning message") == std::string::npos);
  EXPECT_TRUE(content.find("some other error message") != std::string::npos);

  content = FileUtils::slurp(logfile2);
  EXPECT_TRUE(content.find("some error message") == std::string::npos);
  EXPECT_TRUE(content.find("some warning message") == std::string::npos);
  EXPECT_TRUE(content.find("some other warning message") != std::string::npos);

  LogAppenderFile::clear();
}
