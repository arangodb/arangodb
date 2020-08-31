////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_V8EXECUTOR_H
#define ARANGOD_AQL_V8EXECUTOR_H 1

#include "Basics/Common.h"

#include <v8.h>

namespace arangodb {
namespace basics {
class StringBuffer;
}

namespace aql {
class V8Executor {
 public:
  /// @brief checks if a V8 exception has occurred and throws an appropriate C++
  /// exception from it if so
  static void HandleV8Error(v8::TryCatch&, v8::Handle<v8::Value>&,
                            arangodb::basics::StringBuffer*, bool duringCompile);
};
}  // namespace aql
}  // namespace arangodb

#endif
