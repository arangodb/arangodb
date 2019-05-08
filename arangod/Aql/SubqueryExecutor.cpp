////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#include "ExecutionBlock.h"
#include "SubqueryExecutor.h"

#include "Aql/OutputAqlItemRow.h"
#include "Aql/SingleRowFetcher.h"

using namespace arangodb;
using namespace arangodb::aql;

SubqueryExecutorInfos::SubqueryExecutorInfos(
    std::shared_ptr<std::unordered_set<RegisterId>> readableInputRegisters,
    std::shared_ptr<std::unordered_set<RegisterId>> writeableOutputRegisters,
    RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
    std::unordered_set<RegisterId> const& registersToClear,
    std::unordered_set<RegisterId>&& registersToKeep, ExecutionBlock& subQuery,
    RegisterId outReg, bool subqueryIsConst)
    : ExecutorInfos(readableInputRegisters, writeableOutputRegisters, nrInputRegisters,
                    nrOutputRegisters, registersToClear, std::move(registersToKeep)),
      _subQuery(subQuery),
      _outReg(outReg),
      _returnsData(subQuery.getPlanNode()->getType() == ExecutionNode::RETURN),
      _isConst(subqueryIsConst) {}

SubqueryExecutorInfos::SubqueryExecutorInfos(SubqueryExecutorInfos&& other) = default;

SubqueryExecutorInfos::~SubqueryExecutorInfos() = default;

template<bool isModificationSubquery>
SubqueryExecutor<isModificationSubquery>::SubqueryExecutor(Fetcher& fetcher, SubqueryExecutorInfos& infos)
    : _fetcher(fetcher),
      _infos(infos),
      _state(ExecutionState::HASMORE),
      _subqueryInitialized(false),
      _shutdownDone(false),
      _shutdownResult(TRI_ERROR_INTERNAL),
      _subquery(infos.getSubquery()),
      _subqueryResults(nullptr),
      _input(CreateInvalidInputRowHint{}) {}

template<bool isModificationSubquery>
SubqueryExecutor<isModificationSubquery>::~SubqueryExecutor() = default;

/**
 * This follows the following state machine:
 * If we have a subquery ongoing we need to ask it for hasMore, until it is DONE.
 * In the case of DONE we write the result, and remove it from ongoing.
 * If we do not have a subquery ongoing, we fetch a row and we start a new Subquery and ask it for hasMore.
 */

template<bool isModificationSubquery>
std::pair<ExecutionState, NoStats> SubqueryExecutor<isModificationSubquery>::produceRows(OutputAqlItemRow& output) {
  if (_state == ExecutionState::DONE && !_input.isInitialized()) {
    // We have seen DONE upstream, and we have discarded our local reference
    // to the last input, we will not be able to produce results anymore.
    return {_state, NoStats{}};
  }
  while (true) {
    if (_subqueryInitialized) {
      // Continue in subquery

      // Const case
      if (_infos.isConst() && !_input.isFirstRowInBlock()) {
        // Simply write
        writeOutput(output);
        return {_state, NoStats{}};
      }

      // Non const case, or first run in const
      auto res = _subquery.getSome(ExecutionBlock::DefaultBatchSize());
      if (res.first == ExecutionState::WAITING) {
        TRI_ASSERT(res.second == nullptr);
        return {res.first, NoStats{}};
      }
      // We get a result
      if (res.second != nullptr) {
        TRI_IF_FAILURE("SubqueryBlock::executeSubquery") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }

        if (_infos.returnsData()) {
          TRI_ASSERT(_subqueryResults != nullptr);
          _subqueryResults->emplace_back(std::move(res.second));
        }
      }

      // Subquery DONE
      if (res.first == ExecutionState::DONE) {
        writeOutput(output);
        return {_state, NoStats{}};
      }

    } else {
      // init new subquery
      if (!_input) {
        std::tie(_state, _input) = _fetcher.fetchRow();
        if (_state == ExecutionState::WAITING) {
          TRI_ASSERT(!_input);
          return {_state, NoStats{}};
        }
        if (!_input) {
          TRI_ASSERT(_state == ExecutionState::DONE);

          // We are done!
          return {_state, NoStats{}};
        }
      }

      TRI_ASSERT(_input);
      if (!_infos.isConst() || _input.isFirstRowInBlock()) {
        auto initRes = _subquery.initializeCursor(_input);
        if (initRes.first == ExecutionState::WAITING) {
          return {ExecutionState::WAITING, NoStats{}};
        }
        if (initRes.second.fail()) {
          // Error during initialize cursor
          THROW_ARANGO_EXCEPTION(initRes.second);
        }
        _subqueryResults = std::make_unique<std::vector<SharedAqlItemBlockPtr>>();
      }
      // on const subquery we can retoggle init as soon as we have new input.
      _subqueryInitialized = true;
    }
  }
}

template<bool isModificationSubquery>
void SubqueryExecutor<isModificationSubquery>::writeOutput(OutputAqlItemRow& output) {
  _subqueryInitialized = false;
  TRI_IF_FAILURE("SubqueryBlock::getSome") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  if (!_infos.isConst() || _input.isFirstRowInBlock()) {
    // In the non const case, or if we are the first row.
    // We need to copy the data, and hand over ownership
    TRI_ASSERT(_subqueryResults != nullptr);

    // We asser !returnsData => _subqueryResults is empty
    TRI_ASSERT(_infos.returnsData() || _subqueryResults->empty());
    AqlValue resultDocVec{_subqueryResults.get()};
    AqlValueGuard guard{resultDocVec, true};
    // Responsibility is handed over
    _subqueryResults.release();
    output.moveValueInto(_infos.outputRegister(), _input, guard);
    TRI_ASSERT(_subqueryResults == nullptr);
  } else {
    // In this case we can simply reference the last written value
    // We are not responsible for anything ourselfes anymore
    TRI_ASSERT(_subqueryResults == nullptr);
    bool didReuse = output.reuseLastStoredValue(_infos.outputRegister(), _input);
    TRI_ASSERT(didReuse);
  }
  _input = InputAqlItemRow(CreateInvalidInputRowHint{});
  TRI_ASSERT(output.produced());
}

/// @brief shutdown, tell dependency and the subquery
template<bool isModificationSubquery>
std::pair<ExecutionState, Result> SubqueryExecutor<isModificationSubquery>::shutdown(int errorCode) {
  // Note this shutdown needs to be repeatable.
  // Also note the ordering of this shutdown is different
  // from earlier versions we now shutdown subquery first
  if (!_shutdownDone) {
    // We take ownership of _state here for shutdown state
    std::tie(_state, _shutdownResult) = _subquery.shutdown(errorCode);
    if (_state == ExecutionState::WAITING) {
      TRI_ASSERT(_shutdownResult.ok());
      return {ExecutionState::WAITING, TRI_ERROR_NO_ERROR};
    }
    _shutdownDone = true;
  }
  return {_state, _shutdownResult};
}

template class ::arangodb::aql::SubqueryExecutor<true>;
template class ::arangodb::aql::SubqueryExecutor<false>;
