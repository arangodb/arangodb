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

namespace arangodb::aql {
class AqlItemBlockInputRange;
enum class ExecutorState;
class OutputAqlItemRow;

class SortedRowsStorageBackend {
 public:
  virtual ~SortedRowsStorageBackend() = default;

  // add more input to the storage backend
  virtual void consumeInputRange(AqlItemBlockInputRange& inputRange) = 0;

  virtual bool hasReachedCapacityLimit() const noexcept = 0;

  // whether or not there is more output that the storage
  // backend can produce. requires seal() to have been
  // called!
  virtual bool hasMore() const = 0;

  // produce an output row. requires hasMore()
  virtual void produceOutputRow(OutputAqlItemRow& output) = 0;

  // skip an output row. requires hasMore()
  virtual void skipOutputRow() noexcept = 0;

  virtual bool spillOver(SortedRowsStorageBackend& other) = 0;
};

}  // namespace arangodb::aql
