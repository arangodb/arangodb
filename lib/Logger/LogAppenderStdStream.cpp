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

#include "LogAppenderStdStream.h"

#include <fcntl.h>
#include <stdio.h>
#include <cstring>

#include "ApplicationFeatures/ShellColorsFeature.h"
#include "Logger/Logger.h"

namespace arangodb {

LogAppenderStdStream::LogAppenderStdStream(std::string const& filename, int fd)
    : LogAppenderStream(filename, fd) {
  _useColors = ((isatty(_fd) == 1) && Logger::getUseColor());
}

LogAppenderStdStream::~LogAppenderStdStream() {
  // flush output stream on shutdown
  if (Logger::allowStdLogging()) {
    FILE* fp = (_fd == STDOUT_FILENO ? stdout : stderr);
    fflush(fp);
  }
}

void LogAppenderStdStream::writeLogMessage(LogLevel level, size_t topicId,
                                           std::string const& message) {
  writeLogMessage(_fd, _useColors, level, topicId, message);
}

void LogAppenderStdStream::writeLogMessage(int fd, bool useColors,
                                           LogLevel level, size_t /*topicId*/,
                                           std::string const& message) {
  if (!Logger::allowStdLogging()) {
    return;
  }

  // out stream
  FILE* fp = (fd == STDOUT_FILENO ? stdout : stderr);

  if (useColors) {
    // joyful color output
    if (level == LogLevel::FATAL || level == LogLevel::ERR) {
      fprintf(fp, "%s%s%s", ShellColorsFeature::SHELL_COLOR_RED,
              message.c_str(), ShellColorsFeature::SHELL_COLOR_RESET);
    } else if (level == LogLevel::WARN) {
      fprintf(fp, "%s%s%s", ShellColorsFeature::SHELL_COLOR_YELLOW,
              message.c_str(), ShellColorsFeature::SHELL_COLOR_RESET);
    } else {
      fprintf(fp, "%s%s%s", ShellColorsFeature::SHELL_COLOR_RESET,
              message.c_str(), ShellColorsFeature::SHELL_COLOR_RESET);
    }
  } else {
    // non-colored output
    fprintf(fp, "%s", message.c_str());
  }

  if (level == LogLevel::FATAL || level == LogLevel::ERR ||
      level == LogLevel::WARN || level == LogLevel::INFO) {
    // flush the output so it becomes visible immediately
    // at least for log levels that are used seldomly
    // it would probably be overkill to flush everytime we
    // encounter a log message for level DEBUG or TRACE
    fflush(fp);
  }
}

LogAppenderStderr::LogAppenderStderr()
    : LogAppenderStdStream("+", STDERR_FILENO) {}

LogAppenderStdout::LogAppenderStdout()
    : LogAppenderStdStream("-", STDOUT_FILENO) {}

}  // namespace arangodb
