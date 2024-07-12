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

#include "LogAppenderStream.h"

#include "Logger/LogMessage.h"

namespace arangodb {

LogAppenderStream::LogAppenderStream(std::string const& filename, int fd)
    : LogAppender(), _fd(fd), _useColors(false) {}

void LogAppenderStream::logMessage(LogMessage const& message) {
  this->writeLogMessage(message._level, message._topicId, message._message);
}

}  // namespace arangodb
