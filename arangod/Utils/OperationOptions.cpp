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

#include "OperationOptions.h"

#include "Basics/debugging.h"
#include <velocypack/StringRef.h>

using namespace arangodb;

OperationOptions::OperationOptions()
: indexOperationMode(IndexOperationMode::normal),
  overwriteMode(OverwriteMode::Unknown),
  waitForSync(false),
  validate(true),
  keepNull(true),
  mergeObjects(true),
  silent(false),
  ignoreRevs(true),
  returnOld(false),
  returnNew(false),
  isRestore(false),
  ignoreUniqueConstraints(false),
  truncateCompact(true),
  _context(nullptr) {}

OperationOptions::OperationOptions(ExecContext const& context)
: OperationOptions() {
  _context = &context;
}

namespace {
const char* indexOpModeString(IndexOperationMode mode) {
  switch (mode) {
    case IndexOperationMode::normal:
      return "normal";
    case IndexOperationMode::rollback:
      return "rollback";
    case IndexOperationMode::internal:
      return "internal";
  }
  TRI_ASSERT(false);
  return "invalid";
}
}

// The following code does not work with VisualStudio 2019's `cl`
// Lets keep it for debugging on linux.
#ifndef _WIN32
std::ostream& operator<<(std::ostream& os, OperationOptions const& ops) {
  // clang-format off
  os << "OperationOptions : " << std::boolalpha
     << "{ isSynchronousReplicationFrom : '" << ops.isSynchronousReplicationFrom << "'"
     << ", indexOperationMode : " << ::indexOpModeString(ops.indexOperationMode)
     << ", waitForSync : " << ops.waitForSync
     << ", validate : " << ops.validate
     << ", keepNull : " << ops.keepNull
     << ", mergeObjects : " << ops.mergeObjects
     << ", silent : " << ops.silent
     << ", ignoreRevs : " << ops.ignoreRevs
     << ", returnOld :" << ops.returnOld
     << ", returnNew : "  << ops.returnNew
     << ", isRestore : " << ops.isRestore
     << ", overwriteMode : " << OperationOptions::stringifyOverwriteMode(ops.overwriteMode)
     << " }" << std::endl;
  // clang-format on
  return os;
}
#endif

// get associate execution context
ExecContext const& OperationOptions::context() const {
  if (!_context) {
    return ExecContext::current();
  }
  return *_context;
}

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
