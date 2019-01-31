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
#include "Basics/ScopeGuard.h"
#include "SortBlock.h"
#include "VocBase/vocbase.h"

namespace {
void stealRow(std::unordered_map<arangodb::aql::AqlValue, arangodb::aql::AqlValue>& cache,
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
            // new block or requestBlock, indeed with us!
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

void stealRowNoCache(arangodb::aql::RegisterId const nrRegs,
                     arangodb::aql::AqlItemBlock* src, size_t sRow,
                     arangodb::aql::AqlItemBlock* dst, size_t dRow) {
  for (arangodb::aql::RegisterId reg = 0; reg < nrRegs; reg++) {
    auto const& original = src->getValueReference(sRow, reg);
    // If we have already dealt with this value for the next
    // block, then we just put the same value again:
    if (!original.isEmpty()) {
      if (original.requiresDestruction()) {
        // We need to copy original, if it has already been stolen from
        // its source buffer, which we know by looking at the
        // valueCount there.
        auto vCount = src->valueCount(original);

        if (vCount == 0) {
          // Was already stolen for another block
          arangodb::aql::AqlValue copy = original.clone();

          try {
            TRI_IF_FAILURE("SortBlock::doSortingNext1") {
              THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
            }
            dst->setValue(dRow, reg, copy);
          } catch (...) {
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

  std::pair<arangodb::aql::ExecutionState, arangodb::Result> fetch() override {
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

  arangodb::Result sort() override {
    using arangodb::aql::AqlItemBlock;
    using arangodb::aql::AqlValue;
    using arangodb::aql::ExecutionBlock;
    using arangodb::aql::RegisterId;

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
    std::deque<AqlItemBlock*> newBuffer;

    try {
      // If we throw from here, the cleanup will delete the new
      // blocks in newBuffer

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
          newBuffer.emplace_back(next);
        } catch (...) {
          delete next;
          throw;
        }

        // only copy as much as needed!
        for (size_t i = 0; i < sizeNext; i++) {
          ::stealRow(cache, nrRegs, _buffer[coords[count].first],
                     coords[count].second, next, i);
          count++;
        }
        cache.clear();
      }
    } catch (...) {
      for (auto& x : newBuffer) {
        delete x;
      }
      throw;
    }

    _buffer.swap(newBuffer);  // does not throw since allocators are the same
    for (auto& x : newBuffer) {
      delete x;
    }

    return TRI_ERROR_NO_ERROR;
  }

  bool empty() const override { return _buffer.empty(); }
};

class ConstrainedHeapSorter : public arangodb::aql::SortBlock::Sorter {
 private:
  class OurLessThan {
   public:
    OurLessThan(arangodb::transaction::Methods* trx,
                std::vector<arangodb::aql::SortRegister>& sortRegisters) noexcept
        : _trx(trx), _lhsBuffer(nullptr), _rhsBuffer(nullptr), _sortRegisters(sortRegisters) {}

    void setBuffers(arangodb::aql::AqlItemBlock* lhsBuffer,
                    arangodb::aql::AqlItemBlock* rhsBuffer) {
      _lhsBuffer = lhsBuffer;
      _rhsBuffer = rhsBuffer;
    }

    bool operator()(uint32_t const& a, uint32_t const& b) const {
      TRI_ASSERT(_lhsBuffer);
      TRI_ASSERT(_rhsBuffer);

      for (auto const& reg : _sortRegisters) {
        auto const& lhs = _lhsBuffer->getValueReference(a, reg.reg);
        auto const& rhs = _rhsBuffer->getValueReference(b, reg.reg);

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
    arangodb::aql::AqlItemBlock* _lhsBuffer;
    arangodb::aql::AqlItemBlock* _rhsBuffer;
    std::vector<arangodb::aql::SortRegister>& _sortRegisters;
  };  // OurLessThan

 public:
  ConstrainedHeapSorter(arangodb::aql::SortBlock& block, arangodb::transaction::Methods* trx,
                        std::deque<arangodb::aql::AqlItemBlock*>& buffer,
                        std::vector<arangodb::aql::SortRegister>& sortRegisters,
                        Fetcher&& fetch, Allocator&& allocate, size_t limit)
      : arangodb::aql::SortBlock::Sorter(block, trx, buffer, sortRegisters,
                                         std::move(fetch), std::move(allocate)),
        _limit{limit},
        _cmpHeap(_trx, _sortRegisters),
        _cmpInput(_trx, _sortRegisters) {
    TRI_ASSERT(_limit > 0);
    _rows.reserve(_limit);
  }

  ~ConstrainedHeapSorter() { releaseHeapBuffer(); }

  std::pair<arangodb::aql::ExecutionState, arangodb::Result> fetch() override {
    using arangodb::aql::AqlItemBlock;
    using arangodb::aql::ExecutionBlock;
    using arangodb::aql::ExecutionState;

    ExecutionState res = ExecutionState::HASMORE;
    // suck all blocks through _buffer into heap
    while (res != ExecutionState::DONE) {
      res = _fetch(ExecutionBlock::DefaultBatchSize()).first;
      if (res == ExecutionState::WAITING) {
        return {res, TRI_ERROR_NO_ERROR};
      }

      if (!_buffer.empty()) {
        ensureHeapBuffer(_buffer.front());  // make sure we have a dst
      }
      // handle batch
      while (!_buffer.empty()) {
        std::unique_ptr<AqlItemBlock> src{_buffer.front()};
        _cmpInput.setBuffers(_heapBuffer.get(), src.get());
        _buffer.pop_front();
        for (size_t row = 0; row < src->size(); row++) {
          pushRow(src.get(), row);
        }
      }
    }

    return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
  }

  arangodb::Result sort() override {
    using arangodb::aql::AqlItemBlock;
    using arangodb::aql::AqlValue;
    using arangodb::aql::ExecutionBlock;
    using arangodb::aql::RegisterId;

    TRI_IF_FAILURE("SortBlock::doSorting") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    // make sure we don't have less than limit
    size_t total = std::min(_limit, _rowsPushed);
    uint32_t count = 0;

    // sort rows
    std::sort(_rows.begin(), _rows.end(), _cmpHeap);

    // here we collect the new blocks:
    TRI_ASSERT(_buffer.empty());
    count = 0;
    RegisterId const nrRegs = _heapBuffer->getNrRegs();
    std::unordered_map<AqlValue, AqlValue> cache;

    // install the rearranged values from _buffer into newbuffer
    while (count < total) {
      size_t sizeNext = (std::min)(total - count, ExecutionBlock::DefaultBatchSize());
      std::unique_ptr<AqlItemBlock> next(_allocate(sizeNext, nrRegs));

      TRI_IF_FAILURE("SortBlock::doSortingInner") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }

      // hand over ownership for the item block
      AqlItemBlock* p = next.get();
      _buffer.emplace_back(p);
      next.release();

      // only copy as much as needed!
      for (size_t i = 0; i < sizeNext; i++) {
        ::stealRow(cache, nrRegs, _heapBuffer.get(), _rows[count], p, i);
        count++;
      }

      cache.clear();
    }

    // pre-emptively cleanup memory from heap buffer (also done in destructor)
    releaseHeapBuffer();

    return TRI_ERROR_NO_ERROR;
  }

  bool empty() const override {
    return _buffer.empty() && _heapBuffer == nullptr;
  }

 private:
  arangodb::Result pushRow(arangodb::aql::AqlItemBlock* srcBlock, size_t sRow) {
    using arangodb::aql::AqlItemBlock;
    using arangodb::aql::AqlValue;
    using arangodb::aql::RegisterId;

    if (_rowsPushed >= _limit && _cmpInput(_rows.front(), sRow)) {
      // skip row, already too low in sort order to make it past limit
      return TRI_ERROR_NO_ERROR;
    }

    TRI_ASSERT(srcBlock != nullptr);
    AqlItemBlock* dstBlock = _heapBuffer.get();
    TRI_ASSERT(dstBlock != nullptr);
    size_t dRow = _rowsPushed;

    if (_rowsPushed >= _limit) {
      // pop an entry first
      std::pop_heap(_rows.begin(), _rows.end(), _cmpHeap);
      dRow = _rows.back();
      eraseRow(dRow);
      _rows.pop_back();
    }
    TRI_ASSERT(dRow < _limit);

    TRI_IF_FAILURE("SortBlock::doSortingInner") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    RegisterId nrRegs = srcBlock->getNrRegs();
    ::stealRowNoCache(nrRegs, srcBlock, sRow, dstBlock, dRow);
    _rows.emplace_back(dRow);
    ++_rowsPushed;

    // now insert copy into heap
    std::push_heap(_rows.begin(), _rows.end(), _cmpHeap);

    return TRI_ERROR_NO_ERROR;
  }

  void eraseRow(size_t row) {
    arangodb::aql::RegisterId const nrRegs = _heapBuffer->getNrRegs();
    for (size_t i = 0; i < nrRegs; i++) {
      _heapBuffer->destroyValue(row, i);
    }
  }

  void ensureHeapBuffer(arangodb::aql::AqlItemBlock* src) {
    TRI_ASSERT(src != nullptr);
    if (_heapBuffer == nullptr) {
      arangodb::aql::RegisterId const nrRegs = src->getNrRegs();
      _heapBuffer.reset(_allocate(_limit, nrRegs));
      _cmpHeap.setBuffers(_heapBuffer.get(), _heapBuffer.get());
    }
  }

  void releaseHeapBuffer() { _heapBuffer.reset(); }

 private:
  size_t _limit;
  size_t _rowsPushed = 0;
  std::unique_ptr<arangodb::aql::AqlItemBlock> _heapBuffer;
  std::vector<uint32_t> _rows;
  OurLessThan _cmpHeap;
  OurLessThan _cmpInput;
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

SortBlock::SortBlock(ExecutionEngine* engine, SortNode const* en,
                     SortNode::SorterType type, size_t limit)
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
      case SortNode::SorterType::Standard: {
        _sorter = std::make_unique<::StandardSorter>(*this, _trx, _buffer,
                                                     _sortRegisters, std::move(fetch),
                                                     std::move(allocate));
        break;
      }
      case SortNode::SorterType::ConstrainedHeap: {
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
