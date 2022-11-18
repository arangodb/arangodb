////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "CrashHandler.h"

#include <iostream>

// This is a Light/Mock version of ArangoDB's crash handler without
// dependencies on the remainder of ArangoDB.

namespace arangodb {
void CrashHandler::logBacktrace() {}

[[noreturn]] void CrashHandler::crash(std::string_view context) {
  std::cerr << "[LightCrashHandler] " << context << std::endl;
  std::flush(std::cerr);
  std::terminate();
};

[[noreturn]] void CrashHandler::assertionFailure(char const* file, int line,
                                                 char const* func,
                                                 char const* context,
                                                 const char* message) {
  CrashHandler::crash("BOOM");
}

void CrashHandler::setHardKill() {}

void CrashHandler::disableBacktraces() {}

void CrashHandler::installCrashHandler() {}

#ifdef _WIN32
void CrashHandler::setMiniDumpDirectory(std::string path) {}
#endif
}  // namespace arangodb
