////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel Larkin
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "ConstrainedSortExecutor.h"

#include "Basics/Common.h"

#include "Aql/AqlItemBlockManager.h"
#include "Aql/AqlItemBlockShell.h"
#include "Aql/AqlItemMatrix.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SortRegister.h"
#include "Aql/Stats.h"

#include <algorithm>

using namespace arangodb;
using namespace arangodb::aql;

namespace {

template<typename tag>
struct result {
  /* export it ... */
  using type = typename tag::type;
  static type ptr;
};

template<typename tag>
typename result<tag>::type result<tag>::ptr;

template<typename tag, typename tag::type p>
struct snatch : result<tag> {
  struct filler {
    filler() { result<tag>::ptr = p; }
  };
  static filler fillerObject;
};

template<typename tag, typename tag::type p>
typename snatch<tag, p>::filler snatch<tag, p>::fillerObject;

struct InputAqlItemRowBlockFunctionExposer { typedef AqlItemBlock&(InputAqlItemRow::*type)(); };
template struct snatch<InputAqlItemRowBlockFunctionExposer, &InputAqlItemRow::block>;


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

void stealRowNoCache(InputAqlItemRow& input, arangodb::aql::AqlItemBlock& dst, size_t dRow) {
//  OutputAqlItemRow()

  RegisterId const nrRegs = input.getNrRegisters();
  for (arangodb::aql::RegisterId reg = 0; reg < nrRegs; reg++) {
  //auto const& original = input.getValue(reg);
    // If we have already dealt with this value for the next
    // block, then we just put the same value again:


    AqlItemBlock& srcSouceBLock = (input.*result<InputAqlItemRowBlockFunctionExposer>::ptr)();

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

}  // namespace
/// @brief OurLessThan
class arangodb::aql::ConstrainedLessThan {
 public:
  ConstrainedLessThan(arangodb::transaction::Methods* trx,
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
};  // ConstrainedLessThan

arangodb::Result ConstrainedSortExecutor::pushRow(InputAqlItemRow& input) {
  using arangodb::aql::AqlItemBlock;
  using arangodb::aql::AqlValue;
  using arangodb::aql::RegisterId;

  size_t dRow = _rowsPushed;

  if (_rowsPushed >= _infos._limit) {
    // pop an entry first
    std::pop_heap(_rows.begin(), _rows.end(), *_cmpHeap.get());
    dRow = _rows.back();
    // eraseRow(dRow);
    _rows.pop_back();
  }
  TRI_ASSERT(dRow < _infos._limit);

  TRI_IF_FAILURE("SortBlock::doSortingInner") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  ::stealRowNoCache(input, _heapBuffer->block(), dRow);
  _rows.emplace_back(dRow);
  ++_rowsPushed;

  // now insert copy into heap
  std::push_heap(_rows.begin(), _rows.end(), *_cmpHeap.get());

  return TRI_ERROR_NO_ERROR;
}

bool ConstrainedSortExecutor::compareInput(uint32_t const& rowPos, InputAqlItemRow& row) const {
  for (auto const& reg : _infos.sortRegisters()) {
    auto const& lhs = _heapBuffer->block().getValueReference(rowPos, reg.reg);
    auto const& rhs = row.getValue(reg.reg);

    int const cmp = arangodb::aql::AqlValue::Compare(_infos.trx(), lhs, rhs, true);

    if (cmp < 0) {
      return reg.asc;
    } else if (cmp > 0) {
      return !reg.asc;
    }
  }
  return false;
}

// class OurLessThan {
// public:
//  OurLessThan(arangodb::transaction::Methods* trx, AqlItemMatrix const& input,
//              std::vector<SortRegister> const& sortRegisters) noexcept
//      : _trx(trx), _input(input), _sortRegisters(sortRegisters) {}
//
//  bool operator()(size_t const& a, size_t const& b) const {
//    InputAqlItemRow left = _input.getRow(a);
//    InputAqlItemRow right = _input.getRow(b);
//    for (auto const& reg : _sortRegisters) {
//      AqlValue const& lhs = left.getValue(reg.reg);
//      AqlValue const& rhs = right.getValue(reg.reg);
//
//#if 0  // #ifdef USE_IRESEARCH
//      TRI_ASSERT(reg.comparator);
//      int const cmp = (*reg.comparator)(reg.scorer.get(), _trx, lhs, rhs);
//#else
//      int const cmp = AqlValue::Compare(_trx, lhs, rhs, true);
//#endif
//
//      if (cmp < 0) {
//        return reg.asc;
//      } else if (cmp > 0) {
//        return !reg.asc;
//      }
//    }
//
//    return false;
//  }
//
// private:
//  arangodb::transaction::Methods* _trx;
//  AqlItemMatrix const& _input;
//  std::vector<SortRegister> const& _sortRegisters;
//};  // OurLessThan

static std::shared_ptr<std::unordered_set<RegisterId>> mapSortRegistersToRegisterIds(
    std::vector<SortRegister> const& sortRegisters) {
  auto set = make_shared_unordered_set();
  std::transform(sortRegisters.begin(), sortRegisters.end(),
                 std::inserter(*set, set->begin()),
                 [](SortRegister const& sortReg) { return sortReg.reg; });
  return set;
}

ConstrainedSortExecutor::ConstrainedSortExecutor(Fetcher& fetcher, SortExecutorInfos& infos)
    : _infos(infos),
      _fetcher(fetcher),
      //_input(nullptr),
      _returnNext(0),
      _cmpHeap(std::make_unique<ConstrainedLessThan>(_infos.trx(), _infos.sortRegisters())) {
  TRI_ASSERT(_infos._limit > 0);
  _rows.reserve(infos._limit);
};

ConstrainedSortExecutor::~ConstrainedSortExecutor() = default;

std::pair<ExecutionState, NoStats> ConstrainedSortExecutor::produceRow(OutputAqlItemRow& output) {
  ExecutionState state = ExecutionState::HASMORE;

  if (!_outputPrepared) {
    // build-up heap by pulling form all rows in the singlerow fetcher
    state = doSorting();
  }

  if (state != ExecutionState::HASMORE) {
    TRI_ASSERT(state != ExecutionState::WAITING || state == ExecutionState::DONE);
    // assert heap empty
    return {state, NoStats{}};
  }

  // get next row from cache
  // output.copyRow() //from heap

  if (true /*cursor expired*/) {
    return {ExecutionState::DONE, NoStats{}};
  } else {
    return {ExecutionState::HASMORE, NoStats{}};
  }
}

// TRI_ASSERT(_sortedIndexes.size() == _input->size());
// if (_returnNext >= _sortedIndexes.size()) {
//   // Bail out if called too often,
//   // Bail out on no elements
//   return {ExecutionState::DONE, NoStats{}};
// }
// InputAqlItemRow inRow = _input->getRow(_sortedIndexes[_returnNext]);
// output.copyRow(inRow);
// _returnNext++;
// if (_returnNext >= _sortedIndexes.size()) {
//   return {ExecutionState::DONE, NoStats{}};
// }
// return {ExecutionState::HASMORE, NoStats{}};
//}

void ConstrainedSortExecutor::ensureHeapBuffer(InputAqlItemRow& input) {
  if (_heapBuffer == nullptr) {
    auto block = std::unique_ptr<AqlItemBlock>(
        _infos._manager.requestBlock(_infos._limit, input.getNrRegisters()));
    _heapBuffer = std::make_shared<AqlItemBlockShell>(_infos._manager, std::move(block));
    _cmpHeap->setBuffers(&_heapBuffer->block(), &_heapBuffer->block());
  }
}

ExecutionState ConstrainedSortExecutor::doSorting() {
  // pull row one by one and add to heap if fitting
  ExecutionState state = ExecutionState::HASMORE;

  TRI_IF_FAILURE("SortBlock::doSorting") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  do {
    InputAqlItemRow input(CreateInvalidInputRowHint{});
    std::tie(state, input) = _fetcher.fetchRow();
    if (state == ExecutionState::HASMORE ||
        (state == ExecutionState::DONE && input.isInitialized())) {
      ensureHeapBuffer(input);  // better outside of loop
                                // need to get nrRegs && ensure no memory
                                // is allocated when there is an immediate done

      if (_rowsPushed >= _infos._limit && compareInput(_rows.front(), input)) {
        // skip row, already too low in sort order to make it past limit
        continue;
      }
      // there is something to add to the heap
      pushRow(input);
    }

  } while (state == ExecutionState::HASMORE);

  if (state == ExecutionState::DONE) {
    _outputPrepared = true;  // we do not need to re-enter this function
  }

  return state;
}
