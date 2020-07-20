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

#ifndef ARANGODB_LOGGER_LOG_APPENDER_SYSLOG_H
#define ARANGODB_LOGGER_LOG_APPENDER_SYSLOG_H 1

#include "Logger/LogAppender.h"

namespace arangodb {
#ifdef ARANGODB_ENABLE_SYSLOG

class LogAppenderSyslog final : public LogAppender {
 public:
  static void close();

 public:
  LogAppenderSyslog(std::string const& facility, std::string const& name);

  void logMessage(LogMessage const& message) override final;

  std::string details() const override final;

 private:
  std::string const _sysname;

  static bool _opened;
};
#endif
}  // namespace arangodb

#endif
