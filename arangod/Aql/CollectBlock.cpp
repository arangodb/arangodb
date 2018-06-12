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

#include "CollectBlock.h"
#include "Aql/AqlValue.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Basics/Exceptions.h"
#include "Basics/VelocyPackHelper.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace {
static AqlValue EmptyValue;

/// @brief get the value from an input register
/// for a reduce function that does not require input, this will return a
/// reference to a static empty AqlValue
static inline AqlValue const& getValueForRegister(AqlItemBlock const* src,
                                                  size_t row, RegisterId reg) {
  if (reg == ExecutionNode::MaxRegisterId) {
    return EmptyValue;
  }
  return src->getValueReference(row, reg);
}
}

SortedCollectBlock::CollectGroup::CollectGroup(bool count)
    : firstRow(0),
      lastRow(0),
      groupLength(0),
      rowsAreValid(false),
      count(count) {}

SortedCollectBlock::CollectGroup::~CollectGroup() {
  for (auto& it : groupValues) {
    it.destroy();
  }
  for (auto& it : groupBlocks) {
    delete it;
  }
}

void SortedCollectBlock::CollectGroup::initialize(size_t capacity) {
  groupValues.clear();

  if (capacity > 0) {
    groupValues.reserve(capacity);

    for (size_t i = 0; i < capacity; ++i) {
      groupValues.emplace_back();
    }
  }

  groupLength = 0;

  // reset aggregators
  for (auto& it : aggregators) {
    it->reset();
  }
  
  rowsAreValid = false;
}

void SortedCollectBlock::CollectGroup::reset() {
  for (auto& it : groupBlocks) {
    delete it;
  }
  groupBlocks.clear();

  if (!groupValues.empty()) {
    for (auto& it : groupValues) {
      it.destroy();
    }
    groupValues[0].erase();  // only need to erase [0], because we have
                             // only copies of references anyway
  }

  groupLength = 0;

  // reset all aggregators
  for (auto& it : aggregators) {
    it->reset();
  }

  rowsAreValid = false;
}

void SortedCollectBlock::CollectGroup::addValues(AqlItemBlock const* src,
                                                 RegisterId groupRegister) {
  if (groupRegister == ExecutionNode::MaxRegisterId) {
    // nothing to do
    return;
  }

  if (rowsAreValid) {
    // copy group values
    TRI_ASSERT(firstRow <= lastRow);

    if (count) {
      groupLength += lastRow + 1 - firstRow;
    } else {
      TRI_ASSERT(src != nullptr);
      auto block = src->slice(firstRow, lastRow + 1);
      try {
        TRI_IF_FAILURE("CollectGroup::addValues") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        groupBlocks.emplace_back(block);
      } catch (...) {
        delete block;
        throw;
      }
    }
  }

  firstRow = lastRow = 0;
  // the next statement ensures we don't add the same value (row) twice
  rowsAreValid = false;
}

