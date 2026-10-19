////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Result.h"
#include "Utils/ExecContext.h"

#include <optional>
#include <string_view>

namespace arangodb {

/**
 * Remembers whether the calling identity may act on other users' resources
 *
 * `question` is an owner-based ExecContext check such as
 * &ExecContext::canKillTransaction. Acting on a foreign resource is the same
 * admin question for every owner, so the exec context is asked at most once
 * per operation, and only once a foreign resource shows up. Own resources are
 * decided by ExecContext without a question.
 *
 * Example:
 *   MonitorQueryAccess access;
 *   auto const isAccessible = [&access](Entry const& entry) {
 *     return access.isAccessible(entry.owner);
 *   };
 *   for (auto const& entry : entries | std::views::filter(isAccessible)) {
 *     ...
 *   }
 */
template<Result (ExecContext::*question)(std::string_view) const>
class ForeignAccess {
 public:
  /// @brief whether the calling identity may act on a resource of `owner`
  auto isAccessible(std::string_view owner) -> bool {
    auto const& exec = ExecContext::current();
    if (owner == exec.user()) {
      // own resources: ExecContext answers without asking, so there is
      // nothing to cache, and caching it here would let foreign ones through
      return (exec.*question)(owner).ok();
    }
    // foreign resources: the same admin question for every owner, so ask it
    // once and reuse the answer
    if (!_foreignAccessible.has_value()) {
      _foreignAccessible = (exec.*question)(owner).ok();
    }
    return _foreignAccessible.value();
  }

 private:
  std::optional<bool> _foreignAccessible;
};

/// @brief may the caller see other users' queries (listing, clearing slow ones)
using MonitorQueryAccess = ForeignAccess<&ExecContext::canMonitorQuery>;
/// @brief may the caller kill other users' queries (bulk abort)
using KillQueryAccess = ForeignAccess<&ExecContext::canKillQuery>;
/// @brief may the caller see other users' transactions (listing)
using MonitorTransactionAccess =
    ForeignAccess<&ExecContext::canMonitorTransaction>;
/// @brief may the caller abort other users' transactions (bulk abort)
using KillTransactionAccess = ForeignAccess<&ExecContext::canKillTransaction>;

}  // namespace arangodb
