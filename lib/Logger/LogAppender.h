////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
#include <array>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <typeindex>
#include <utility>
#include <vector>

#include "Basics/Common.h"
#include "Basics/Result.h"
#include "Logger/LogGroup.h"
#include "Logger/LogLevel.h"

namespace arangodb {
class LogTopic;
struct LogMessage;
class Mutex;

class LogAppender {
 public:
  static void addAppender(LogGroup const&, std::string const& definition);

  static void addGlobalAppender(LogGroup const&, std::shared_ptr<LogAppender>);

  static std::shared_ptr<LogAppender> buildAppender(LogGroup const&,
                                                    std::string const& output);

  static void logGlobal(LogGroup const&, LogMessage const&);
  static void log(LogGroup const&, LogMessage const&);

  static void reopen();
  static void shutdown();

  static bool haveAppenders(LogGroup const&, size_t topicId);

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
  static std::array<std::vector<std::shared_ptr<LogAppender>>, LogGroup::Count> _globalAppenders;
  static std::array<std::map<size_t, std::vector<std::shared_ptr<LogAppender>>>, LogGroup::Count> _topics2appenders;
  static std::array<std::map<std::string, std::shared_ptr<LogAppender>>, LogGroup::Count> _definition2appenders;
  static bool _allowStdLogging;
};
}  // namespace arangodb

#endif
