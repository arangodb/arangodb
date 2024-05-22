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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "CrashHandler.h"

#include <iostream>
#include <sstream>

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
  auto msg = std::ostringstream{};

  msg << "Assertion failed in file " << file << ":" << line << ", expression "
      << context << " with message " << message << std::endl;
  CrashHandler::crash(msg.str());
}

void CrashHandler::setHardKill() {
  CrashHandler::crash("setHardKill is not implemented");
}

void CrashHandler::disableBacktraces() {
  CrashHandler::crash("disabledBacktraces is not implemented.");
}

void CrashHandler::installCrashHandler() {
  CrashHandler::crash("installCrashHandler is not implemented.");
}

}  // namespace arangodb
