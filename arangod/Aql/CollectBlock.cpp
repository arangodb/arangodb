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

#include "Aql/CollectBlock.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Basics/Exceptions.h"
#include "VocBase/vocbase.h"

using namespace std;
using namespace triagens::arango;
using namespace triagens::aql;

using Json = triagens::basics::Json;
using JsonHelper = triagens::basics::JsonHelper;
using StringBuffer = triagens::basics::StringBuffer;


CollectGroup::CollectGroup(bool count)
    : firstRow(0),
      lastRow(0),
      groupLength(0),
      rowsAreValid(false),
      count(count) {}

CollectGroup::~CollectGroup() {
  // reset();
  for (auto& it : groupBlocks) {
    delete it;
  }
  for (auto& it : groupValues) {
    it.destroy();
  }
}

void CollectGroup::initialize(size_t capacity) {
  // TRI_ASSERT(capacity > 0);

  groupValues.clear();
  collections.clear();

  if (capacity > 0) {
    groupValues.reserve(capacity);
    collections.reserve(capacity);

    for (size_t i = 0; i < capacity; ++i) {
      groupValues.emplace_back();
      collections.emplace_back(nullptr);
    }
  }

  groupLength = 0;
}

void CollectGroup::reset() {
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
}

void CollectGroup::addValues(AqlItemBlock const* src,
                                RegisterId groupRegister) {
  if (groupRegister == ExecutionNode::MaxRegisterId) {
    // nothing to do
    return;
  }

  if (rowsAreValid) {
    // emit group details
    TRI_ASSERT(firstRow <= lastRow);

    if (count) {
      groupLength += lastRow + 1 - firstRow;
    } else {
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

    auto itIn = en->getRegisterPlan()->varInfo.find(p.second.first->id);
    TRI_ASSERT(itIn != en->getRegisterPlan()->varInfo.end());
    TRI_ASSERT((*itIn).second.registerId < ExecutionNode::MaxRegisterId);
    TRI_ASSERT((*itOut).second.registerId < ExecutionNode::MaxRegisterId);
    _aggregateRegisters.emplace_back(
        std::make_pair((*itOut).second.registerId, (*itIn).second.registerId));
  }
  TRI_ASSERT(_aggregateRegisters.size() == en->_aggregateVariables.size());

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
      _variableNames.emplace_back("");  // initialize with some default value
    }

    // iterate over all our variables
    if (en->_keepVariables.empty()) {
      auto&& usedVariableIds = en->getVariableIdsUsedHere();

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

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize
////////////////////////////////////////////////////////////////////////////////

int SortedCollectBlock::initialize() {
  int res = ExecutionBlock::initialize();

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // reserve space for the current row
  _currentGroup.initialize(_groupRegisters.size());

  return TRI_ERROR_NO_ERROR;
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
        res.reset(new AqlItemBlock(1, getPlanNode()
                                          ->getRegisterPlan()
                                          ->nrRegs[getPlanNode()->getDepth()]));
        emitGroup(nullptr, res.get(), skipped);
        result = res.release();
      }

      return TRI_ERROR_NO_ERROR;
    }
    _pos = 0;  // this is in the first block
  }

  // If we get here, we do have _buffer.front()
  AqlItemBlock* cur = _buffer.front();

  if (!skipping) {
    res.reset(new AqlItemBlock(
        atMost,
        getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]));

    TRI_ASSERT(cur->getNrRegs() <= res->getNrRegs());
    inheritRegisters(cur, res.get(), _pos);
  }

  while (skipped < atMost) {
    // read the next input row

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
              _trx, _currentGroup.groupValues[i], _currentGroup.collections[i],
              cur->getValue(_pos, it.second),
              cur->getDocumentCollection(it.second), false);

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
          emitGroup(cur, res.get(), skipped);
        }

        // increase output row count
        ++skipped;

        if (skipped == atMost) {
          // output is full
          // do NOT advance input pointer
          result = res.release();
          return TRI_ERROR_NO_ERROR;
        }
      }

      // still space left in the output to create a new group

      // construct the new group
      size_t i = 0;
      for (auto& it : _groupRegisters) {
        _currentGroup.groupValues[i] =
            cur->getValue(_pos, it.second).clone();
        _currentGroup.collections[i] = cur->getDocumentCollection(it.second);
        ++i;
      }
      if (!skipping) {
        _currentGroup.setFirstRow(_pos);
      }
    }

    if (!skipping) {
      _currentGroup.setLastRow(_pos);
    }

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
          delete cur;
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

            emitGroup(cur, res.get(), skipped);
            ++skipped;
            res->shrink(skipped);
          } else {
            ++skipped;
          }
          delete cur;
          _done = true;
          result = res.release();
          return TRI_ERROR_NO_ERROR;
        } catch (...) {
          delete cur;
          throw;
        }
      }

      // hasMore

      // move over the last group details into the group before we delete the
      // block
      _currentGroup.addValues(cur, _collectRegister);

      delete cur;
      cur = _buffer.front();
    }
  }

  if (!skipping) {
    TRI_ASSERT(skipped > 0);
    res->shrink(skipped);
  }

  result = res.release();
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes the current group data into the result
////////////////////////////////////////////////////////////////////////////////

