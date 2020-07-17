////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_LOGGER_LOG_APPENDER_H
#define ARANGODB_LOGGER_LOG_APPENDER_H 1

#include <stddef.h>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Basics/Common.h"
#include "Basics/Result.h"
#include "Logger/LogLevel.h"

namespace arangodb {
class LogTopic;
struct LogMessage;
class Mutex;

class LogAppender {
 public:
  static void addAppender(std::string const& definition);
  
  static void addGlobalAppender(std::shared_ptr<LogAppender>);
  
  static std::shared_ptr<LogAppender> buildAppender(std::string const& output);

  static void logGlobal(LogMessage const&);
  static void log(LogMessage const&);

  static void reopen();
  static void shutdown();

 public:
  LogAppender() = default;
  virtual ~LogAppender() = default;

  LogAppender(LogAppender const&) = delete;
  LogAppender& operator=(LogAppender const&) = delete;

 public:
  virtual void logMessage(LogMessage const&) = 0;

  virtual std::string details() const = 0;

  static bool allowStdLogging() { return _allowStdLogging; }
  static void allowStdLogging(bool value) { _allowStdLogging = value; }

  static Result parseDefinition(std::string const& definition, 
                                std::string& topicName,
                                std::string& output,
                                LogTopic*& topic);
 
 private:
  static Mutex _appendersLock;
  static std::vector<std::shared_ptr<LogAppender>> _globalAppenders;
  static std::map<size_t, std::vector<std::shared_ptr<LogAppender>>> _topics2appenders;
  static std::map<std::string, std::shared_ptr<LogAppender>> _definition2appenders;
  static bool _allowStdLogging;
};
}  // namespace arangodb

#endif
