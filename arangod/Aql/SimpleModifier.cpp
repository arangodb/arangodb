////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "SimpleModifier.h"

#include "Aql/AqlValue.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ModificationExecutor.h"
#include "Aql/ModificationExecutorHelpers.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SharedQueryState.h"
#include "Basics/Common.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/application-exit.h"
#include "Cluster/ServerState.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Collection.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::aql::ModificationExecutorHelpers;
using namespace arangodb::basics;

template <class ModifierCompletion, typename Enable>
SimpleModifier<ModifierCompletion, Enable>::OutputIterator::OutputIterator(
    SimpleModifier<ModifierCompletion, Enable> const& modifier)
    : _modifier(modifier),
      _operationsIterator(modifier._operations.begin()),
      _resultsIterator(modifier.getResultsIterator()) {}

template <typename ModifierCompletion, typename Enable>
typename SimpleModifier<ModifierCompletion, Enable>::OutputIterator&
SimpleModifier<ModifierCompletion, Enable>::OutputIterator::next() {
  // Only move results on if there has been a document
  // submitted to the transaction
  if (_operationsIterator->first == ModifierOperationType::ReturnIfAvailable) {
    ++_resultsIterator;
  }
  ++_operationsIterator;
  return *this;
}

template <class ModifierCompletion, typename Enable>
typename SimpleModifier<ModifierCompletion, Enable>::OutputIterator&
SimpleModifier<ModifierCompletion, Enable>::OutputIterator::operator++() {
  return next();
}

template <class ModifierCompletion, typename Enable>
bool SimpleModifier<ModifierCompletion, Enable>::OutputIterator::operator!=(
    SimpleModifier<ModifierCompletion, Enable>::OutputIterator const& other) const
    noexcept {
  return _operationsIterator != other._operationsIterator;
}

template <class ModifierCompletion, typename Enable>
ModifierOutput SimpleModifier<ModifierCompletion, Enable>::OutputIterator::operator*() const {
  switch (_operationsIterator->first) {
    case ModifierOperationType::ReturnIfAvailable: {
      // This means the results slice is relevant
      if (_modifier.resultAvailable()) {
        VPackSlice elm = *_resultsIterator;
        bool error = VelocyPackHelper::getBooleanValue(elm, StaticStrings::Error, false);

        if (error) {
          return ModifierOutput{_operationsIterator->second, ModifierOutput::Type::SkipRow};
        } else {
          return ModifierOutput{
              _operationsIterator->second, ModifierOutput::Type::ReturnIfRequired,
              ModificationExecutorHelpers::getDocumentOrNull(elm, StaticStrings::Old),
              ModificationExecutorHelpers::getDocumentOrNull(elm, StaticStrings::New)};
        }
      } else {
        return ModifierOutput{_operationsIterator->second, ModifierOutput::Type::CopyRow};
      }
      case ModifierOperationType::CopyRow:
        return ModifierOutput{_operationsIterator->second, ModifierOutput::Type::CopyRow};
      case ModifierOperationType::SkipRow:
        return ModifierOutput{_operationsIterator->second, ModifierOutput::Type::SkipRow};
    }
  }

  // Shut up compiler
  TRI_ASSERT(false);
  return ModifierOutput{_operationsIterator->second, ModifierOutput::Type::SkipRow};
}

template <class ModifierCompletion, typename Enable>
typename SimpleModifier<ModifierCompletion, Enable>::OutputIterator
SimpleModifier<ModifierCompletion, Enable>::OutputIterator::begin() const {
  return SimpleModifier<ModifierCompletion, Enable>::OutputIterator(this->_modifier);
}

template <class ModifierCompletion, typename Enable>
typename SimpleModifier<ModifierCompletion, Enable>::OutputIterator
SimpleModifier<ModifierCompletion, Enable>::OutputIterator::end() const {
  auto it = SimpleModifier<ModifierCompletion, Enable>::OutputIterator(this->_modifier);
  it._operationsIterator = _modifier._operations.end();

  return it;
}

template <typename ModifierCompletion, typename Enable>
ModificationExecutorResultState SimpleModifier<ModifierCompletion, Enable>::resultState() const noexcept {
  std::lock_guard<std::mutex> guard(_resultStateMutex);
  return _resultState;
}

template <typename ModifierCompletion, typename Enable>
bool SimpleModifier<ModifierCompletion, Enable>::operationPending() const noexcept {
  switch (resultState()) {
    case ModificationExecutorResultState::NoResult:
      return false;
    case ModificationExecutorResultState::WaitingForResult:
    case ModificationExecutorResultState::HaveResult:
      return true;
  }
  LOG_TOPIC("710c4", FATAL, Logger::AQL)
      << "Invalid or unhandled value for ModificationExecutorResultState: "
      << static_cast<std::underlying_type_t<ModificationExecutorResultState>>(resultState());
  FATAL_ERROR_ABORT();
}

template <typename ModifierCompletion, typename Enable>
void SimpleModifier<ModifierCompletion, Enable>::checkException() const {
  throwOperationResultException(_infos, _results);
}

template <typename ModifierCompletion, typename Enable>
void SimpleModifier<ModifierCompletion, Enable>::resetResult() noexcept {
  std::lock_guard<std::mutex> guard(_resultStateMutex);
  _resultState = ModificationExecutorResultState::NoResult;
}

template <typename ModifierCompletion, typename Enable>
void SimpleModifier<ModifierCompletion, Enable>::reset() {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  {
    std::unique_lock<std::mutex> guard(_resultStateMutex, std::try_to_lock);
    TRI_ASSERT(guard.owns_lock());
    TRI_ASSERT(_resultState != ModificationExecutorResultState::WaitingForResult);
  }
#endif
  _accumulator.reset();
  _operations.clear();
  _results.reset();
  resetResult();
}

