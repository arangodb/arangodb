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
/// @author Dr. Oreste Costa-Panaia
////////////////////////////////////////////////////////////////////////////////

#include "Basics/application-exit.h"

#include "Basics/CleanupFunctions.h"
#include "Basics/Common.h"
#include "Logger/Logger.h"

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

static void defaultExitFunction(int, void*);

TRI_ExitFunction_t TRI_EXIT_FUNCTION = defaultExitFunction;

void defaultExitFunction(int exitCode, void* /*data*/) {
  arangodb::basics::CleanupFunctions::run(exitCode, nullptr);
  _exit(exitCode);
}

void TRI_Application_Exit_SetExit(TRI_ExitFunction_t exitFunction) {
  if (exitFunction != nullptr) {
    TRI_EXIT_FUNCTION = exitFunction;
  } else {
    TRI_EXIT_FUNCTION = defaultExitFunction;
  }
}

[[noreturn]] void FATAL_ERROR_EXIT_CODE(int code) noexcept {
  try {
    arangodb::basics::CleanupFunctions::run(code, nullptr);
    arangodb::Logger::flush();
    arangodb::Logger::shutdown();
    TRI_EXIT_FUNCTION(code, nullptr);
  } catch (...) {
  }
  exit(code);
}

[[noreturn]] void FATAL_ERROR_EXIT() noexcept {
  FATAL_ERROR_EXIT_CODE(EXIT_FAILURE);
}

[[noreturn]] void FATAL_ERROR_ABORT() noexcept {
  try {
    arangodb::basics::CleanupFunctions::run(500, nullptr);
    arangodb::Logger::flush();
    arangodb::Logger::shutdown();
  } catch (...) {
  }
  std::abort();
}