SortedCollectBlock::SortedCollectBlock(ExecutionEngine* engine,
                                       CollectNode const* en)
    : ExecutionBlock(engine, en),
      _groupRegisters(),
      _aggregateRegisters(),
      _currentGroup(en->_count),
      _expressionRegister(ExecutionNode::MaxRegisterId),
      _collectRegister(ExecutionNode::MaxRegisterId),
      _variableNames() {
  for (auto const& p : en->_groupVariables) {
    // We know that planRegisters() has been run, so
    // getPlanNode()->_registerPlan is set up
    auto itOut = en->getRegisterPlan()->varInfo.find(p.first->id);
    TRI_ASSERT(itOut != en->getRegisterPlan()->varInfo.end());

    auto itIn = en->getRegisterPlan()->varInfo.find(p.second->id);
    TRI_ASSERT(itIn != en->getRegisterPlan()->varInfo.end());
    TRI_ASSERT((*itIn).second.registerId < ExecutionNode::MaxRegisterId);
    TRI_ASSERT((*itOut).second.registerId < ExecutionNode::MaxRegisterId);
    _groupRegisters.emplace_back(
        std::make_pair((*itOut).second.registerId, (*itIn).second.registerId));
  }

  for (auto const& p : en->_aggregateVariables) {
    // We know that planRegisters() has been run, so
    // getPlanNode()->_registerPlan is set up
    auto itOut = en->getRegisterPlan()->varInfo.find(p.first->id);
    TRI_ASSERT(itOut != en->getRegisterPlan()->varInfo.end());
    TRI_ASSERT((*itOut).second.registerId < ExecutionNode::MaxRegisterId);

    RegisterId reg;
    if (Aggregator::requiresInput(p.second.second)) {
      auto itIn = en->getRegisterPlan()->varInfo.find(p.second.first->id);
      TRI_ASSERT(itIn != en->getRegisterPlan()->varInfo.end());
      TRI_ASSERT((*itIn).second.registerId < ExecutionNode::MaxRegisterId);
      reg = (*itIn).second.registerId;
    } else {
      // no input variable required
      reg = ExecutionNode::MaxRegisterId;
    }
    _aggregateRegisters.emplace_back(
        std::make_pair((*itOut).second.registerId, reg));
    _currentGroup.aggregators.emplace_back(Aggregator::fromTypeString(_trx, p.second.second));
  }
  TRI_ASSERT(_aggregateRegisters.size() == en->_aggregateVariables.size());
  TRI_ASSERT(_aggregateRegisters.size() == _currentGroup.aggregators.size());
    
  if (en->_outVariable != nullptr) {
    auto const& registerPlan = en->getRegisterPlan()->varInfo;
    auto it = registerPlan.find(en->_outVariable->id);
    TRI_ASSERT(it != registerPlan.end());
    _collectRegister = (*it).second.registerId;
    TRI_ASSERT(_collectRegister > 0 &&
               _collectRegister < ExecutionNode::MaxRegisterId);

    if (en->_expressionVariable != nullptr) {
      auto it = registerPlan.find(en->_expressionVariable->id);
      TRI_ASSERT(it != registerPlan.end());
      _expressionRegister = (*it).second.registerId;
    }

    // construct a mapping of all register ids to variable names
    // we need this mapping to generate the grouped output

    for (size_t i = 0; i < registerPlan.size(); ++i) {
      _variableNames.emplace_back("");  // initialize with default value
    }

    // iterate over all our variables
    if (en->_keepVariables.empty()) {
      auto usedVariableIds(en->getVariableIdsUsedHere());
       
      for (auto const& vi : registerPlan) {
        if (vi.second.depth > 0 || en->getDepth() == 1) {
          // Do not keep variables from depth 0, unless we are depth 1 ourselves
          // (which means no FOR in which we are contained)

          if (usedVariableIds.find(vi.first) == usedVariableIds.end()) {
            // variable is not visible to the CollectBlock
            continue;
          }

          // find variable in the global variable map
          auto itVar = en->_variableMap.find(vi.first);

          if (itVar != en->_variableMap.end()) {
            _variableNames[vi.second.registerId] = (*itVar).second;
          }
        }
      }
    } else {
      for (auto const& x : en->_keepVariables) {
        auto it = registerPlan.find(x->id);

        if (it != registerPlan.end()) {
          _variableNames[(*it).second.registerId] = x->name;
        }
      }
    }
  }
  
  // reserve space for the current row
  _currentGroup.initialize(_groupRegisters.size());
  _pos = 0;
}

