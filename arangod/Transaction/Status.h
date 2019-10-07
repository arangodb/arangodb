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

#ifndef ARANGOD_TRANSACTION_STATUS_H
#define ARANGOD_TRANSACTION_STATUS_H 1

#include <cstdint>
#include <iosfwd>

#include "Basics/Common.h"
#include "Basics/debugging.h"

namespace arangodb {
namespace transaction {

/// @brief transaction statuses
enum class Status : uint32_t {
  UNDEFINED = 0,
  CREATED = 1,
  RUNNING = 2,
  COMMITTED = 3,
  ABORTED = 4
};

/// @brief return the status of the transaction as a string
static inline char const* statusString(Status status) {
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

Status statusFromString(char const* str, size_t len);

}  // namespace transaction
}  // namespace arangodb

std::ostream& operator<<(std::ostream& stream, arangodb::transaction::Status const& s);

#endif
