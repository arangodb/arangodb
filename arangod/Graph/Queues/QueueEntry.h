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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <variant>
#include <vector>
#include "Inspection/Types.h"

namespace arangodb::graph {

template<typename T, typename Step>
concept NeighbourCursor = requires(T t) {
  { t.next() } -> std::convertible_to<std::vector<Step>>;
  { t.hasMore() } -> std::convertible_to<bool>;
  { t.markForDeletion() };
};

template<typename Step, NeighbourCursor<Step> Cursor>
struct QueueEntry : std::variant<Step, std::reference_wrapper<Cursor>> {};

template<typename Step, NeighbourCursor<Step> Cursor, typename Inspector>
auto inspect(Inspector& f, QueueEntry<Step, Cursor>& x) {
  return f.variant(x).unqualified().alternatives(
      inspection::inlineType<Step>(), inspection::type<Cursor>("cursor"));
}

}  // namespace arangodb::graph
