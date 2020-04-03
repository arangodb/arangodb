////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include "OperationOptions.h"

#include "velocypack/StringRef.h"

using namespace arangodb;

/// @brief stringifies the overwrite mode
char const* OperationOptions::stringifyOverwriteMode(OperationOptions::OverwriteMode mode) {
  switch (mode) {
    case OverwriteMode::Unknown: return "unknown";
    case OverwriteMode::Conflict: return "conflict";
    case OverwriteMode::Replace: return "replace";
    case OverwriteMode::Update: return "update";
    case OverwriteMode::Ignore: return "ignore";
  }
  TRI_ASSERT(false);
  return "unknown";
}
  
OperationOptions::OverwriteMode OperationOptions::determineOverwriteMode(velocypack::StringRef value) {
  if (value == "conflict") {
    return OverwriteMode::Conflict;
  }
  if (value == "ignore") {
    return OverwriteMode::Ignore;
  }
  if (value == "update") {
    return OverwriteMode::Update;
  }
  if (value == "replace") {
    return OverwriteMode::Replace;
  }
  return OverwriteMode::Unknown;
}