std::pair<ExecutionState, arangodb::Result> SortedCollectBlock::initializeCursor(
    AqlItemBlock* items, size_t pos) {
  DEBUG_BEGIN_BLOCK();
  auto res = ExecutionBlock::initializeCursor(items, pos);

  if (res.first == ExecutionState::WAITING ||
      !res.second.ok()) {
    // If we need to wait or get an error we return as is.
    return res;
  }

  DEBUG_BEGIN_BLOCK();
  _currentGroup.reset();
  _pos = 0;
  DEBUG_END_BLOCK();

  return res;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

int SortedCollectBlock::getOrSkipSomeOld(size_t atMost,
                                      bool skipping, AqlItemBlock*& result,
                                      size_t& skipped) {
  TRI_ASSERT(result == nullptr && skipped == 0);

  if (_done) {
    return TRI_ERROR_NO_ERROR;
  }

  bool const isTotalAggregation = _groupRegisters.empty();
  std::unique_ptr<AqlItemBlock> res;

  if (_buffer.empty()) {
    if (!ExecutionBlock::getBlockOld(atMost)) {
      // done
      _done = true;

      if (isTotalAggregation && _currentGroup.groupLength == 0) {
        // total aggregation, but have not yet emitted a group
        res.reset(requestBlock(1, getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]));
        emitGroup(nullptr, res.get(), skipped, skipping);
        result = res.release();
      }

      return TRI_ERROR_NO_ERROR;
    }
    _pos = 0;  // this is in the first block
  }

  // If we get here, we do have _buffer.front()
  AqlItemBlock* cur = _buffer.front();
  TRI_ASSERT(cur != nullptr);
  
  if (!skipping) {
    res.reset(requestBlock(atMost, getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]));

    TRI_ASSERT(cur->getNrRegs() <= res->getNrRegs());
    inheritRegisters(cur, res.get(), _pos);
  }

  while (skipped < atMost) {
    // read the next input row
    TRI_IF_FAILURE("SortedCollectBlock::getOrSkipSomeOuter") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    throwIfKilled();  // check if we were aborted
    
    bool newGroup = false;
    if (!isTotalAggregation) {
      if (_currentGroup.groupValues[0].isEmpty()) {
        // we never had any previous group
        newGroup = true;
      } else {
        // we already had a group, check if the group has changed
        size_t i = 0;
        
        for (auto& it : _groupRegisters) {
          int cmp = AqlValue::Compare(
              _trx, _currentGroup.groupValues[i], 
              cur->getValueReference(_pos, it.second), false);

          if (cmp != 0) {
            // group change
            newGroup = true;
            break;
          }
          ++i;
        }
      }
    }

    if (newGroup) {
      if (!_currentGroup.groupValues[0].isEmpty()) {
        if (!skipping) {
          // need to emit the current group first
          TRI_ASSERT(cur != nullptr);
          emitGroup(cur, res.get(), skipped, skipping);
        } else {
          skipGroup();
        }

        // increase output row count
        ++skipped;

        if (skipped == atMost && !skipping) {
          // output is full
          // do NOT advance input pointer
          result = res.release();
          return TRI_ERROR_NO_ERROR;
        }
      }

      // still space left in the output to create a new group

      // construct the new group
      size_t i = 0;
      TRI_ASSERT(cur != nullptr);
      for (auto& it : _groupRegisters) {
        _currentGroup.groupValues[i] = cur->getValueReference(_pos, it.second).clone();
        ++i;
      }
      _currentGroup.setFirstRow(_pos);
    }

    _currentGroup.setLastRow(_pos);

    if (++_pos >= cur->size()) {
      _buffer.pop_front();
      _pos = 0;

      bool hasMore = !_buffer.empty();

      if (!hasMore) {
        try {
          TRI_IF_FAILURE("SortedCollectBlock::hasMore") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }
          hasMore = ExecutionBlock::getBlockOld(atMost);
        } catch (...) {
          // prevent leak
          returnBlock(cur);
          throw;
        }
      }

      if (!hasMore) {
        // no more input. we're done
        try {
          // emit last buffered group
          if (!skipping) {
            TRI_IF_FAILURE("SortedCollectBlock::getOrSkipSome") {
              THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
            }

            throwIfKilled();

            TRI_ASSERT(cur != nullptr);
            emitGroup(cur, res.get(), skipped, skipping);
            ++skipped;
            res->shrink(skipped);
          } else {
            ++skipped;
          }
          returnBlock(cur);
          _done = true;
          result = res.release();
          return TRI_ERROR_NO_ERROR;
        } catch (...) {
          returnBlock(cur);
          throw;
        }
      }

      // hasMore

      if (_currentGroup.rowsAreValid) {
        size_t j = 0;
        for (auto& it : _currentGroup.aggregators) {
          RegisterId const reg = _aggregateRegisters[j].second;
          for (size_t r = _currentGroup.firstRow; r < _currentGroup.lastRow + 1;
               ++r) {
            it->reduce(getValueForRegister(cur, r, reg));
          }
          ++j;
        }
      }

      // move over the last group details into the group before we delete the
      // block
      _currentGroup.addValues(cur, _collectRegister);

      returnBlock(cur);
      cur = _buffer.front();
      _currentGroup.firstRow = 0; 
      _currentGroup.lastRow = 0; 
    }
  }

  if (!skipping) {
    TRI_ASSERT(skipped > 0);
    res->shrink(skipped);
  }

  result = res.release();
  return TRI_ERROR_NO_ERROR;
}

