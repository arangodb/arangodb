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
////////////////////////////////////////////////////////////////////////////////

#include "Transaction/Hints.h"

#include <iostream>
#include <string>

namespace arangodb::transaction {

std::string Hints::toString() const {
  std::string result;

  auto append = [&result](std::string_view value) {
    if (!result.empty()) {
      result.append(", ");
    }
    result.append(value);
  };

  if (has(Hint::SINGLE_OPERATION)) {
    append("single operation");
  }
  if (has(Hint::LOCK_NEVER)) {
    append("lock never");
  }
  if (has(Hint::NO_DLD)) {
    append("no dld");
  }
  if (has(Hint::NO_INDEXING)) {
    append("no indexing");
  }
  if (has(Hint::INTERMEDIATE_COMMITS)) {
    append("intermediate commits");
  }
  if (has(Hint::ALLOW_RANGE_DELETE)) {
    append("allow range delete");
  }
  if (has(Hint::FROM_TOPLEVEL_AQL)) {
    append("from toplevel aql");
  }
  if (has(Hint::GLOBAL_MANAGED)) {
    append("global managed");
  }
  if (has(Hint::INDEX_CREATION)) {
    append("index creation");
  }
  if (has(Hint::IS_FOLLOWER_TRX)) {
    append("is follower trx");
  }
  if (has(Hint::ALLOW_FAST_LOCK_ROUND_CLUSTER)) {
    append("allow fast lock round cluster");
  }
  if (result.empty()) {
    append("none");
  }

  return result;
}

std::ostream& operator<<(std::ostream& stream, Hints const& hints) {
  stream << hints.toString();
  return stream;
}

}  // namespace arangodb::transaction
