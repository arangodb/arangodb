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

using namespace arangodb::aql;

static AqlValue EmptyValue;

/// @brief get the value from an input register
/// for a reduce function that does not require input, this will return a
/// reference to a static empty AqlValue
static inline AqlValue const& GetValueForRegister(AqlItemBlock const* src,
                                                   size_t row, RegisterId reg) {
  if (reg == ExecutionNode::MaxRegisterId) {
    return EmptyValue;
  }
  return src->getValueReference(row, reg);
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
}

SortedCollectBlock::~SortedCollectBlock() {}

/// @brief initialize
int SortedCollectBlock::initialize() {
  int res = ExecutionBlock::initialize();

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // reserve space for the current row
  _currentGroup.initialize(_groupRegisters.size());

  return TRI_ERROR_NO_ERROR;
}

int SortedCollectBlock::initializeCursor(AqlItemBlock* items,
                                         size_t pos) {
  DEBUG_BEGIN_BLOCK();
  int res = ExecutionBlock::initializeCursor(items, pos);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  DEBUG_BEGIN_BLOCK();
  _currentGroup.reset();
  _pos = 0;
  DEBUG_END_BLOCK();

  return TRI_ERROR_NO_ERROR;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

int SortedCollectBlock::getOrSkipSome(size_t atLeast, size_t atMost,
                                      bool skipping, AqlItemBlock*& result,
                                      size_t& skipped) {
  TRI_ASSERT(result == nullptr && skipped == 0);

  if (_done) {
    return TRI_ERROR_NO_ERROR;
  }

  bool const isTotalAggregation = _groupRegisters.empty();
  std::unique_ptr<AqlItemBlock> res;

  if (_buffer.empty()) {
    if (!ExecutionBlock::getBlock(atLeast, atMost)) {
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
              cur->getValue(_pos, it.second), false);

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
          hasMore = ExecutionBlock::getBlock(atLeast, atMost);
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
            res->shrink(skipped, false);
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
            it->reduce(GetValueForRegister(cur, r, reg));
          }
          ++j;
        }
      }

      // move over the last group details into the group before we delete the
      // block
      _currentGroup.addValues(cur, _collectRegister);

      returnBlock(cur);
      cur = _buffer.front();
    }
  }

  if (!skipping) {
    TRI_ASSERT(skipped > 0);
    res->shrink(skipped, false);
  }

  result = res.release();
  return TRI_ERROR_NO_ERROR;
}

