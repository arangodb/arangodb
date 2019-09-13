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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "SortExecutor.h"

#include "Basics/Common.h"
#include "VocBase/LogicalCollection.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SortRegister.h"
#include "Aql/Stats.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"

#include <algorithm>

using namespace arangodb;
using namespace arangodb::aql;

namespace {

/// @brief OurLessThan
class OurLessThan {
 public:
  OurLessThan(arangodb::transaction::Methods* trx, AqlItemMatrix const& input,
              std::vector<SortRegister> const& sortRegisters) noexcept
      : _trx(trx), _input(input), _sortRegisters(sortRegisters) {}

  bool operator()(AqlItemMatrix::RowIndex const& a, AqlItemMatrix::RowIndex const& b) const {
    InputAqlItemRow left = _input.getRow(a);
    InputAqlItemRow right = _input.getRow(b);
    for (auto const& reg : _sortRegisters) {
      AqlValue const& lhs = left.getValue(reg.reg);
      AqlValue const& rhs = right.getValue(reg.reg);

      int const cmp = AqlValue::Compare(_trx, lhs, rhs, true);

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
  AqlItemMatrix const& _input;
  std::vector<SortRegister> const& _sortRegisters;
};  // OurLessThan

}  // namespace

void CopyRowProducer::outputRow(const InputAqlItemRow & input, OutputAqlItemRow & output) {
  output.copyRow(input);
}

void MaterializerProducer::outputRow(const InputAqlItemRow & input, OutputAqlItemRow & output) {
  auto collection =
    reinterpret_cast<arangodb::LogicalCollection const*>(
      input.getValue(
        _readDocumentContext._infos->inputNonMaterializedColRegId()).toInt64());
  TRI_ASSERT(collection != nullptr);
  _readDocumentContext._inputRow = &input;
  _readDocumentContext._outputRow = &output;
  collection->readDocumentWithCallback(_readDocumentContext._infos->trx(),
    LocalDocumentId(input.getValue(_readDocumentContext._infos->inputNonMaterializedDocRegId()).toInt64()),
    _readDocumentContext._callback);
}

static std::shared_ptr<std::unordered_set<RegisterId>> mapSortRegistersToRegisterIds(
    std::vector<SortRegister> const& sortRegisters) {
  auto set = make_shared_unordered_set();
  std::transform(sortRegisters.begin(), sortRegisters.end(),
                 std::inserter(*set, set->begin()),
                 [](SortRegister const& sortReg) { return sortReg.reg; });
  return set;
}

SortExecutorInfos::SortExecutorInfos(
    // cppcheck-suppress passedByValue
    std::vector<SortRegister> sortRegisters, std::size_t limit,
    AqlItemBlockManager& manager, RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
    // cppcheck-suppress passedByValue
    std::unordered_set<RegisterId> registersToClear,
    // cppcheck-suppress passedByValue
    std::unordered_set<RegisterId> registersToKeep, transaction::Methods* trx, bool stable,
    std::shared_ptr<std::unordered_set<RegisterId>> outputRegisters)
    : ExecutorInfos(mapSortRegistersToRegisterIds(sortRegisters), outputRegisters,
                    nrInputRegisters, nrOutputRegisters,
                    std::move(registersToClear), std::move(registersToKeep)),
      _limit(limit),
      _manager(manager),
      _trx(trx),
      _sortRegisters(std::move(sortRegisters)),
      _stable(stable) {
  TRI_ASSERT(trx != nullptr);
  TRI_ASSERT(!_sortRegisters.empty());
}

transaction::Methods* SortExecutorInfos::trx() const { return _trx; }

std::vector<SortRegister>& SortExecutorInfos::sortRegisters() {
  return _sortRegisters;
}

bool SortExecutorInfos::stable() const { return _stable; }

SortMaterializingExecutorInfos::SortMaterializingExecutorInfos(
    std::vector<SortRegister> sortRegisters, std::size_t limit, AqlItemBlockManager & manager,
    RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
    const std::unordered_set<RegisterId>& registersToClear,
    const std::unordered_set<RegisterId>& registersToKeep, transaction::Methods * trx,
    bool stable, RegisterId inNonMaterializedColRegId, RegisterId inNonMaterializedDocRegId,
    RegisterId outMaterializedDocumentRegId)
  : SortExecutorInfos(std::move(sortRegisters), limit, manager, nrInputRegisters, nrOutputRegisters,
    registersToClear, registersToKeep, trx, stable,
    std::make_shared<std::unordered_set<RegisterId>>(std::initializer_list<RegisterId>({outMaterializedDocumentRegId}))),
  _inNonMaterializedColRegId(inNonMaterializedColRegId),
  _inNonMaterializedDocRegId(inNonMaterializedDocRegId),
  _outMaterializedDocumentRegId(outMaterializedDocumentRegId) {}

template<typename OutputRowImpl>
SortExecutor<OutputRowImpl>::SortExecutor(Fetcher& fetcher, Infos& infos)
    : _infos(infos), _fetcher(fetcher), _input(nullptr), _returnNext(0), _outputImpl(_infos) {}

template<typename OutputRowImpl>
SortExecutor<OutputRowImpl>::~SortExecutor() = default;

template<typename OutputRowImpl>
std::pair<ExecutionState, NoStats> SortExecutor<OutputRowImpl>::produceRows(OutputAqlItemRow& output) {
  if (_input == nullptr) {
    auto fetchRes = consumeInput();
    if (fetchRes.first == ExecutionState::WAITING) {
      return fetchRes;
    }
  }
  // If we get here we have an input matrix
  // And we have a list of sorted indexes.
  TRI_ASSERT(_input != nullptr);
  TRI_ASSERT(_sortedIndexes.size() == _input->size());
  if (_returnNext >= _sortedIndexes.size()) {
    // Bail out if called too often,
    // Bail out on no elements
    return {ExecutionState::DONE, NoStats{}};
  }
  InputAqlItemRow inRow = _input->getRow(_sortedIndexes[_returnNext]);
  _outputImpl.outputRow(inRow, output);
  _returnNext++;
  if (_returnNext >= _sortedIndexes.size()) {
    return {ExecutionState::DONE, NoStats{}};
  }
  return {ExecutionState::HASMORE, NoStats{}};
}

template<typename OutputRowImpl>
std::pair<ExecutionState, NoStats> arangodb::aql::SortExecutor<OutputRowImpl>::consumeInput() {
  ExecutionState state;
  // We need to get data
  std::tie(state, _input) = _fetcher.fetchAllRows();
  if (state == ExecutionState::WAITING) {
    return { state, NoStats{} };
  }
  // If the execution state was not waiting it is guaranteed that we get a
  // matrix. Maybe empty still
  TRI_ASSERT(_input != nullptr);
  if (_input == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  // After allRows the dependency has to be done
  TRI_ASSERT(state == ExecutionState::DONE);

  // Execute the sort
  doSorting();
  return { state, NoStats{} };
}

template<typename OutputRowImpl>
void SortExecutor<OutputRowImpl>::doSorting() {
  TRI_IF_FAILURE("SortBlock::doSorting") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  TRI_ASSERT(_input != nullptr);
  _sortedIndexes = _input->produceRowIndexes();
  // comparison function
  OurLessThan ourLessThan(_infos.trx(), *_input, _infos.sortRegisters());
  if (_infos.stable()) {
    std::stable_sort(_sortedIndexes.begin(), _sortedIndexes.end(), ourLessThan);
  } else {
    std::sort(_sortedIndexes.begin(), _sortedIndexes.end(), ourLessThan);
  }
}

template<typename OutputRowImpl>
std::pair<ExecutionState, size_t> SortExecutor<OutputRowImpl>::expectedNumberOfRows(size_t atMost) const {
  if (_input == nullptr) {
    // This executor does not know anything yet.
    // Just take whatever is presented from upstream.
    // This will return WAITING a couple of times
    return _fetcher.preFetchNumberOfRows(atMost);
  }
  TRI_ASSERT(_returnNext <= _sortedIndexes.size());
  size_t rowsLeft = _sortedIndexes.size() - _returnNext;
  if (rowsLeft > 0) {
    return {ExecutionState::HASMORE, rowsLeft};
  }
  return {ExecutionState::DONE, rowsLeft};
}

template<typename OutputRowImpl>
std::tuple<ExecutionState, NoStats, size_t> SortExecutor<OutputRowImpl>::skipRows(size_t toSkip) {
  if (_input == nullptr) {
    auto fetchRes = consumeInput();
    if (fetchRes.first == ExecutionState::WAITING) {
      return std::make_tuple(fetchRes.first, fetchRes.second, 0);
    }
  }
  const size_t skipped = std::min(toSkip, _sortedIndexes.size() - _returnNext);
  _returnNext += skipped;
  return std::make_tuple(
    _returnNext >= _sortedIndexes.size() ? ExecutionState::DONE : ExecutionState::HASMORE,
    NoStats(), skipped);
}


arangodb::IndexIterator::DocumentCallback MaterializerProducer::ReadContext::copyDocumentCallback(ReadContext & ctx) {
  auto* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine);
  typedef std::function<arangodb::IndexIterator::DocumentCallback(ReadContext&)> CallbackFactory;
  static CallbackFactory const callbackFactories[]{
    [](ReadContext& ctx) {
    // capture only one reference to potentially avoid heap allocation
    return [&ctx](LocalDocumentId /*id*/, VPackSlice doc) {
      TRI_ASSERT(ctx._outputRow);
      TRI_ASSERT(ctx._inputRow);
      TRI_ASSERT(ctx._inputRow->isInitialized());
      TRI_ASSERT(ctx._infos);
      arangodb::aql::AqlValue a{ arangodb::aql::AqlValueHintCopy(doc.begin()) };
      bool mustDestroy = true;
      arangodb::aql::AqlValueGuard guard{ a, mustDestroy };
      ctx._outputRow->moveValueInto(ctx._infos->outputMaterializedDocumentRegId(), *ctx._inputRow, guard);
    };
  },

    [](ReadContext& ctx) {
    // capture only one reference to potentially avoid heap allocation
    return [&ctx](LocalDocumentId /*id*/, VPackSlice doc) {
      TRI_ASSERT(ctx._outputRow);
      TRI_ASSERT(ctx._inputRow);
      TRI_ASSERT(ctx._inputRow->isInitialized());
      TRI_ASSERT(ctx._infos);
      arangodb::aql::AqlValue a{ arangodb::aql::AqlValueHintDocumentNoCopy(doc.begin()) };
      bool mustDestroy = true;
      arangodb::aql::AqlValueGuard guard{ a, mustDestroy };
      ctx._outputRow->moveValueInto(ctx._infos->outputMaterializedDocumentRegId(), *ctx._inputRow, guard);
    };
  } };
  return callbackFactories[size_t(engine->useRawDocumentPointers())](ctx);
}

template class arangodb::aql::SortExecutor<arangodb::aql::CopyRowProducer>;
template class arangodb::aql::SortExecutor<arangodb::aql::MaterializerProducer>;


