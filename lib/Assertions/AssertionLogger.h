////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include <sstream>

#include "Basics/SourceLocation.h"
#include "CrashHandler/CrashHandler.h"

namespace arangodb::debug {
struct AssertionLogger {
  [[noreturn]] void operator&(std::ostringstream const& stream) const {
    std::string message = stream.str();
    arangodb::CrashHandler::assertionFailure(
        location.file_name(), location.line(), function, expr,
        message.empty() ? nullptr : message.c_str());
  }
  // can be removed in C++20 because of LWG 1203
  [[noreturn]] void operator&(std::ostream const& stream) const {
    operator&(static_cast<std::ostringstream const&>(stream));
  }

  basics::SourceLocation location;
  const char* function;
  const char* expr;

  static thread_local std::ostringstream assertionStringStream;
};
}  // namespace arangodb::debug
