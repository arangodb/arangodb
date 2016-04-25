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

#include "Basics/Common.h"

#include "Basics/Mutex.h"
#include "Basics/tri-strings.h"
#include "Logger/LogLevel.h"

namespace arangodb {
struct LogMessage;

class LogAppender {
 public:
  static void addLogger(std::function<void(LogMessage*)>);

  static void addAppender(std::string const& definition,
                          std::string const& contentFilter = "");
  static void addTtyAppender();

  static std::shared_ptr<LogAppender> buildAppender(
      std::string const& definition, std::string const& contentFilter);

  static void log(LogMessage*);
  static void writeStderr(LogLevel, std::string const&);

  static void reopen();
  static void shutdown();

 public:
  explicit LogAppender(std::string const& filter) : _filter(filter) {}
  virtual ~LogAppender() {}

 public:
  virtual bool logMessage(LogLevel, std::string const& message,
                          size_t offset) = 0;

  virtual std::string details() = 0;

 public:
  bool checkContent(std::string const& message) {
    return _filter.empty() ||
           TRI_IsContainedString(message.c_str(), _filter.c_str());
  }

 protected:
  std::string const _filter;  // an optional content filter for log messages

 private:
  static Mutex _appendersLock;
  static std::unique_ptr<LogAppender> _ttyAppender;
  static std::map<size_t, std::vector<std::shared_ptr<LogAppender>>>
      _topics2appenders;
  static std::map<std::pair<std::string, std::string>,
                  std::shared_ptr<LogAppender>> _definition2appenders;
  static std::vector<std::function<void(LogMessage*)>> _loggers;
};
}

#endif
