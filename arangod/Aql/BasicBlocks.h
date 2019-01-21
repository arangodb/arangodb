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

#include "Aql/AqlItemBlock.h"
#include "Aql/BlockCollector.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionNode.h"

namespace arangodb {
namespace aql {

class AqlItemBlock;

class ExecutionEngine;

class SingletonBlock final : public ExecutionBlock {
 public:
  SingletonBlock(ExecutionEngine* engine, SingletonNode const* ep);

  /// @brief initializeCursor, store a copy of the register values coming from
  /// above
  std::pair<ExecutionState, Result> initializeCursor(AqlItemBlock* items, size_t pos) override;

 private:
  std::pair<ExecutionState, arangodb::Result> getOrSkipSome(size_t atMost, bool skipping,
                                                            AqlItemBlock*& result,
                                                            size_t& skipped) override;

  /// @brief _inputRegisterValues
  std::unique_ptr<AqlItemBlock> _inputRegisterValues;

  std::unordered_set<RegisterId> _whitelist;
};

class LimitBlock final : public ExecutionBlock {
 private:
  enum class State { INITFULLCOUNT, SKIPPING, RETURNING, DONE };

 public:
  LimitBlock(ExecutionEngine* engine, LimitNode const* ep)
      : ExecutionBlock(engine, ep),
        _offset(ep->_offset),
        _limit(ep->_limit),
        _remainingOffset(ep->_limit),
        _count(0),
        _state(State::INITFULLCOUNT),  // start in the beginning
        _fullCount(ep->_fullCount),
        _limitSkipped(0),
        _result(nullptr) {}

  std::pair<ExecutionState, Result> initializeCursor(AqlItemBlock* items,
                                                     size_t pos) final override;

  std::pair<ExecutionState, Result> getOrSkipSome(size_t atMost, bool skipping,
                                                  AqlItemBlock*& result_,
                                                  size_t& skipped) override;

 protected:
  ExecutionState getHasMoreState() override;

 private:
  /// @brief _offset
  size_t const _offset;

  /// @brief _limit
  size_t const _limit;

  /// @brief remaining number of documents to skip. initialized to _offset
  size_t _remainingOffset;

  /// @brief _count, number of items already handed on
  size_t _count;

  /// @brief _state, 0 is beginning, 1 is after offset, 2 is done
  State _state;

  /// @brief whether or not the block should count what it limits
  bool const _fullCount;

  /// @brief skipped count. We cannot use _skipped here, has it would interfere
  /// with calling ExecutionBlock::getOrSkipSome in this getOrSkipSome
  /// implementation.
  size_t _limitSkipped;

  /// @brief result to return in getOrSkipSome
  std::unique_ptr<AqlItemBlock> _result;
};

}  // namespace aql
}  // namespace arangodb

#endif