/// @brief writes the current group data into the result
void SortedCollectBlock::emitGroup(AqlItemBlock const* cur, AqlItemBlock* res,
                                   size_t row, bool skipping) {
  TRI_ASSERT(res != nullptr);

  if (row > 0 && !skipping) {
    // re-use already copied AqlValues
    TRI_ASSERT(cur != nullptr);
    for (RegisterId i = 0; i < cur->getNrRegs(); i++) {
      res->emplaceValue(row, i, res->getValueReference(0, i));
      // Note: if this throws, then all values will be deleted
      // properly since the first one is.
    }
  }

  size_t i = 0;
  for (auto& it : _groupRegisters) {
    if (!skipping) {
      res->setValue(row, it.first, _currentGroup.groupValues[i]);
    } else {
      _currentGroup.groupValues[i].destroy();
    }
    // ownership of value is transferred into res
    _currentGroup.groupValues[i].erase();
    ++i;
  }

  // handle aggregators
  if (!skipping) {
    size_t j = 0;
    for (auto& it : _currentGroup.aggregators) {
      if (_currentGroup.rowsAreValid) {
        TRI_ASSERT(cur != nullptr);
        RegisterId const reg = _aggregateRegisters[j].second;
        for (size_t r = _currentGroup.firstRow; r < _currentGroup.lastRow + 1;
            ++r) {
          it->reduce(getValueForRegister(cur, r, reg));
        }
        res->setValue(row, _aggregateRegisters[j].first, it->stealValue());
      } else {
        res->emplaceValue(
            row, _aggregateRegisters[j].first, arangodb::basics::VelocyPackHelper::NullValue());
      }
      ++j;
    }

    // set the group values
    if (_collectRegister != ExecutionNode::MaxRegisterId) {
      _currentGroup.addValues(cur, _collectRegister);

      if (ExecutionNode::castTo<CollectNode const*>(_exeNode)->_count) {
        // only set group count in result register
        res->emplaceValue(row, _collectRegister, AqlValueHintUInt(static_cast<uint64_t>(_currentGroup.groupLength)));
      } else if (ExecutionNode::castTo<CollectNode const*>(_exeNode)->_expressionVariable !=
                nullptr) {
        // copy expression result into result register
        res->setValue(row, _collectRegister,
                      AqlValue::CreateFromBlocks(_trx, _currentGroup.groupBlocks,
                                                _expressionRegister));
      } else {
        // copy variables / keep variables into result register
        res->setValue(row, _collectRegister,
                      AqlValue::CreateFromBlocks(_trx, _currentGroup.groupBlocks,
                                                _variableNames));
      }
    }
  }

  // reset the group so a new one can start
  _currentGroup.reset();
}

/// @brief skips the current group
void SortedCollectBlock::skipGroup() {
  // reset the group so a new one can start
  _currentGroup.reset();
}

HashedCollectBlock::HashedCollectBlock(ExecutionEngine* engine,
                                       CollectNode const* en)
    : ExecutionBlock(engine, en),
      _groupRegisters(),
      _aggregateRegisters(),
      _collectRegister(ExecutionNode::MaxRegisterId),
      _allGroups(1024, AqlValueGroupHash(_trx, en->_groupVariables.size()),
                 AqlValueGroupEqual(_trx)) {
  for (auto const& p : en->_groupVariables) {
    // We know that planRegisters() has been run, so
    // getPlanNode()->_registerPlan is set up
    auto itOut = en->getRegisterPlan()->varInfo.find(p.first->id);
    TRI_ASSERT(itOut != en->getRegisterPlan()->varInfo.end());

    auto itIn = en->getRegisterPlan()->varInfo.find(p.second->id);
    TRI_ASSERT(itIn != en->getRegisterPlan()->varInfo.end());
    TRI_ASSERT((*itIn).second.registerId < ExecutionNode::MaxRegisterId);
    TRI_ASSERT((*itOut).second.registerId < ExecutionNode::MaxRegisterId);
    _groupRegisters.emplace_back(
        std::make_pair((*itOut).second.registerId, (*itIn).second.registerId));
  }

  for (auto const& p : en->_aggregateVariables) {
    // We know that planRegisters() has been run, so
    // getPlanNode()->_registerPlan is set up
    auto itOut = en->getRegisterPlan()->varInfo.find(p.first->id);
    TRI_ASSERT(itOut != en->getRegisterPlan()->varInfo.end());
    TRI_ASSERT((*itOut).second.registerId < ExecutionNode::MaxRegisterId);

    RegisterId reg;
    if (Aggregator::requiresInput(p.second.second)) {
      auto itIn = en->getRegisterPlan()->varInfo.find(p.second.first->id);
      TRI_ASSERT(itIn != en->getRegisterPlan()->varInfo.end());
      TRI_ASSERT((*itIn).second.registerId < ExecutionNode::MaxRegisterId);
      reg = (*itIn).second.registerId;
    } else {
      // no input variable required
      reg = ExecutionNode::MaxRegisterId;
    }
    _aggregateRegisters.emplace_back(
        std::make_pair((*itOut).second.registerId, reg));
  }
  TRI_ASSERT(_aggregateRegisters.size() == en->_aggregateVariables.size());

  if (en->_outVariable != nullptr) {
    TRI_ASSERT(ExecutionNode::castTo<CollectNode const*>(_exeNode)->_count);

    auto const& registerPlan = en->getRegisterPlan()->varInfo;
    auto it = registerPlan.find(en->_outVariable->id);
    TRI_ASSERT(it != registerPlan.end());
    _collectRegister = (*it).second.registerId;
    TRI_ASSERT(_collectRegister > 0 &&
               _collectRegister < ExecutionNode::MaxRegisterId);
  } else {
    TRI_ASSERT(!ExecutionNode::castTo<CollectNode const*>(_exeNode)->_count);
  }

  TRI_ASSERT(!_groupRegisters.empty());
}

