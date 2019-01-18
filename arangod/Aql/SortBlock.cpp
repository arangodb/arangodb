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

#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Basics/Exceptions.h"
#include "SortBlock.h"
#include "VocBase/vocbase.h"

namespace {
void copyRow(std::unordered_map<arangodb::aql::AqlValue, arangodb::aql::AqlValue>& cache,
             arangodb::aql::RegisterId const nrRegs, arangodb::aql::AqlItemBlock* src,
             size_t sRow, arangodb::aql::AqlItemBlock* dst, size_t dRow) {
  for (arangodb::aql::RegisterId reg = 0; reg < nrRegs; reg++) {
    auto const& original = src->getValueReference(sRow, reg);
    // If we have already dealt with this value for the next
    // block, then we just put the same value again:
    if (!original.isEmpty()) {
      if (original.requiresDestruction()) {
        // complex value, with ownership transfer
        auto it = cache.find(original);

        if (it != cache.end()) {
          // If one of the following throws, all is well, because
          // the new block already has either a copy or stolen
          // the AqlValue:
          src->eraseValue(sRow, reg);
          dst->setValue(dRow, reg, (*it).second);
        } else {
          // We need to copy original, if it has already been stolen from
          // its source buffer, which we know by looking at the
          // valueCount there.
          auto vCount = src->valueCount(original);

          if (vCount == 0) {
            // Was already stolen for another block
            arangodb::aql::AqlValue copy = original.clone();
            try {
              TRI_IF_FAILURE("SortBlock::doSortingCache") {
                THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
              }
              cache.emplace(original, copy);
            } catch (...) {
              copy.destroy();
              throw;
            }

            try {
              TRI_IF_FAILURE("SortBlock::doSortingNext1") {
                THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
              }
              dst->setValue(dRow, reg, copy);
            } catch (...) {
              cache.erase(copy);
              copy.destroy();
              throw;
            }
            // It does not matter whether the following works or not,
            // since the source block keeps its responsibility
            // for original:
            src->eraseValue(sRow, reg);
          } else {
            TRI_IF_FAILURE("SortBlock::doSortingNext2") {
              THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
            }
            // Here we are the first to want to inherit original, so we
            // steal it:
            dst->setValue(dRow, reg, original);
            src->steal(original);
            src->eraseValue(sRow, reg);
            // If this has worked, responsibility is now with the
            // new block or requestBlockindeed with us!
            // If the following does not work, we will create a
            // few unnecessary copies, but this does not matter:
            cache.emplace(original, original);
          }
        }
      } else {
        // simple value, which does not need ownership transfer
        TRI_IF_FAILURE("SortBlock::doSortingCache") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        TRI_IF_FAILURE("SortBlock::doSortingNext1") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        TRI_IF_FAILURE("SortBlock::doSortingNext2") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        dst->setValue(dRow, reg, original);
        src->eraseValue(sRow, reg);
      }
    }
  }
}

class StandardSorter : public arangodb::aql::SortBlock::Sorter {
 private:
  class OurLessThan {
   public:
    OurLessThan(arangodb::transaction::Methods* trx,
                std::deque<arangodb::aql::AqlItemBlock*>& buffer,
                std::vector<arangodb::aql::SortRegister>& sortRegisters) noexcept
        : _trx(trx), _buffer(buffer), _sortRegisters(sortRegisters) {}

    bool operator()(std::pair<uint32_t, uint32_t> const& a,
                    std::pair<uint32_t, uint32_t> const& b) const {
      for (auto const& reg : _sortRegisters) {
        auto const& lhs = _buffer[a.first]->getValueReference(a.second, reg.reg);
        auto const& rhs = _buffer[b.first]->getValueReference(b.second, reg.reg);

        int const cmp = arangodb::aql::AqlValue::Compare(_trx, lhs, rhs, true);

        if (cmp < 0) {
          return reg.asc;
        } else if (cmp > 0) {
          return !reg.asc;
        }
      }

      return false;
    }

   private:
    arangodb::transaction::Methods* _trx;
    std::deque<arangodb::aql::AqlItemBlock*>& _buffer;
    std::vector<arangodb::aql::SortRegister>& _sortRegisters;
  };  // OurLessThan

