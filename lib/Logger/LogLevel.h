////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Achim Brandt
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_LOGGER_LOG_LEVEL_H
#define ARANGODB_LOGGER_LOG_LEVEL_H 1

#include <iosfwd>

#include "Basics/operating-system.h"

#ifdef TRI_UNDEF_ERR
#undef ERR
#endif

namespace arangodb {
enum class LogLevel {
  DEFAULT = 0,
  FATAL = 1,
  ERR = 2,
  WARN = 3,
  INFO = 4,
  DEBUG = 5,
  TRACE = 6
};
}

std::ostream& operator<<(std::ostream&, arangodb::LogLevel);

#endif
