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
#include "Aql/AqlValue.h"
#include "Aql/CollectNode.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionNode.h"

#include <velocypack/Builder.h>

namespace arangodb {
namespace transaction {
class Methods;
}

namespace aql {
struct Aggregator;
class AqlItemBlock;
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
    }

    void setLastRow(size_t value) {
      lastRow = value;
      rowsAreValid = true;
    }

    void addValues(AqlItemBlock const* src, RegisterId groupRegister);
  };

 public:
  SortedCollectBlock(ExecutionEngine*, CollectNode const*);

  ~SortedCollectBlock();

  int initialize() override final;
  
  /// @brief initializeCursor
  int initializeCursor(AqlItemBlock* items, size_t pos) override;

 private:
  int getOrSkipSome(size_t atLeast, size_t atMost, bool skipping,
                    AqlItemBlock*& result, size_t& skipped) override;

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
  
  /// @brief builder for temporary aggregate values
  arangodb::velocypack::Builder _builder;
};

class HashedCollectBlock : public ExecutionBlock {
 public:
  HashedCollectBlock(ExecutionEngine*, CollectNode const*);
  ~HashedCollectBlock();

 private:
  int getOrSkipSome(size_t atLeast, size_t atMost, bool skipping,
                    AqlItemBlock*& result, size_t& skipped) override;

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

  /// @brief hasher for a vector of AQL values
  struct GroupKeyHash {
    GroupKeyHash(transaction::Methods* trx, size_t num)
        : _trx(trx), _num(num) {}

    size_t operator()(std::vector<AqlValue> const& value) const;

    transaction::Methods* _trx;
    size_t const _num;
  };

  /// @brief comparator for a vector of AQL values
  struct GroupKeyEqual {
    explicit GroupKeyEqual(transaction::Methods* trx)
        : _trx(trx) {}

    bool operator()(std::vector<AqlValue> const&,
                    std::vector<AqlValue> const&) const;

    transaction::Methods* _trx;
  };
};

}  // namespace arangodb::aql
}  // namespace arangodb

#endif
