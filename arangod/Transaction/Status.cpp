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

#include "Transaction/Status.h"

#include "Basics/debugging.h"

#include <iostream>
#include <cstring>

namespace arangodb::transaction {

std::string_view statusString(Status status) {
  switch (status) {
    case transaction::Status::UNDEFINED:
      return "undefined";
    case transaction::Status::CREATED:
      return "created";
    case transaction::Status::RUNNING:
      return "running";
    case transaction::Status::COMMITTED:
      return "committed";
    case transaction::Status::ABORTED:
      return "aborted";
  }

  TRI_ASSERT(false);
  return "unknown";
}

Status statusFromString(std::string_view value) {
  if (value == "undefined") {
    return Status::UNDEFINED;
  }
  if (value == "created") {
    return Status::CREATED;
  }
  if (value == "running") {
    return Status::RUNNING;
  }
  if (value == "committed") {
    return Status::COMMITTED;
  }
  if (value == "aborted") {
    return Status::ABORTED;
  }

  TRI_ASSERT(false);
  return Status::UNDEFINED;
}

std::ostream& operator<<(std::ostream& stream,
                         arangodb::transaction::Status const& s) {
  stream << arangodb::transaction::statusString(s);
  return stream;
}

}  // namespace arangodb::transaction
