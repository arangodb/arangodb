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

#ifndef ARANGOD_AQL_BASIC_BLOCKS_H
#define ARANGOD_AQL_BASIC_BLOCKS_H 1

#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionNode.h"
#include "Utils/AqlTransaction.h"

namespace arangodb {
namespace aql {

class AqlItemBlock;

class ExecutionEngine;

class SingletonBlock : public ExecutionBlock {
 public:
  SingletonBlock(ExecutionEngine* engine, SingletonNode const* ep)
      : ExecutionBlock(engine, ep), _inputRegisterValues(nullptr), _whitelistBuilt(false) {}

  ~SingletonBlock() { deleteInputVariables(); }

  int initialize() override final {
    deleteInputVariables();
    return ExecutionBlock::initialize();
  }

  /// @brief initializeCursor, store a copy of the register values coming from
  /// above
  int initializeCursor(AqlItemBlock* items, size_t pos) override;

  int shutdown(int) override final;

  bool hasMore() override final { return !_done; }

  int64_t count() const override final { return 1; }

  int64_t remaining() override final { return _done ? 0 : 1; }

 private:
  void deleteInputVariables() {
    delete _inputRegisterValues;
    _inputRegisterValues = nullptr;
  }

  void buildWhitelist();

  int getOrSkipSome(size_t atLeast, size_t atMost, bool skipping,
                    AqlItemBlock*& result, size_t& skipped) override;

  /// @brief _inputRegisterValues
  AqlItemBlock* _inputRegisterValues;

  std::unordered_set<RegisterId> _whitelist;

  bool _whitelistBuilt;
};

class FilterBlock : public ExecutionBlock {
 public:
  FilterBlock(ExecutionEngine*, FilterNode const*);

  ~FilterBlock();

 private:
  /// @brief internal function to actually decide if the document should be used
  inline bool takeItem(AqlItemBlock* items, size_t index) const {
    return items->getValueReference(index, _inReg).toBoolean();
  }

  /// @brief internal function to get another block
  bool getBlock(size_t atLeast, size_t atMost);

  int getOrSkipSome(size_t atLeast, size_t atMost, bool skipping,
                    AqlItemBlock*& result, size_t& skipped) override;

  bool hasMore() override final;

  int64_t count() const override final {
    return -1;  // refuse to work
  }

  int64_t remaining() override final {
    return -1;  // refuse to work
  }

  /// @brief input register
 private:
  RegisterId _inReg;

  /// @brief vector of indices of those documents in the current block
  /// that are chosen
  std::vector<size_t> _chosen;
};

class LimitBlock : public ExecutionBlock {
 public:
  LimitBlock(ExecutionEngine* engine, LimitNode const* ep)
      : ExecutionBlock(engine, ep),
        _offset(ep->_offset),
        _limit(ep->_limit),
        _count(0),
        _state(0),  // start in the beginning
        _fullCount(ep->_fullCount) {}

  ~LimitBlock() {}

  int initializeCursor(AqlItemBlock* items, size_t pos) override final;

  virtual int getOrSkipSome(size_t atLeast, size_t atMost, bool skipping,
                            AqlItemBlock*& result, size_t& skipped) override;

  /// @brief _offset
  size_t _offset;

  /// @brief _limit
  size_t _limit;

  /// @brief _count, number of items already handed on
  size_t _count;

  /// @brief _state, 0 is beginning, 1 is after offset, 2 is done
  int _state;

  /// @brief whether or not the block should count what it limits
  bool const _fullCount;
};

class ReturnBlock : public ExecutionBlock {
 public:
  ReturnBlock(ExecutionEngine* engine, ReturnNode const* ep)
      : ExecutionBlock(engine, ep), _returnInheritedResults(false) {}

  ~ReturnBlock() {}

  /// @brief getSome
  AqlItemBlock* getSome(size_t atLeast, size_t atMost) override final;

  /// @brief make the return block return the results inherited from above,
  /// without creating new blocks
  /// returns the id of the register the final result can be found in
  RegisterId returnInheritedResults();

 private:
  /// @brief if set to true, the return block will return the AqlItemBlocks it
  /// gets from above directly. if set to false, the return block will create a
  /// new AqlItemBlock with one output register and copy the data from its input
  /// block into it
  bool _returnInheritedResults;
};

class NoResultsBlock : public ExecutionBlock {
 public:
  NoResultsBlock(ExecutionEngine* engine, NoResultsNode const* ep)
      : ExecutionBlock(engine, ep) {}

  ~NoResultsBlock() {}

  /// @brief initializeCursor, store a copy of the register values coming from
  /// above
  int initializeCursor(AqlItemBlock* items, size_t pos) override final;

  bool hasMore() override final { return false; }

  int64_t count() const override final { return 0; }

  int64_t remaining() override final { return 0; }

 private:
  int getOrSkipSome(size_t atLeast, size_t atMost, bool skipping,
                    AqlItemBlock*& result, size_t& skipped) override;
};

}  // namespace arangodb::aql
}  // namespace arangodb

#endif
