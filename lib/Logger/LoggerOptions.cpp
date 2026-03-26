////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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

#include "LoggerOptions.h"

#include "Logger/LogTimeFormat.h"

#include "Basics/operating-system.h"  // required for TRI_HAVE_UNISTD_H

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

namespace arangodb {

LoggerOptions::LoggerOptions()
    : timeFormatString(LogTimeFormats::defaultFormatName()) {
  levels.push_back("info");

#ifdef TRI_HAVE_UNISTD_H
  // if stdout is a tty, then the default for foregroundTty becomes true
  foregroundTty = (isatty(STDOUT_FILENO) == 1);
#endif
}

}  // namespace arangodb