std::pair<ExecutionState, Result> HashedCollectBlock::getOrSkipSome(
    size_t const atMost, bool const skipping, AqlItemBlock*& result,
    size_t& skipped_) {
  TRI_ASSERT(result == nullptr && skipped_ == 0);

  if (atMost == 0) {
    return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
  }

  if (_done) {
    return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
  }

  enum class GetNextRowState { NONE, SUCCESS, WAITING };

  auto previousNode = getPlanNode()->getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);
  RegisterId const curNrRegs =
      previousNode->getRegisterPlan()->nrRegs[previousNode->getDepth()];

  // state variables of the following generator-lambda, iterating over rows
  AqlItemBlock* nextCur = nullptr;
  // size_t nextPos = 0;
  auto getNextRow =
      [this, &nextCur,
       &curNrRegs]() -> std::tuple<GetNextRowState, AqlItemBlock*, size_t> {

    // TODO nextcur is either nullptr or _buffer.front() - seems somewhat
    // superfluous

    // try to ensure a nonempty buffer
    if (nextCur == nullptr && _buffer.empty()) {
      ExecutionBlock::getBlockOld(DefaultBatchSize());
      _pos = 0;
      // TODO use getBlock(), return WAITING if necessary
    }

    // check if we're done
    if (_buffer.empty()) {
      return std::make_tuple(GetNextRowState::NONE, nullptr, 0);
    }

    nextCur = _buffer.front();

    // save current position (to return)
    AqlItemBlock* cur = nextCur;
    size_t pos = _pos;

    TRI_ASSERT(curNrRegs == cur->getNrRegs());

    // calculate next position
    ++_pos;
    if (_pos >= nextCur->size()) {
      nextCur = nullptr;
      _pos = 0;
      _buffer.pop_front();
    }

    return std::make_tuple(GetNextRowState::SUCCESS, cur, pos);
  };

  auto* en = ExecutionNode::castTo<CollectNode const*>(_exeNode);

  auto buildNewGroup = [this, en](
      const AqlItemBlock* cur, size_t const pos,
      const size_t n) -> std::pair<std::unique_ptr<AggregateValuesType>,
                                   std::vector<AqlValue>> {
    std::vector<AqlValue> group;
    group.reserve(n);

    // copy the group values before they get invalidated
    for (size_t i = 0; i < n; ++i) {
      group.emplace_back(
          cur->getValueReference(pos, _groupRegisters[i].second).clone());
    }

    auto aggregateValues = std::make_unique<AggregateValuesType>();

    if (en->_aggregateVariables.empty()) {
      // no aggregate registers. this means we'll only count the number of items
      if (en->_count) {
        aggregateValues->emplace_back(
            std::make_unique<AggregatorLength>(_trx, 0));
      }
    } else {
      // we do have aggregate registers. create them as empty AqlValues
      aggregateValues->reserve(_aggregateRegisters.size());

      // initialize aggregators
      for (auto const& r : en->_aggregateVariables) {
        aggregateValues->emplace_back(
            Aggregator::fromTypeString(_trx, r.second.second));
      }
    }

    return std::make_pair(std::move(aggregateValues), group);
  };

  auto findOrEmplaceGroup = [this, &buildNewGroup](
      AqlItemBlock const* cur,
      size_t const pos) -> decltype(_allGroups)::iterator {
    std::vector<AqlValue> groupValues;
    size_t const n = _groupRegisters.size();
    groupValues.reserve(n);

    // for hashing simply re-use the aggregate registers, without cloning
    // their contents
    for (size_t i = 0; i < n; ++i) {
      groupValues.emplace_back(
          cur->getValueReference(pos, _groupRegisters[i].second));
    }

    auto it = _allGroups.find(groupValues);

    if (it != _allGroups.end()) {
      // group already exists
      return it;
    }

    // must create new group
    std::unique_ptr<AggregateValuesType> aggregateValues;
    std::vector<AqlValue> group;
    std::tie(aggregateValues, group) = buildNewGroup(cur, pos, n);

    // note: aggregateValues may be a nullptr!
    auto emplaceResult = _allGroups.emplace(group, std::move(aggregateValues));
    // emplace must not fail
    TRI_ASSERT(emplaceResult.second);

    return emplaceResult.first;
  };

  auto buildResult = [this, en,
                      &curNrRegs](AqlItemBlock const* src) -> AqlItemBlock* {
    RegisterId nrRegs = en->getRegisterPlan()->nrRegs[en->getDepth()];

    std::unique_ptr<AqlItemBlock> result(
        requestBlock(_allGroups.size(), nrRegs));

    if (src != nullptr) {
      inheritRegisters(src, result.get(), 0);
    }

    TRI_ASSERT(!en->_count || _collectRegister != ExecutionNode::MaxRegisterId);

    size_t row = 0;
    for (auto& it : _allGroups) {
      auto& keys = it.first;
      TRI_ASSERT(it.second != nullptr);

      TRI_ASSERT(keys.size() == _groupRegisters.size());
      size_t i = 0;
      for (auto& key : keys) {
        result->setValue(row, _groupRegisters[i++].first, key);
        const_cast<AqlValue*>(&key)
            ->erase();  // to prevent double-freeing later
      }

      if (!en->_count) {
        TRI_ASSERT(it.second->size() == _aggregateRegisters.size());
        size_t j = 0;
        for (auto const& r : *(it.second)) {
          result->setValue(row, _aggregateRegisters[j++].first,
                           r->stealValue());
        }
      } else {
        // set group count in result register
        TRI_ASSERT(!it.second->empty());
        result->setValue(row, _collectRegister,
                         it.second->back()->stealValue());
      }

      if (row > 0) {
        // re-use already copied AQLValues for remaining registers
        result->copyValuesFromFirstRow(row, static_cast<RegisterId>(curNrRegs));
      }

      ++row;
    }

    return result.release();
  };
  auto reduceAggregates = [this, en](decltype(_allGroups)::iterator groupIt,
                                     AqlItemBlock const* cur,
                                     size_t const pos) {
    AggregateValuesType* aggregateValues = groupIt->second.get();

    if (en->_aggregateVariables.empty()) {
      // no aggregate registers. simply increase the counter
      if (en->_count) {
        // TODO maybe get rid of this special case
        TRI_ASSERT(!aggregateValues->empty());
        aggregateValues->back()->reduce(AqlValue());
      }
    } else {
      // apply the aggregators for the group
      TRI_ASSERT(aggregateValues->size() == _aggregateRegisters.size());
      size_t j = 0;
      for (auto const& r : _aggregateRegisters) {
        (*aggregateValues)[j]->reduce(getValueForRegister(cur, pos, r.second));
        ++j;
      }
    }
  };

  // TODO return blocks

  // For buildResult (respectively inheritRegisters). Is this really necessary?
  AqlItemBlock* lastBlock = nullptr;

  while (true) {
    TRI_IF_FAILURE("HashedCollectBlock::getOrSkipSomeOuter") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    GetNextRowState state;
    AqlItemBlock* cur = nullptr;
    size_t pos = 0;
    std::tie(state, cur, pos) = getNextRow();

    throwIfKilled();

    if (state == GetNextRowState::NONE) {
      // no more rows
      break;
    } else if (state == GetNextRowState::WAITING) {
      // continue later
      return {ExecutionState::WAITING, TRI_ERROR_NO_ERROR};
    }

    TRI_IF_FAILURE("HashedCollectBlock::getOrSkipSome") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    lastBlock = cur;
    TRI_ASSERT(state == GetNextRowState::SUCCESS);
    ++skipped_;

    // NOLINTNEXTLINE(hicpp-use-auto,modernize-use-auto)
    decltype(_allGroups)::iterator currentGroupIt =
        findOrEmplaceGroup(cur, pos);

    reduceAggregates(currentGroupIt, cur, pos);
  }

  if (skipped_ > 0) {
    TRI_ASSERT(lastBlock != nullptr);
    result = buildResult(lastBlock);
  }

  _done = true;

  return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
}

