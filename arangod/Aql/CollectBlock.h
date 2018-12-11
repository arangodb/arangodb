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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_COLLECT_BLOCK_H
#define ARANGOD_AQL_COLLECT_BLOCK_H 1

#include "Basics/Common.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/AqlValue.h"
#include "Aql/AqlValueGroup.h"
#include "Aql/CollectNode.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionNode.h"
#include "Basics/Result.h"

#include <velocypack/Builder.h>

namespace arangodb {
namespace transaction {
class Methods;
}

namespace aql {
struct Aggregator;
class ExecutionEngine;
  
typedef std::vector<std::unique_ptr<Aggregator>> AggregateValuesType;

class SortedCollectBlock final : public ExecutionBlock {
 private:
  struct CollectGroup {
    std::vector<AqlValue> groupValues;

    std::vector<AqlItemBlock*> groupBlocks;
    AggregateValuesType aggregators;
    size_t firstRow;
    size_t lastRow;
    size_t groupLength;
    bool rowsAreValid;
    bool const count;

    // is true iff at least one row belongs to the current group (the values
    // aren't necessarily added yet)
    bool hasRows;

    CollectGroup() = delete;
    CollectGroup(CollectGroup const&) = delete;
    CollectGroup& operator=(CollectGroup const&) = delete;

    explicit CollectGroup(bool count);
    ~CollectGroup();

    void initialize(size_t capacity);
    void reset();

    void setFirstRow(size_t value) {
      firstRow = value;
      rowsAreValid = true;
      hasRows = true;
    }

    void setLastRow(size_t value) {
      lastRow = value;
      rowsAreValid = true;
      hasRows = true;
    }

    void addValues(AqlItemBlock const* src, RegisterId groupRegister);
  };

 public:
  SortedCollectBlock(ExecutionEngine*, CollectNode const*);

  /// @brief initializeCursor
  std::pair<ExecutionState, Result> initializeCursor(AqlItemBlock* items, size_t pos) override;

 private:
  std::pair<ExecutionState, Result> getOrSkipSome(size_t atMost, bool skipping,
                                                  AqlItemBlock*& result,
                                                  size_t& skipped) override;

  /// @brief writes the current group data into the result
  void emitGroup(AqlItemBlock const* cur, AqlItemBlock* res, size_t row, bool skipping);
  
  /// @brief skips the current group
  void skipGroup();

 private:
  /// @brief pairs, consisting of out register and in register
  std::vector<std::pair<RegisterId, RegisterId>> _groupRegisters;

  /// @brief pairs, consisting of out register and in register
  std::vector<std::pair<RegisterId, RegisterId>> _aggregateRegisters;

  /// @brief details about the current group
  CollectGroup _currentGroup;

  /// @brief the last input block. Only set in the iteration immediately after
  // its last row was processed. Set to nullptr otherwise.
  AqlItemBlock* _lastBlock;

  /// @brief result built during getOrSkipSome
  std::unique_ptr<AqlItemBlock> _result;

  /// @brief the optional register that contains the input expression values for
  /// each group
  RegisterId _expressionRegister;

  /// @brief the optional register that contains the values for each group
  /// if no values should be returned, then this has a value of MaxRegisterId
  /// this register is also used for counting in case WITH COUNT INTO var is
  /// used
  RegisterId _collectRegister;

  /// @brief list of variables names for the registers
  std::vector<std::string> _variableNames;
};

class HashedCollectBlock final : public ExecutionBlock {
 public:
  HashedCollectBlock(ExecutionEngine*, CollectNode const*);
  ~HashedCollectBlock() final;

  std::pair<ExecutionState, Result> initializeCursor(AqlItemBlock* items,
                                                     size_t pos) override;

 private:
  std::pair<ExecutionState, Result> getOrSkipSome(size_t atMost, bool skipping,
                                                  AqlItemBlock*& result,
                                                  size_t& skipped) override;

 private:
  /// @brief pairs, consisting of out register and in register
  std::vector<std::pair<RegisterId, RegisterId>> _groupRegisters;

  /// @brief pairs, consisting of out register and in register
  std::vector<std::pair<RegisterId, RegisterId>> _aggregateRegisters;

  /// @brief the optional register that contains the values for each group
  /// if no values should be returned, then this has a value of MaxRegisterId
  /// this register is also used for counting in case WITH COUNT INTO var is
  /// used
  RegisterId _collectRegister;

  /// @brief the last input block
  AqlItemBlock* _lastBlock;

  /// @brief hashmap of all encountered groups
  std::unordered_map<std::vector<AqlValue>,
                     std::unique_ptr<AggregateValuesType>, AqlValueGroupHash,
                     AqlValueGroupEqual>
      _allGroups;

  void _destroyAllGroupsAqlValues();
};

class DistinctCollectBlock final : public ExecutionBlock {
 public:
  DistinctCollectBlock(ExecutionEngine*, CollectNode const*);
  ~DistinctCollectBlock();

  /// @brief initializeCursor
  std::pair<ExecutionState, Result> initializeCursor(AqlItemBlock* items, size_t pos) override;

 private:
  std::pair<ExecutionState, Result> getOrSkipSome(size_t atMost, bool skipping,
                                                  AqlItemBlock*& result,
                                                  size_t& skipped) override;

  void clearValues();

 private:
  /// @brief pairs, consisting of out register and in register
  std::vector<std::pair<RegisterId, RegisterId>> _groupRegisters;
  
  std::unique_ptr<std::unordered_set<std::vector<AqlValue>, AqlValueGroupHash, AqlValueGroupEqual>> _seen;
  std::unique_ptr<AqlItemBlock> _res;
};

class CountCollectBlock final : public ExecutionBlock {
 public:
  CountCollectBlock(ExecutionEngine*, CollectNode const*);

  std::pair<ExecutionState, Result> initializeCursor(AqlItemBlock* items, size_t pos) override;
  
  std::pair<ExecutionState, Result> getOrSkipSome(size_t atMost, bool skipping,
                                                  AqlItemBlock*& result, size_t& skipped) override;

 private:
  RegisterId _collectRegister;

  size_t _count;
};

}  // namespace arangodb::aql
}  // namespace arangodb

#endif