 public:
  StandardSorter(arangodb::aql::SortBlock& block, arangodb::transaction::Methods* trx,
                 std::deque<arangodb::aql::AqlItemBlock*>& buffer,
                 std::vector<arangodb::aql::SortRegister>& sortRegisters,
                 Fetcher&& fetch, Allocator&& allocate)
      : arangodb::aql::SortBlock::Sorter(block, trx, buffer, sortRegisters,
                                         std::move(fetch), std::move(allocate)) {}

  virtual std::pair<arangodb::aql::ExecutionState, arangodb::Result> fetch() {
    using arangodb::aql::ExecutionBlock;
    using arangodb::aql::ExecutionState;

    ExecutionState res = ExecutionState::HASMORE;
    // suck all blocks into _buffer
    while (res != ExecutionState::DONE) {
      res = _fetch(ExecutionBlock::DefaultBatchSize()).first;
      if (res == ExecutionState::WAITING) {
        return {res, TRI_ERROR_NO_ERROR};
      }
    }
    return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
  }

  virtual arangodb::Result sort() {
    using arangodb::aql::AqlItemBlock;
    using arangodb::aql::AqlValue;
    using arangodb::aql::ExecutionBlock;
    using arangodb::aql::RegisterId;
    using arangodb::basics::catchVoidToResult;

    TRI_IF_FAILURE("SortBlock::doSorting") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    size_t sum = 0;
    for (auto const& block : _buffer) {
      sum += block->size();
    }

    // coords[i][j] is the <j>th row of the <i>th block
    std::vector<std::pair<uint32_t, uint32_t>> coords;
    coords.reserve(sum);

    // install the coords
    // we are intentionally using uint32_t here to save memory and
    // have better cache utilization
    uint32_t count = 0;

    for (auto const& block : _buffer) {
      uint32_t const n = static_cast<uint32_t>(block->size());

      for (uint32_t i = 0; i < n; i++) {
        coords.emplace_back(std::make_pair(count, i));
      }
      ++count;
    }

    // comparison function
    OurLessThan ourLessThan(_trx, _buffer, _sortRegisters);

    // sort coords
    if (_block.stable()) {
      std::stable_sort(coords.begin(), coords.end(), ourLessThan);
    } else {
      std::sort(coords.begin(), coords.end(), ourLessThan);
    }

    // here we collect the new blocks (later swapped into _buffer):
    std::deque<AqlItemBlock*> newbuffer;

    arangodb::Result result = catchVoidToResult([&]() -> void {
      // If we throw from here, the cleanup will delete the new
      // blocks in newbuffer

      count = 0;
      RegisterId const nrRegs = _buffer.front()->getNrRegs();

      std::unordered_map<AqlValue, AqlValue> cache;

      // install the rearranged values from _buffer into newbuffer

      while (count < sum) {
        size_t sizeNext = (std::min)(sum - count, ExecutionBlock::DefaultBatchSize());
        AqlItemBlock* next = _allocate(sizeNext, nrRegs);

        try {
          TRI_IF_FAILURE("SortBlock::doSortingInner") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }
          newbuffer.emplace_back(next);
        } catch (...) {
          delete next;
          throw;
        }

        // only copy as much as needed!
        for (size_t i = 0; i < sizeNext; i++) {
          ::copyRow(cache, nrRegs, _buffer[coords[count].first],
                    coords[count].second, next, i);
          count++;
        }
        cache.clear();
      }
    });
    if (result.fail()) {
      for (auto& x : newbuffer) {
        delete x;
      }
      THROW_ARANGO_EXCEPTION(result);
    }

    _buffer.swap(newbuffer);  // does not throw since allocators
    // are the same
    for (auto& x : newbuffer) {
      delete x;
    }

    return TRI_ERROR_NO_ERROR;
  }

  virtual bool empty() const { return _buffer.empty(); }
};