HashedCollectBlock::~HashedCollectBlock() {
  // Generally, _allGroups should be empty when the block is destroyed - except
  // when an exception is thrown during getOrSkipSome, in which case the
  // AqlValue ownership hasn't been transferred.
  _destroyAllGroupsAqlValues();
}

void HashedCollectBlock::_destroyAllGroupsAqlValues() {
  for (auto& it : _allGroups) {
    for (auto& it2 : it.first) {
      const_cast<AqlValue*>(&it2)->destroy();
    }
  }
}

DistinctCollectBlock::DistinctCollectBlock(ExecutionEngine* engine,
                                           CollectNode const* en)
    : ExecutionBlock(engine, en),
      _groupRegisters(),
      _res(nullptr),
      _skipped(0) {
  for (auto const& p : en->_groupVariables) {
    // We know that planRegisters() has been run, so
    // getPlanNode()->_registerPlan is set up
    auto itOut = en->getRegisterPlan()->varInfo.find(p.first->id);
    TRI_ASSERT(itOut != en->getRegisterPlan()->varInfo.end());

    auto itIn = en->getRegisterPlan()->varInfo.find(p.second->id);
    TRI_ASSERT(itIn != en->getRegisterPlan()->varInfo.end());
    TRI_ASSERT((*itIn).second.registerId < ExecutionNode::MaxRegisterId);
    TRI_ASSERT((*itOut).second.registerId < ExecutionNode::MaxRegisterId);
    _groupRegisters.emplace_back(
        std::make_pair((*itOut).second.registerId, (*itIn).second.registerId));
  }

  TRI_ASSERT(!_groupRegisters.empty());
      
  _seen.reset(new std::unordered_set<std::vector<AqlValue>, AqlValueGroupHash, AqlValueGroupEqual>(
      1024, AqlValueGroupHash(transaction(), _groupRegisters.size()), AqlValueGroupEqual(transaction())));
}

