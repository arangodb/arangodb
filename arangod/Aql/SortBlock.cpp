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

#include "SortBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Basics/Exceptions.h"
#include "VocBase/vocbase.h"

using namespace arangodb::aql;

SortBlock::SortBlock(ExecutionEngine* engine, SortNode const* en)
    : ExecutionBlock(engine, en), _sortRegisters(), _stable(en->_stable), _mustFetchAll(true) {
  for (auto const& p : en->_elements) {
    auto it = en->getRegisterPlan()->varInfo.find(p.first->id);
    TRI_ASSERT(it != en->getRegisterPlan()->varInfo.end());
    TRI_ASSERT(it->second.registerId < ExecutionNode::MaxRegisterId);
    _sortRegisters.emplace_back(
        std::make_pair(it->second.registerId, p.second));
  }
}

SortBlock::~SortBlock() {}

int SortBlock::initializeCursor(AqlItemBlock* items, size_t pos) {
  DEBUG_BEGIN_BLOCK(); 
  int res = ExecutionBlock::initializeCursor(items, pos);
  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  _mustFetchAll = !_done;
  _pos = 0;

  return TRI_ERROR_NO_ERROR;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();  
}

int SortBlock::getOrSkipSome(size_t atLeast, size_t atMost, bool skipping,
                             AqlItemBlock*& result, size_t& skipped) {
  DEBUG_BEGIN_BLOCK(); 
  
  TRI_ASSERT(result == nullptr && skipped == 0);
  
  if (_mustFetchAll) {
    // suck all blocks into _buffer
    while (getBlock(DefaultBatchSize(), DefaultBatchSize())) {
    }

    _mustFetchAll = false;
    if (!_buffer.empty()) {
      doSorting();
    }
  }

  return ExecutionBlock::getOrSkipSome(atLeast, atMost, skipping, result, skipped);
  
  // cppcheck-suppress style
  DEBUG_END_BLOCK();  
}

void SortBlock::doSorting() {
  DEBUG_BEGIN_BLOCK();  

  size_t sum = 0;
  for (auto const& block : _buffer) {
    sum += block->size();
  }

  TRI_IF_FAILURE("SortBlock::doSorting") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  
  // coords[i][j] is the <j>th row of the <i>th block
  std::vector<std::pair<size_t, size_t>> coords;
  coords.reserve(sum);

  // install the coords
  size_t count = 0;

  for (auto const& block : _buffer) {
    for (size_t i = 0; i < block->size(); i++) {
      coords.emplace_back(std::make_pair(count, i));
    }
    count++;
  }

  // comparison function
  OurLessThan ourLessThan(_trx, _buffer, _sortRegisters);

  // sort coords
  if (_stable) {
    std::stable_sort(coords.begin(), coords.end(), ourLessThan);
  } else {
    std::sort(coords.begin(), coords.end(), ourLessThan);
  }

  // here we collect the new blocks (later swapped into _buffer):
  std::deque<AqlItemBlock*> newbuffer;

  try {  // If we throw from here, the catch will delete the new
    // blocks in newbuffer

    count = 0;
    RegisterId const nrregs = _buffer.front()->getNrRegs();

    std::unordered_map<AqlValue, AqlValue> cache;

    // install the rearranged values from _buffer into newbuffer

    while (count < sum) {
      size_t sizeNext = (std::min)(sum - count, DefaultBatchSize());
      AqlItemBlock* next = new AqlItemBlock(_engine->getQuery()->resourceMonitor(), sizeNext, nrregs);

      try {
        TRI_IF_FAILURE("SortBlock::doSortingInner") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        newbuffer.emplace_back(next);
      } catch (...) {
        delete next;
        throw;
      }

      cache.clear();
      // only copy as much as needed!
      for (size_t i = 0; i < sizeNext; i++) {
        for (RegisterId j = 0; j < nrregs; j++) {
          auto a =
              _buffer[coords[count].first]->getValue(coords[count].second, j);
          // If we have already dealt with this value for the next
          // block, then we just put the same value again:
          if (!a.isEmpty()) {
            auto it = cache.find(a);

            if (it != cache.end()) {
              AqlValue const& b = it->second;
              // If one of the following throws, all is well, because
              // the new block already has either a copy or stolen
              // the AqlValue:
              _buffer[coords[count].first]->eraseValue(coords[count].second, j);
              next->setValue(i, j, b);
            } else {
              // We need to copy a, if it has already been stolen from
              // its original buffer, which we know by looking at the
              // valueCount there.
              auto vCount = _buffer[coords[count].first]->valueCount(a);

              if (vCount == 0) {
                // Was already stolen for another block
                AqlValue b = a.clone();
                try {
                  TRI_IF_FAILURE("SortBlock::doSortingCache") {
                    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
                  }
                  cache.emplace(a, b);
                } catch (...) {
                  b.destroy();
                  throw;
                }

                try {
                  TRI_IF_FAILURE("SortBlock::doSortingNext1") {
                    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
                  }
                  next->setValue(i, j, b);
                } catch (...) {
                  b.destroy();
                  cache.erase(a);
                  throw;
                }
                // It does not matter whether the following works or not,
                // since the original block keeps its responsibility
                // for a:
                _buffer[coords[count].first]->eraseValue(coords[count].second,
                                                         j);
              } else {
                // Here we are the first to want to inherit a, so we
                // steal it:
                _buffer[coords[count].first]->steal(a);
                // If this has worked, responsibility is now with the
                // new block or indeed with us!
                try {
                  TRI_IF_FAILURE("SortBlock::doSortingNext2") {
                    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
                  }
                  next->setValue(i, j, a);
                } catch (...) {
                  a.destroy();
                  throw;
                }
                _buffer[coords[count].first]->eraseValue(coords[count].second,
                                                         j);
                // This might throw as well, however, the responsibility
                // is already with the new block.

                // If the following does not work, we will create a
                // few unnecessary copies, but this does not matter:
                cache.emplace(a, a);
              }
            }
          }
        }
        count++;
      }
      cache.clear();
    }
  } catch (...) {
    for (auto& x : newbuffer) {
      delete x;
    }
    throw;
  }
  _buffer.swap(newbuffer);  // does not throw since allocators
  // are the same
  for (auto& x : newbuffer) {
    delete x;
  }
  DEBUG_END_BLOCK();  
}

bool SortBlock::OurLessThan::operator()(std::pair<size_t, size_t> const& a,
                                        std::pair<size_t, size_t> const& b) const {
  for (auto const& reg : _sortRegisters) {
    int cmp = AqlValue::Compare(
        _trx, _buffer[a.first]->getValueReference(a.second, reg.first),
        _buffer[b.first]->getValueReference(b.second, reg.first), true);

    if (cmp < 0) {
      return reg.second;
    } else if (cmp > 0) {
      return !reg.second;
    }
  }

  return false;
}