class ConstrainedHeapSorter : public arangodb::aql::SortBlock::Sorter {
 private:
  bool rowLessThan(std::pair<arangodb::aql::AqlItemBlock*, uint32_t> const& a,
                   std::pair<arangodb::aql::AqlItemBlock*, uint32_t> const& b) {
    for (auto const& reg : _sortRegisters) {
      auto const& lhs = a.first->getValueReference(a.second, reg.reg);
      auto const& rhs = b.first->getValueReference(b.second, reg.reg);

      int const cmp = arangodb::aql::AqlValue::Compare(_trx, lhs, rhs, true);

      if (cmp < 0) {
        return reg.asc;
      } else if (cmp > 0) {
        return !reg.asc;
      }
    }

    return false;
  }

  std::function<bool(std::unique_ptr<arangodb::aql::AqlItemBlock> const& a,
                     std::unique_ptr<arangodb::aql::AqlItemBlock> const& b)>
  blockLessThan() {
    return [this](std::unique_ptr<arangodb::aql::AqlItemBlock> const& a,
                  std::unique_ptr<arangodb::aql::AqlItemBlock> const& b) -> bool {
      return rowLessThan({a.get(), 0}, {b.get(), 0});
    };
  }

 public:
  ConstrainedHeapSorter(arangodb::aql::SortBlock& block, arangodb::transaction::Methods* trx,
                        std::deque<arangodb::aql::AqlItemBlock*>& buffer,
                        std::vector<arangodb::aql::SortRegister>& sortRegisters,
                        Fetcher&& fetch, Allocator&& allocate, size_t limit)
      : arangodb::aql::SortBlock::Sorter(block, trx, buffer, sortRegisters,
                                         std::move(fetch), std::move(allocate)),
        _limit{limit} {
    TRI_ASSERT(_limit > 0);
    _rows.reserve(_limit);
  }

  virtual std::pair<arangodb::aql::ExecutionState, arangodb::Result> fetch() {
    using arangodb::aql::AqlItemBlock;
    using arangodb::aql::ExecutionBlock;
    using arangodb::aql::ExecutionState;

    ExecutionState res = ExecutionState::HASMORE;
    // suck all blocks into _buffer
    while (res != ExecutionState::DONE) {
      res = _fetch(ExecutionBlock::DefaultBatchSize()).first;
      if (res == ExecutionState::WAITING) {
        return {res, TRI_ERROR_NO_ERROR};
      }

      // handle batch
      while (!_buffer.empty()) {
        AqlItemBlock* block = _buffer.front();
        _buffer.pop_front();
        for (size_t row = 0; row < block->size(); row++) {
          pushRow(block, row);
        }
        delete block;
      }
    }

    return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
  }

  virtual arangodb::Result sort() {
    using arangodb::aql::AqlItemBlock;
    using arangodb::basics::catchVoidToResult;

    TRI_IF_FAILURE("SortBlock::doSorting") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    // sort heap in-place
    std::sort(_rows.begin(), _rows.end(), blockLessThan());

    // here we collect the new blocks, to later swap into _buffer
    std::deque<AqlItemBlock*> newBuffer;
    arangodb::Result result = catchVoidToResult([this, &newBuffer]() -> void {
      for (auto& row : _rows) {
        newBuffer.emplace_back(row.release());
      }
    });
    if (result.fail()) {
      // have to actually clean up the blocks that have been moved to the
      // buffer since they aren't managed pointers anymore
      for (auto& block : newBuffer) {
        delete block;
      }
      THROW_ARANGO_EXCEPTION(result);
    }
    _rows.clear();

    // everything is sorted and in the right form now, just swap
    _buffer.swap(newBuffer);

    return TRI_ERROR_NO_ERROR;
  }

  virtual bool empty() const { return _buffer.empty() && _rows.empty(); }