DistinctCollectBlock::~DistinctCollectBlock() {
  clearValues();
}

std::pair<ExecutionState, arangodb::Result> DistinctCollectBlock::initializeCursor(
    AqlItemBlock* items, size_t pos) {
  DEBUG_BEGIN_BLOCK();
  auto res = ExecutionBlock::initializeCursor(items, pos);

  if (res.first == ExecutionState::WAITING ||
      !res.second.ok()) {
    // If we need to wait or get an error we return as is.
    return res;
  }

  DEBUG_BEGIN_BLOCK();
  _pos = 0;
  clearValues();
  DEBUG_END_BLOCK();

  return res;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

void DistinctCollectBlock::clearValues() {
  if (_seen) {
    for (auto& it : *_seen) {
      for (auto& it2 : it) {
        const_cast<AqlValue*>(&it2)->destroy();
      }
    }
    _seen->clear(); 
  }
}

std::pair<ExecutionState, Result> DistinctCollectBlock::getOrSkipSome(
    size_t const atMost, bool const skipping, AqlItemBlock*& result,
    size_t& skipped_) {
  TRI_ASSERT(result == nullptr && skipped_ == 0);

  auto const assignReturnValues = [this, skipping, &result, &skipped_]() {
    // set &skipped_ and &result; reset _skipped and _res.

    if (!skipping && _skipped > 0) {
      TRI_ASSERT(_res != nullptr);
      _res->shrink(_skipped);
    }

    result = _res.release();
    skipped_ = _skipped;
    _skipped = 0;
  };

  if (_done) {
    return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
  }

  std::vector<AqlValue> groupValues;
  groupValues.reserve(_groupRegisters.size());

  if (_buffer.empty()) {
    ExecutionState state;
    bool blockAppended;
    std::tie(state, blockAppended) = ExecutionBlock::getBlock(atMost);
    if (state == ExecutionState::WAITING) {
      TRI_ASSERT(!blockAppended);
      return {ExecutionState::WAITING, TRI_ERROR_NO_ERROR};
    }
    if (!blockAppended) {
      // done
      _done = true;
      assignReturnValues();
      return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
    }
    _pos = 0;  // this is in the first block
  }

  // If we get here, we do have _buffer.front()
  AqlItemBlock* cur = _buffer.front();
  TRI_ASSERT(cur != nullptr);

  if (!skipping && _res == nullptr) {
    TRI_ASSERT(_skipped == 0);
    _res.reset(requestBlock(
        atMost,
        getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]));

    TRI_ASSERT(cur->getNrRegs() <= _res->getNrRegs());
    inheritRegisters(cur, _res.get(), _pos);
  }

  while (_skipped < atMost) {
    // read the next input row
    TRI_IF_FAILURE("DistinctCollectBlock::getOrSkipSomeOuter") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    throwIfKilled();  // check if we were aborted
   
    groupValues.clear(); 
    // for hashing simply re-use the aggregate registers, without cloning
    // their contents
    for (auto &it : _groupRegisters) {
      groupValues.emplace_back(
          cur->getValueReference(_pos, it.second));
    }

    // now check if we already know this group
    auto foundIt = _seen->find(groupValues);

    if (foundIt == _seen->end()) {
      if (!skipping) {
        size_t i = 0;
        for (auto& it : _groupRegisters) {
          _res->setValue(_skipped, it.first, groupValues[i].clone());
          ++i;
        }
      }
      // transfer ownership 
      std::vector<AqlValue> copy;
      copy.reserve(groupValues.size());
      for (auto const& it : groupValues) {
        copy.emplace_back(it.clone());
      }
      _seen->emplace(std::move(copy));
      ++_skipped;
    }

    if (++_pos >= cur->size()) {
      _buffer.pop_front();
      _pos = 0;

      bool hasMore = !_buffer.empty();

      if (!hasMore) {
        try {
          TRI_IF_FAILURE("DistinctCollectBlock::hasMore") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }
          ExecutionState state;
          bool blockAppended;
          std::tie(state, blockAppended) = ExecutionBlock::getBlock(atMost);
          if (state == ExecutionState::WAITING) {
            TRI_ASSERT(!blockAppended);
            return {ExecutionState::WAITING, TRI_ERROR_NO_ERROR};
          }
          hasMore = blockAppended;

        } catch (...) {
          // prevent leak
          returnBlock(cur);
          throw;
        }
      }

      if (!hasMore) {
        // no more input. we're done
        try {
          // emit last buffered group
          if (!skipping) {
            TRI_IF_FAILURE("DistinctCollectBlock::getOrSkipSome") {
              THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
            }
            TRI_ASSERT(cur != nullptr);
          }
          returnBlock(cur);
          _done = true;

          assignReturnValues();
          return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
        } catch (...) {
          returnBlock(cur);
          throw;
        }
      }

      // hasMore
      returnBlock(cur);
      cur = _buffer.front();
    }
  }

  if (!skipping) {
    TRI_ASSERT(_skipped > 0);
  }

  assignReturnValues();
  return {ExecutionState::HASMORE, TRI_ERROR_NO_ERROR};
}

