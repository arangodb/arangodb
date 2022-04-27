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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "OperationOptions.h"

#include "Basics/debugging.h"

using namespace arangodb;

OperationOptions::OperationOptions() noexcept
    : indexOperationMode(IndexOperationMode::kNormal),
      overwriteMode(OverwriteMode::kUnknown),
      nullBehavior(NullBehavior::kKeepAllNulls),
      waitForSync(false),
      validate(true),
      mergeObjects(true),
      silent(false),
      ignoreRevs(true),
      returnOld(false),
      returnNew(false),
      isRestore(false),
      checkUniqueConstraintsInPreflight(false),
      truncateCompact(true),
      documentCallFromAql(false),
      _context(nullptr) {}

OperationOptions::OperationOptions(ExecContext const& context) noexcept
    : OperationOptions() {
  _context = &context;
}

std::ostream& operator<<(std::ostream& os, OperationOptions const& ops) {
  // clang-format off
  os << "OperationOptions : " << std::boolalpha
     << "{ isSynchronousReplicationFrom : '" << ops.isSynchronousReplicationFrom << "'"
     << ", indexOperationMode : " << OperationOptions::stringifyIndexOperationMode(ops.indexOperationMode)
     << ", nullBehavior : " << OperationOptions::stringifyNullBehavior(ops.nullBehavior)
     << ", waitForSync : " << ops.waitForSync
     << ", validate : " << ops.validate
     << ", mergeObjects : " << ops.mergeObjects
     << ", silent : " << ops.silent
     << ", ignoreRevs : " << ops.ignoreRevs
     << ", returnOld :" << ops.returnOld
     << ", returnNew : "  << ops.returnNew
     << ", isRestore : " << ops.isRestore
     << ", overwriteMode : " << OperationOptions::stringifyOverwriteMode(ops.overwriteMode)
     << ", canDisableIndexing : " << ops.canDisableIndexing
     << " }" << std::endl;
  // clang-format on
  return os;
}

// get associate execution context
ExecContext const& OperationOptions::context() const {
  if (!_context) {
    return ExecContext::current();
  }
  return *_context;
}

/// @brief stringifies the overwrite mode
std::string_view OperationOptions::stringifyOverwriteMode(
    OperationOptions::OverwriteMode mode) noexcept {
  switch (mode) {
    case OverwriteMode::kUnknown:
      return "unknown";
    case OverwriteMode::kConflict:
      return "conflict";
    case OverwriteMode::kReplace:
      return "replace";
    case OverwriteMode::kUpdate:
      return "update";
    case OverwriteMode::kIgnore:
      return "ignore";
  }
  TRI_ASSERT(false);
  return "unknown";
}

OperationOptions::OverwriteMode OperationOptions::determineOverwriteMode(
    std::string_view value) noexcept {
  if (value == "conflict") {
    return OverwriteMode::kConflict;
  }
  if (value == "ignore") {
    return OverwriteMode::kIgnore;
  }
  if (value == "update") {
    return OverwriteMode::kUpdate;
  }
  if (value == "replace") {
    return OverwriteMode::kReplace;
  }
  return OverwriteMode::kUnknown;
}

std::string_view OperationOptions::stringifyIndexOperationMode(
    OperationOptions::IndexOperationMode mode) noexcept {
  switch (mode) {
    case IndexOperationMode::kNormal:
      return "normal";
    case IndexOperationMode::kRollback:
      return "rollback";
    case IndexOperationMode::kInternal:
      return "internal";
  }
  TRI_ASSERT(false);
  return "invalid";
}

std::string_view OperationOptions::stringifyNullBehavior(
    OperationOptions::NullBehavior nullBehavior) noexcept {
  switch (nullBehavior) {
    case NullBehavior::kKeepAllNulls:
      return "keepAllNulls";
    case NullBehavior::kRemoveSomeNullsOnUpdate:
      return "removeSomeNullsOnUpdate";
    case NullBehavior::kRemoveAllNulls:
      return "removeAllNulls";
  }
  TRI_ASSERT(false);
  return "invalid";
}
