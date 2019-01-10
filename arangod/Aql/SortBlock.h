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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_SORT_BLOCK_H
#define ARANGOD_AQL_SORT_BLOCK_H 1

#include "Aql/ExecutionBlock.h"
#include "Aql/SortNode.h"
#include "Aql/SortRegister.h"
#include "Basics/Common.h"

namespace arangodb {
namespace aql {

class AqlItemBlock;
class ExecutionEngine;

class SortBlock final : public ExecutionBlock {
 public:
  class Sorter {
   public:
    using Fetcher = std::function<std::pair<ExecutionState, bool>(size_t)>;
    using Allocator = std::function<AqlItemBlock*(size_t, RegisterId)>;

   public:
    Sorter(arangodb::aql::SortBlock&, transaction::Methods*, std::deque<AqlItemBlock*>&,
           std::vector<SortRegister>&, Fetcher&&, Allocator&&);
    virtual ~Sorter();
    virtual std::pair<ExecutionState, arangodb::Result> sort() = 0;

   protected:
    SortBlock& _block;
    transaction::Methods* _trx;
    std::deque<AqlItemBlock*>& _buffer;
    std::vector<SortRegister>& _sortRegisters;
    Fetcher _fetch;
    Allocator _allocate;
  };
  enum SorterType { Standard, ConstrainedHeap };

 public:
  SortBlock(ExecutionEngine*, SortNode const*,
            SorterType type = SorterType::Standard, size_t limit = 0);

  ~SortBlock();

  /// @brief initializeCursor, could be called multiple times
  std::pair<ExecutionState, Result> initializeCursor(AqlItemBlock* items, size_t pos) override;

  std::pair<ExecutionState, Result> getOrSkipSome(size_t atMost, bool skipping,
                                                  AqlItemBlock*&,
                                                  size_t& skipped) override final;

  bool stable() const;

 private:
  void initializeSorter();

  /// @brief pairs, consisting of variable and sort direction
  /// (true = ascending | false = descending)
  std::vector<SortRegister> _sortRegisters;

  /// @brief whether or not the sort should be stable
  bool _stable;

  /// @brief the type of sorter to use
  SorterType _type;

  /// @brief the maximum number of items to return; unlimited if zero
  size_t _limit;

  /// @brief the object which actually handles the sorting
  std::unique_ptr<Sorter> _sorter = nullptr;
};

}  // namespace aql
}  // namespace arangodb

#endif