CountCollectBlock::CountCollectBlock(ExecutionEngine* engine,
                                     CollectNode const* en)
    : ExecutionBlock(engine, en),
      _collectRegister(ExecutionNode::MaxRegisterId),
      _count(0) {
  TRI_ASSERT(en->_groupVariables.empty());
  TRI_ASSERT(en->_count);
  TRI_ASSERT(en->_outVariable != nullptr);
   
  auto const& registerPlan = en->getRegisterPlan()->varInfo;
  auto it = registerPlan.find(en->_outVariable->id);
  TRI_ASSERT(it != registerPlan.end());
  _collectRegister = (*it).second.registerId;
  TRI_ASSERT(_collectRegister > 0 &&
             _collectRegister < ExecutionNode::MaxRegisterId);
}

std::pair<ExecutionState, arangodb::Result> CountCollectBlock::initializeCursor(
    AqlItemBlock* items, size_t pos) {
  auto res = ExecutionBlock::initializeCursor(items, pos);

  if (res.first == ExecutionState::WAITING ||
      !res.second.ok()) {
    // If we need to wait or get an error we return as is.
    return res;
  }

  _count = 0;
  return res;
}

std::pair<ExecutionState, Result> CountCollectBlock::getOrSkipSome(size_t atMost, bool skipping,
                                                                   AqlItemBlock*& result, size_t& skipped) {
  DEBUG_BEGIN_BLOCK();
  traceGetSomeBegin(atMost);

  TRI_ASSERT(result == nullptr && skipped == 0);
  
  if (_done) {
    traceGetSomeEnd(nullptr, ExecutionState::DONE);
    return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
  }
  
  while (!_done) {  
    if (_buffer.empty()) {
      auto upstreamRes = ExecutionBlock::getBlock(DefaultBatchSize());
      if (upstreamRes.first == ExecutionState::WAITING) {
        return {ExecutionState::WAITING, TRI_ERROR_NO_ERROR};
      }
      if (upstreamRes.first == ExecutionState::DONE ||
          !upstreamRes.second) {
        _done = true;
      }
    }
  
    if (!_buffer.empty()) {
      AqlItemBlock* cur = _buffer.front();
      TRI_ASSERT(cur != nullptr);
      _count += cur->size();
       
      // we are only aggregating data here, so we can immediately get rid of
      // everything that we see
      _buffer.pop_front();
      _pos = 0;
      returnBlock(cur);
    }
  
    throwIfKilled();  // check if we were aborted
  }

  TRI_ASSERT(_done);

  std::unique_ptr<AqlItemBlock> res;
 
  if (skipping) {
    skipped = 1;
  } else {
    res.reset(requestBlock(1, getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]));
    res->emplaceValue(0, _collectRegister, AqlValueHintUInt(static_cast<uint64_t>(_count)));
  }
   
  traceGetSomeEnd(res.get(), ExecutionState::DONE);
  result = res.release();

  return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}
