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
/// @author Achim Brandt
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <string>
#include <string_view>
#include <type_traits>

#include "LogAppender.h"

#include "Basics/operating-system.h"
#include "Basics/ReadLocker.h"
#include "Basics/RecursiveLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/WriteLocker.h"
#include "Basics/voc-errors.h"
#include "Logger/LogAppenderFile.h"
#include "Logger/LogAppenderSyslog.h"
#include "Logger/LogGroup.h"
#include "Logger/LogMacros.h"
#include "Logger/LogTopic.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

using namespace arangodb;
using namespace arangodb::basics;

void LogAppender::logMessageGuarded(LogMessage const& message) {
  // Only one thread is allowed to actually write logs to the file.
  // We use a recusive lock here, just in case writing the log message
  // causes a crash, in this case we may trigger another force-direct
  // log. This is not very likely, but it is better to be safe than sorry.
  RECURSIVE_WRITE_LOCKER(_logOutputMutex, _logOutputMutexOwner);
  logMessage(message);
}