/// @brief writes the current group data into the result
void SortedCollectBlock::emitGroup(AqlItemBlock const* cur, AqlItemBlock* res,
                                   size_t row, bool skipping) {
  if (row > 0 && !skipping) {
    // re-use already copied AqlValues
    TRI_ASSERT(cur != nullptr);
    for (RegisterId i = 0; i < cur->getNrRegs(); i++) {
      res->setValue(row, i, res->getValue(0, i));
      // Note: if this throws, then all values will be deleted
      // properly since the first one is.
    }
  }

  size_t i = 0;
  for (auto& it : _groupRegisters) {
    if (!skipping) {
      res->setValue(row, it.first, _currentGroup.groupValues[i]);
      // ownership of value is transferred into res
      _currentGroup.groupValues[i].erase();
    } else {
      _currentGroup.groupValues[i].destroy();
      _currentGroup.groupValues[i].erase();
    }
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
          it->reduce(GetValueForRegister(cur, r, reg));
        }
        res->setValue(row, _aggregateRegisters[j].first, it->stealValue());
      } else {
        res->setValue(
            row, _aggregateRegisters[j].first, AqlValue(arangodb::basics::VelocyPackHelper::NullValue()));
      }
      ++j;
    }

    // set the group values
    if (_collectRegister != ExecutionNode::MaxRegisterId) {
      _currentGroup.addValues(cur, _collectRegister);

      if (static_cast<CollectNode const*>(_exeNode)->_count) {
        // only set group count in result register
        res->setValue(row, _collectRegister, AqlValue(static_cast<uint64_t>(_currentGroup.groupLength)));
      } else if (static_cast<CollectNode const*>(_exeNode)->_expressionVariable !=
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
      _collectRegister(ExecutionNode::MaxRegisterId) {
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
    TRI_ASSERT(static_cast<CollectNode const*>(_exeNode)->_count);

    auto const& registerPlan = en->getRegisterPlan()->varInfo;
    auto it = registerPlan.find(en->_outVariable->id);
    TRI_ASSERT(it != registerPlan.end());
    _collectRegister = (*it).second.registerId;
    TRI_ASSERT(_collectRegister > 0 &&
               _collectRegister < ExecutionNode::MaxRegisterId);
  } else {
    TRI_ASSERT(!static_cast<CollectNode const*>(_exeNode)->_count);
  }

  TRI_ASSERT(!_groupRegisters.empty());
}

HashedCollectBlock::~HashedCollectBlock() {}

int HashedCollectBlock::getOrSkipSome(size_t atLeast, size_t atMost,
                                      bool skipping, AqlItemBlock*& result,
                                      size_t& skipped) {
  TRI_ASSERT(result == nullptr && skipped == 0);

  if (_done) {
    return TRI_ERROR_NO_ERROR;
  }

  if (_buffer.empty()) {
    if (!ExecutionBlock::getBlock(atLeast, atMost)) {
      // done
      _done = true;

      return TRI_ERROR_NO_ERROR;
    }
    _pos = 0;  // this is in the first block
  }

  auto* en = static_cast<CollectNode const*>(_exeNode);

  // If we get here, we do have _buffer.front()
  AqlItemBlock* cur = _buffer.front();
  TRI_ASSERT(cur != nullptr);
  size_t const curNrRegs = cur->getNrRegs();

  TRI_ASSERT(_aggregateRegisters.size() == en->_aggregateVariables.size());

  std::unordered_map<std::vector<AqlValue>, AggregateValuesType*, GroupKeyHash,
                     GroupKeyEqual> allGroups(1024,
                                              GroupKeyHash(_trx, _groupRegisters.size()),
                                              GroupKeyEqual(_trx));

  // cleanup function for group values
  auto cleanup = [&allGroups]() -> void {
    for (auto& it : allGroups) {
      delete it.second;
    }
  };

  // prevent memory leaks by always cleaning up the groups
  TRI_DEFER(cleanup());

  auto buildResult = [&](AqlItemBlock const* src) {
    RegisterId nrRegs = en->getRegisterPlan()->nrRegs[en->getDepth()];

    std::unique_ptr<AqlItemBlock> result(requestBlock(allGroups.size(), nrRegs));

    if (src != nullptr) {
      inheritRegisters(src, result.get(), 0);
    }

    TRI_ASSERT(!en->_count || _collectRegister != ExecutionNode::MaxRegisterId);

    size_t row = 0;
    for (auto& it : allGroups) {
      auto& keys = it.first;
      TRI_ASSERT(it.second != nullptr);

      TRI_ASSERT(keys.size() == _groupRegisters.size());
      size_t i = 0;
      for (auto& key : keys) {
        result->setValue(row, _groupRegisters[i++].first, key);
        const_cast<AqlValue*>(&key)->erase(); // to prevent double-freeing later
      }

      if (!en->_count) {
        TRI_ASSERT(it.second->size() == _aggregateRegisters.size());
        size_t j = 0;
        for (auto const& r : *(it.second)) {
          result->setValue(row, _aggregateRegisters[j++].first,
                           r->stealValue());
        }
      } else if (en->_count) {
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

  std::vector<AqlValue> groupValues;
  size_t const n = _groupRegisters.size();
  groupValues.reserve(n);

  std::vector<AqlValue> group;
  group.reserve(n);

  try {
    while (skipped < atMost) {
      TRI_IF_FAILURE("HashedCollectBlock::getOrSkipSomeOuter") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }

      throwIfKilled();  // check if we were aborted

      groupValues.clear();

      // for hashing simply re-use the aggregate registers, without cloning
      // their contents
      for (size_t i = 0; i < n; ++i) {
        groupValues.emplace_back(
            cur->getValueReference(_pos, _groupRegisters[i].second));
      }

      // now check if we already know this group
      auto it = allGroups.find(groupValues);

      if (it == allGroups.end()) {
        // new group
        group.clear();

        // copy the group values before they get invalidated
        for (size_t i = 0; i < n; ++i) {
          group.emplace_back(
              cur->getValueReference(_pos, _groupRegisters[i].second).clone());
        }

        auto aggregateValues = std::make_unique<AggregateValuesType>();

        if (en->_aggregateVariables.empty()) {
          // no aggregate registers. this means we'll only count the number of
          // items
          if (en->_count) {
            aggregateValues->emplace_back(std::make_unique<AggregatorLength>(_trx, 1));
          }
        } else {
          // we do have aggregate registers. create them as empty AqlValues
          aggregateValues->reserve(_aggregateRegisters.size());

          // initialize aggregators
          size_t j = 0;
          for (auto const& r : en->_aggregateVariables) {
            aggregateValues->emplace_back(Aggregator::fromTypeString(_trx, r.second.second));
            aggregateValues->back()->reduce(
                GetValueForRegister(cur, _pos, _aggregateRegisters[j].second));
            ++j;
          }
        }

        // note: aggregateValues may be a nullptr!
        allGroups.emplace(group, aggregateValues.get());
        aggregateValues.release();
      } else {
        // existing group
        auto aggregateValues = (*it).second;

        if (en->_aggregateVariables.empty()) {
          // no aggregate registers. simply increase the counter
          if (en->_count) {
            TRI_ASSERT(!aggregateValues->empty());
            aggregateValues->back()->reduce(AqlValue());
          }
        } else {
          // apply the aggregators for the group
          TRI_ASSERT(aggregateValues->size() == _aggregateRegisters.size());
          size_t j = 0;
          for (auto const& r : _aggregateRegisters) {
            (*aggregateValues)[j]->reduce(
                GetValueForRegister(cur, _pos, r.second));
            ++j;
          }
        }
      }

      if (++_pos >= cur->size()) {
        _buffer.pop_front();
        _pos = 0;

        bool hasMore = !_buffer.empty();

        if (!hasMore) {
          hasMore = ExecutionBlock::getBlock(atLeast, atMost);
        }

        if (!hasMore) {
          // no more input. we're done
          try {
            // emit last buffered group
            if (!skipping) {
              TRI_IF_FAILURE("HashedCollectBlock::getOrSkipSome") {
                THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
              }

              throwIfKilled();
            }

            ++skipped;
            result = buildResult(cur);

            returnBlock(cur);
            _done = true;

            groupValues.clear();

            return TRI_ERROR_NO_ERROR;
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
  } catch (...) {
    // clean up
    for (auto& it : allGroups) {
      for (auto& it2 : it.first) {
        const_cast<AqlValue*>(&it2)->destroy();
      }
    }
    throw;
  }

  groupValues.clear();

  if (!skipping) {
    TRI_ASSERT(skipped > 0);
  }

  result = buildResult(nullptr);

  return TRI_ERROR_NO_ERROR;
}

/// @brief hasher for groups
size_t HashedCollectBlock::GroupKeyHash::operator()(
    std::vector<AqlValue> const& value) const {
  uint64_t hash = 0x12345678;

  TRI_ASSERT(value.size() == _num);

  for (auto const& it : value) {
    // we must use the slow hash function here, because a value may have 
    // different representations in case its an array/object/number
    // (calls normalizedHash() internally)
    hash = it.hash(_trx, hash);
  }

  return static_cast<size_t>(hash);
}

/// @brief comparator for groups
bool HashedCollectBlock::GroupKeyEqual::operator()(
    std::vector<AqlValue> const& lhs, std::vector<AqlValue> const& rhs) const {
  size_t const n = lhs.size();

  for (size_t i = 0; i < n; ++i) {
    int res =
        AqlValue::Compare(_trx, lhs[i], rhs[i], false);

    if (res != 0) {
      return false;
    }
  }

  return true;
}