void SortedCollectBlock::emitGroup(AqlItemBlock const* cur, AqlItemBlock* res,
                                     size_t row) {
  if (row > 0) {
    // re-use already copied aqlvalues
    for (RegisterId i = 0; i < cur->getNrRegs(); i++) {
      res->setValue(row, i, res->getValue(0, i));
      // Note: if this throws, then all values will be deleted
      // properly since the first one is.
    }
  }

  size_t i = 0;
  for (auto& it : _groupRegisters) {
    if (_currentGroup.groupValues[i].type() == AqlValue::SHAPED) {
      // if a value in the group is a document, it must be converted into its
      // JSON equivalent. the reason is
      // that a group might theoretically consist of multiple documents, from
      // different collections. but there
      // is only one collection pointer per output register
      auto document = cur->getDocumentCollection(it.second);
      res->setValue(row, it.first,
                    AqlValue(new Json(_currentGroup.groupValues[i].toJson(
                        _trx, document, true))));
    } else {
      res->setValue(row, it.first, _currentGroup.groupValues[i]);
    }
    // ownership of value is transferred into res
    _currentGroup.groupValues[i].erase();
    ++i;
  }

  if (_collectRegister != ExecutionNode::MaxRegisterId) {
    // set the group values
    _currentGroup.addValues(cur, _collectRegister);

    if (static_cast<CollectNode const*>(_exeNode)->_count) {
      // only set group count in result register
      res->setValue(
          row, _collectRegister,
          AqlValue(new Json(static_cast<double>(_currentGroup.groupLength))));
    } else if (static_cast<CollectNode const*>(_exeNode)
                   ->_expressionVariable != nullptr) {
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
 
    auto itIn = en->getRegisterPlan()->varInfo.find(p.second.first->id);
    TRI_ASSERT(itIn != en->getRegisterPlan()->varInfo.end());
    TRI_ASSERT((*itIn).second.registerId < ExecutionNode::MaxRegisterId);
    TRI_ASSERT((*itOut).second.registerId < ExecutionNode::MaxRegisterId);
    _aggregateRegisters.emplace_back(
        std::make_pair((*itOut).second.registerId, (*itIn).second.registerId));
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

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize
////////////////////////////////////////////////////////////////////////////////

int HashedCollectBlock::initialize() {
  int res = ExecutionBlock::initialize();

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  return TRI_ERROR_NO_ERROR;
}

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
        
  // set up collections
  std::vector<TRI_document_collection_t const*> groupColls;
  for (auto const& it : _groupRegisters) {
    groupColls.emplace_back(cur->getDocumentCollection(it.second));
  }
  std::vector<TRI_document_collection_t const*> aggregateColls;
  for (auto const& it : _aggregateRegisters) {
    aggregateColls.emplace_back(cur->getDocumentCollection(it.second));
  }

  TRI_ASSERT(aggregateColls.size() == en->_aggregateVariables.size());
  TRI_ASSERT(_aggregateRegisters.size() == en->_aggregateVariables.size());
  
  typedef std::vector<Aggregator*> AggregateValuesType;

  std::unordered_map<std::vector<AqlValue>, AggregateValuesType*, GroupKeyHash, GroupKeyEqual>
      allGroups(1024, GroupKeyHash(_trx, groupColls), GroupKeyEqual(_trx, groupColls));

  // cleanup function for group values
  auto cleanup = [&allGroups] () -> void {
    for (auto& it : allGroups) {
      for (auto& it2 : *(it.second)) {
        delete it2;
      }
      delete it.second;
    }
    allGroups.clear();
  };

  // prevent memory leaks by always cleaning up the groups 
  TRI_DEFER(cleanup()); 

  auto buildResult = [&](AqlItemBlock const* src) {
    TRI_ASSERT(groupColls.size() == _groupRegisters.size());
    TRI_ASSERT(aggregateColls.size() == _aggregateRegisters.size());

    auto nrRegs = en->getRegisterPlan()->nrRegs[en->getDepth()];

    auto result = std::make_unique<AqlItemBlock>(allGroups.size(), nrRegs);

    if (src != nullptr) {
      inheritRegisters(src, result.get(), 0);
    }

    // collections
    for (size_t i = 0; i < _groupRegisters.size(); ++i) {
      result->setDocumentCollection(_groupRegisters[i].first, groupColls[i]);
    }
    for (size_t i = 0; i < _aggregateRegisters.size(); ++i) {
      result->setDocumentCollection(_aggregateRegisters[i].first, aggregateColls[i]);
    }

    TRI_ASSERT(!en->_count ||
               _collectRegister != ExecutionNode::MaxRegisterId);

    size_t const n = _groupRegisters.size();
    size_t row = 0;
    for (auto const& it : allGroups) {
      auto& keys = it.first;

      TRI_ASSERT_EXPENSIVE(keys.size() == n);
      size_t i = 0;
      for (auto& key : keys) {
        result->setValue(row, _groupRegisters[i++].first, key);
        const_cast<AqlValue*>(&key)
            ->erase();  // to prevent double-freeing later
      }

      if (it.second != nullptr && ! en->_count) {
        TRI_ASSERT(it.second->size() == _aggregateRegisters.size());
        size_t j = 0;
        for (auto const& r : *(it.second)) {
          result->setValue(row, _aggregateRegisters[j++].first, r->stealValue());
        }
      }
      else if (en->_count) {
        // set group count in result register
        TRI_ASSERT(it.second != nullptr);
        result->setValue(row, _collectRegister, it.second->back()->stealValue());
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
              cur->getValueReference(_pos, _groupRegisters[i].second)
                  .clone());
        }

        auto aggregateValues = std::make_unique<AggregateValuesType>();

        if (en->_aggregateVariables.empty()) {
          // no aggregate registers. this means we'll only count the number of items
          if (en->_count) {
            aggregateValues->emplace_back(new AggregatorLength(_trx, 1));
          }
        }
        else {
          // we do have aggregate registers. create them as empty AqlValues
          aggregateValues->reserve(_aggregateRegisters.size());

          // initialize aggregators
          size_t j = 0;
          for (auto const& r : en->_aggregateVariables) {
            aggregateValues->emplace_back(Aggregator::fromTypeString(_trx, r.second.second));
            aggregateValues->back()->reduce(cur->getValueReference(_pos, _aggregateRegisters[j].second), aggregateColls[j]);
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
            aggregateValues->back()->reduce(AqlValue(), nullptr);
          }
        }
        else {
          // apply the aggregators for the group
          size_t j = 0;
          for (auto const& r : _aggregateRegisters) {
            (*aggregateValues)[j]->reduce(cur->getValueReference(_pos, r.second), aggregateColls[j]);
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
      delete it.second;
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

////////////////////////////////////////////////////////////////////////////////
/// @brief hasher for groups
////////////////////////////////////////////////////////////////////////////////

size_t HashedCollectBlock::GroupKeyHash::operator()(
    std::vector<AqlValue> const& value) const {
  uint64_t hash = 0x12345678;

  TRI_ASSERT(value.size() == _num);
  TRI_ASSERT(value.size() == _colls.size());

  for (size_t i = 0; i < _num; ++i) {
    hash ^= value[i].hash(_trx, _colls[i]);
  }

  return static_cast<size_t>(hash);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief comparator for groups
////////////////////////////////////////////////////////////////////////////////

bool HashedCollectBlock::GroupKeyEqual::operator()(
    std::vector<AqlValue> const& lhs, std::vector<AqlValue> const& rhs) const {
  size_t const n = lhs.size();

  for (size_t i = 0; i < n; ++i) {
    int res =
        AqlValue::Compare(_trx, lhs[i], _colls[i], rhs[i], _colls[i], false);

    if (res != 0) {
      return false;
    }
  }

  return true;
}