 private:
  arangodb::Result pushRow(arangodb::aql::AqlItemBlock* block, size_t row) {
    using arangodb::aql::AqlItemBlock;
    using arangodb::aql::AqlValue;
    using arangodb::aql::RegisterId;

    if (_rows.size() >= _limit && rowLessThan({_rows[0].get(), 0}, {block, row})) {
      // skip row, already too low in sort order to make it past limit
      return TRI_ERROR_NO_ERROR;
    }

    if (_rows.size() >= _limit) {
      // pop an entry first
      std::pop_heap(_rows.begin(), _rows.end(), blockLessThan());
      _rows.pop_back();
    }

    // now copy the row
    RegisterId const nrRegs = block->getNrRegs();
    std::unordered_map<AqlValue, AqlValue> cache;
    std::unique_ptr<AqlItemBlock> copy{_allocate(1, nrRegs)};
    if (copy == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }

    TRI_IF_FAILURE("SortBlock::doSortingInner") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    ::copyRow(cache, nrRegs, block, row, copy.get(), 0);

    // now insert copy into heap
    _rows.emplace_back(std::move(copy));
    std::push_heap(_rows.begin(), _rows.end(), blockLessThan());

    return TRI_ERROR_NO_ERROR;
  }

 private:
  size_t _limit;
  std::vector<std::unique_ptr<arangodb::aql::AqlItemBlock>> _rows;
};
}  // namespace

namespace arangodb {
namespace aql {

SortBlock::Sorter::Sorter(arangodb::aql::SortBlock& block, transaction::Methods* trx,
                          std::deque<AqlItemBlock*>& buffer,
                          std::vector<SortRegister>& sortRegisters,
                          Fetcher&& fetch, Allocator&& allocate)
    : _block{block}, _trx{trx}, _buffer{buffer}, _sortRegisters{sortRegisters}, _fetch{std::move(fetch)}, _allocate{std::move(allocate)} {}

SortBlock::Sorter::~Sorter() {}

SortBlock::SortBlock(ExecutionEngine* engine, SortNode const* en, SorterType type, size_t limit)
    : ExecutionBlock(engine, en), _stable(en->_stable), _type{type}, _limit{limit} {
  TRI_ASSERT(en && en->plan() && en->getRegisterPlan());
  SortRegister::fill(*en->plan(), *en->getRegisterPlan(), en->elements(), _sortRegisters);
  initializeSorter();
}

SortBlock::~SortBlock() {}

std::pair<ExecutionState, arangodb::Result> SortBlock::initializeCursor(AqlItemBlock* items,
                                                                        size_t pos) {
  auto res = ExecutionBlock::initializeCursor(items, pos);

  if (res.first == ExecutionState::WAITING || res.second.fail()) {
    // If we need to wait or get an error we return as is.
    return res;
  }

  _mustFetchAll = !_done;
  _pos = 0;

  return res;
}

std::pair<ExecutionState, arangodb::Result> SortBlock::getOrSkipSome(
    size_t atMost, bool skipping, AqlItemBlock*& result, size_t& skipped) {
  TRI_ASSERT(_sorter != nullptr && result == nullptr && skipped == 0);

  if (_mustFetchAll) {
    // sorter handles all the dirty work
    auto res = _sorter->fetch();
    if (res.first == ExecutionState::WAITING || res.second.fail()) {
      // If we need to wait or get an error we return as is.
      return res;
    }
    _mustFetchAll = false;

    if (!_sorter->empty()) {
      auto result = _sorter->sort();
      if (result.fail()) {
        return {ExecutionState::DONE, result};
      }
    }
  }

  return ExecutionBlock::getOrSkipSome(atMost, skipping, result, skipped);
}

bool SortBlock::stable() const { return _stable; }

void SortBlock::initializeSorter() {
  if (_sorter == nullptr) {
    auto fetch = [this](size_t atMost) -> std::pair<ExecutionState, bool> {
      return getBlock(atMost);
    };
    auto allocate = [this](size_t nrItems, RegisterId nrRegs) -> AqlItemBlock* {
      return requestBlock(nrItems, nrRegs);
    };
    switch (_type) {
      case SorterType::Standard: {
        _sorter = std::make_unique<::StandardSorter>(*this, _trx, _buffer,
                                                     _sortRegisters, std::move(fetch),
                                                     std::move(allocate));
        break;
      }
      case SorterType::ConstrainedHeap: {
        TRI_ASSERT(!_stable && _limit > 0);
        _sorter = std::make_unique<::ConstrainedHeapSorter>(*this, _trx, _buffer, _sortRegisters,
                                                            std::move(fetch),
                                                            std::move(allocate), _limit);
        break;
      }
    }
  }
}

}  // namespace aql
}  // namespace arangodb