template <typename ModifierCompletion, typename Enable>
void SimpleModifier<ModifierCompletion, Enable>::accumulate(InputAqlItemRow& row) {
  auto result = _completion.accumulate(_accumulator, row);
  _operations.push_back({result, row});
}

template <typename ModifierCompletion, typename Enable>
ExecutionState SimpleModifier<ModifierCompletion, Enable>::transact(transaction::Methods& trx) {
  std::unique_lock<std::mutex> guard(_resultStateMutex);
  switch (_resultState) {
    case ModificationExecutorResultState::WaitingForResult:
      return ExecutionState::WAITING;
    case ModificationExecutorResultState::HaveResult:
      return ExecutionState::DONE;
    case ModificationExecutorResultState::NoResult:
      // continue
      break;
  }

  TRI_ASSERT(_resultState == ModificationExecutorResultState::NoResult);
  _resultState = ModificationExecutorResultState::NoResult;

  auto result = _completion.transact(trx, _accumulator.closeAndGetContents());

  if (result.isReady()) {
    _results = std::move(result.get());
    _resultState = ModificationExecutorResultState::HaveResult;
    return ExecutionState::DONE;
  } 

  _resultState = ModificationExecutorResultState::WaitingForResult;

  TRI_ASSERT(!ServerState::instance()->isSingleServer());
  TRI_ASSERT(_infos.engine() != nullptr);
  TRI_ASSERT(_infos.engine()->sharedState() != nullptr);

  // The guard has to be unlocked before "thenValue" is called, otherwise locking
  // the mutex there will cause a deadlock if the result is already available.
  guard.unlock();

  auto self = this->shared_from_this();
  std::move(result).thenFinal([self, sqs = _infos.engine()->sharedState()](futures::Try<OperationResult>&& opRes) {
    sqs->executeAndWakeup([&]() noexcept {
      std::unique_lock<std::mutex> guard(self->_resultStateMutex);
      try {
        TRI_ASSERT(self->_resultState == ModificationExecutorResultState::WaitingForResult);
        if (self->_resultState == ModificationExecutorResultState::WaitingForResult) {
          // get() will throw if opRes holds an exception, which is intended.
          self->_results = std::move(opRes.get());
          self->_resultState = ModificationExecutorResultState::HaveResult;
        } else {
          auto message = StringUtils::concatT(
              "Unexpected state ", to_string(self->_resultState),
              " when reporting modification result: expected ",
              to_string(ModificationExecutorResultState::WaitingForResult));
          LOG_TOPIC("1f48d", ERR, Logger::AQL) << message;
          // This can never happen. However, if it does anyway: we can't clean up
          // the query from here, so we do not wake it up and let it run into a
          // timeout instead. This should be good enough for an impossible case.
          return false;
        }
        return true;
      } catch(std::exception const& ex) {
        LOG_TOPIC("027ea", FATAL, Logger::AQL) << ex.what();
        TRI_ASSERT(false);
      } catch(...) {
        auto exptr = std::current_exception();
        TRI_ASSERT(false);
        //self->_results = exptr;
      }
    });
  });

  return ExecutionState::WAITING;
}

template <typename ModifierCompletion, typename Enable>
size_t SimpleModifier<ModifierCompletion, Enable>::nrOfOperations() const {
  return _operations.size();
}

template <typename ModifierCompletion, typename Enable>
size_t SimpleModifier<ModifierCompletion, Enable>::nrOfDocuments() const {
  return _accumulator.nrOfDocuments();
}

template <typename ModifierCompletion, typename Enable>
size_t SimpleModifier<ModifierCompletion, Enable>::nrOfResults() const {
  if (_results.hasSlice() && _results.slice().isArray()) {
    return _results.slice().length();
  }
  return 0;
}

template <typename ModifierCompletion, typename Enable>
size_t SimpleModifier<ModifierCompletion, Enable>::nrOfErrors() const {
  size_t nrOfErrors{0};

  for (auto const& pair : _results.countErrorCodes) {
    nrOfErrors += pair.second;
  }
  return nrOfErrors;
}

template <typename ModifierCompletion, typename Enable>
size_t SimpleModifier<ModifierCompletion, Enable>::nrOfWritesExecuted() const {
  return nrOfDocuments() - nrOfErrors();
}

template <typename ModifierCompletion, typename Enable>
size_t SimpleModifier<ModifierCompletion, Enable>::nrOfWritesIgnored() const {
  return nrOfErrors();
}

template <typename ModifierCompletion, typename Enable>
ModificationExecutorInfos& SimpleModifier<ModifierCompletion, Enable>::getInfos() const
    noexcept {
  return _infos;
}

template <typename ModifierCompletion, typename Enable>
size_t SimpleModifier<ModifierCompletion, Enable>::getBatchSize() const noexcept {
  return _batchSize;
}

template <typename ModifierCompletion, typename Enable>
bool SimpleModifier<ModifierCompletion, Enable>::resultAvailable() const {
  return (nrOfDocuments() > 0 && !_infos._options.silent);
}

template <typename ModifierCompletion, typename Enable>
VPackArrayIterator SimpleModifier<ModifierCompletion, Enable>::getResultsIterator() const {
  if (resultAvailable()) {
    TRI_ASSERT(_results.hasSlice() && _results.slice().isArray());
    return VPackArrayIterator{_results.slice()};
  }
  return VPackArrayIterator(VPackArrayIterator::Empty{});
}

template class ::arangodb::aql::SimpleModifier<InsertModifierCompletion>;
template class ::arangodb::aql::SimpleModifier<RemoveModifierCompletion>;
template class ::arangodb::aql::SimpleModifier<UpdateReplaceModifierCompletion>;
