////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include <glog/logging.h>
#include <iostream>

#include "Basics/Common.h"
#include "Logger/Logger.h"
#include "Logger/LogTopic.h"

static arangodb::LogLevel MapLogLevel(LogSeverity sev) {
  switch (sev) {
    case GLOG_INFO:
      return arangodb::LogLevel::INFO;
    case GLOG_WARNING:
      return arangodb::LogLevel::WARN;
    case GLOG_ERROR:
      return arangodb::LogLevel::ERR;
    case GLOG_FATAL:
      return arangodb::LogLevel::FATAL;
    default:
      return arangodb::LogLevel::ERR;
  }
}

google::LogStream::~LogStream() {
  try {
    arangodb::Logger::log("", "", 0, MapLogLevel(_severity),
                          arangodb::Logger::FIXME.id(), _out.str());
  } catch (...) {
    try {
      // logging the error may fail as well, and we should never throw in the dtor
      std::cerr << "failed to log: " << _out.str() << std::endl;
    } catch (...) {
    }
  }
  if (_severity >= GLOG_FATAL) {
    FATAL_ERROR_EXIT();
  }
}
