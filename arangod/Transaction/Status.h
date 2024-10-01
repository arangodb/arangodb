////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#pragma once

#include <cstdint>
#include <iosfwd>
#include <string_view>

namespace arangodb::transaction {

/// @brief transaction statuses
enum class Status : uint32_t {
  UNDEFINED = 0,
  CREATED = 1,
  RUNNING = 2,
  COMMITTED = 3,
  ABORTED = 4
};

/// @brief return the status of the transaction as a string
std::string_view statusString(Status status);

Status statusFromString(std::string_view value);

std::ostream& operator<<(std::ostream& stream,
                         arangodb::transaction::Status const& s);

}  // namespace arangodb::transaction
