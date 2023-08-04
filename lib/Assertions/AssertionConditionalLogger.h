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
/// @author Jan Steemann
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "AssertionConditionalStream.h"

#include "CrashHandler/CrashHandler.h"

namespace arangodb::debug {
struct AssertionConditionalLogger {
  void operator&(AssertionConditionalStream& stream) const {
    if (!stream.condition) {
      std::string message = stream.stream.str();
      arangodb::CrashHandler::assertionFailure(
          file, line, function, expr,
          message.empty() ? nullptr : message.c_str());
    } else {
      // need to clear the stream to avoid cumulation of assertion output!
      stream.stream.str({});
      stream.stream.clear();
    }
  }

  const char* file;
  int line;
  const char* function;
  const char* expr;

  static thread_local AssertionConditionalStream assertionStringStream;
};
}  // namespace arangodb::debug
