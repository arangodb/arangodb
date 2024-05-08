////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2024-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <atomic>
#include <memory>

namespace arangodb::aql {
class Query;

struct QueryAborter {
  QueryAborter(std::weak_ptr<Query> query) : _query(query) {}

  void abort() noexcept;

  bool wasAborted() const noexcept;

 private:
  std::weak_ptr<Query> _query;
  std::atomic<bool> _wasAborted{false};
};
}  // namespace arangodb::aql

