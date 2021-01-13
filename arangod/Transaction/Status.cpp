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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Transaction/Status.h"

#include <iostream>
#include <cstring>

namespace arangodb {
namespace transaction {
Status statusFromString(char const* str, size_t len) {
  if (len == 9 && memcmp(str, "undefined", len) == 0) {
    return Status::UNDEFINED;
  } else if (len == 7 && memcmp(str, "created", len) == 0) {
    return Status::CREATED;
  } else if (len == 7 && memcmp(str, "running", len) == 0) {
    return Status::RUNNING;
  } else if (len == 9 && memcmp(str, "committed", len) == 0) {
    return Status::COMMITTED;
  } else if (len == 7 && memcmp(str, "aborted", len) == 0) {
    return Status::ABORTED;
  }

  TRI_ASSERT(false);
  return Status::UNDEFINED;
}
}  // namespace transaction
}  // namespace arangodb

std::ostream& operator<<(std::ostream& stream, arangodb::transaction::Status const& s) {
  stream << arangodb::transaction::statusString(s);
  return stream;
}
