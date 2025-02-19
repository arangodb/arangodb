////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "Aql/Executor/ModificationExecutor.h"
#include "Aql/Executor/ModificationExecutorHelpers.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SharedQueryState.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/application-exit.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Collection.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::aql::ModificationExecutorHelpers;
using namespace arangodb::basics;

template<class ModifierCompletion, typename Enable>
SimpleModifier<ModifierCompletion, Enable>::OutputIterator::OutputIterator(
    SimpleModifier<ModifierCompletion, Enable> const& modifier)
    : _modifier(modifier),
      _operationsIterator(modifier._operations.begin()),
      _resultsIterator(modifier.getResultsIterator()) {}

template<typename ModifierCompletion, typename Enable>
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

template<class ModifierCompletion, typename Enable>
typename SimpleModifier<ModifierCompletion, Enable>::OutputIterator&
SimpleModifier<ModifierCompletion, Enable>::OutputIterator::operator++() {
  return next();
}

template<class ModifierCompletion, typename Enable>
bool SimpleModifier<ModifierCompletion, Enable>::OutputIterator::operator!=(
    SimpleModifier<ModifierCompletion, Enable>::OutputIterator const& other)
    const noexcept {
  return _operationsIterator != other._operationsIterator;
}

template<class ModifierCompletion, typename Enable>
ModifierOutput
SimpleModifier<ModifierCompletion, Enable>::OutputIterator::operator*() const {
  switch (_operationsIterator->first) {
    case ModifierOperationType::ReturnIfAvailable: {
      // This means the results slice is relevant
      if (_modifier.resultAvailable()) {
        VPackSlice elm = *_resultsIterator;
        bool error =
            VelocyPackHelper::getBooleanValue(elm, StaticStrings::Error, false);

        if (error) {
          return ModifierOutput{_operationsIterator->second,
                                ModifierOutput::Type::SkipRow};
        } else {
          return ModifierOutput{_operationsIterator->second,
                                ModifierOutput::Type::ReturnIfRequired,
                                ModificationExecutorHelpers::getDocumentOrNull(
                                    elm, StaticStrings::Old),
                                ModificationExecutorHelpers::getDocumentOrNull(
                                    elm, StaticStrings::New)};
        }
      } else {
        return ModifierOutput{_operationsIterator->second,
                              ModifierOutput::Type::CopyRow};
      }
      case ModifierOperationType::CopyRow:
        return ModifierOutput{_operationsIterator->second,
                              ModifierOutput::Type::CopyRow};
      case ModifierOperationType::SkipRow:
        return ModifierOutput{_operationsIterator->second,
                              ModifierOutput::Type::SkipRow};
    }
  }

  // Shut up compiler
  TRI_ASSERT(false);
  return ModifierOutput{_operationsIterator->second,
                        ModifierOutput::Type::SkipRow};
}

template<class ModifierCompletion, typename Enable>
typename SimpleModifier<ModifierCompletion, Enable>::OutputIterator
SimpleModifier<ModifierCompletion, Enable>::OutputIterator::begin() const {
  return SimpleModifier<ModifierCompletion, Enable>::OutputIterator(
      this->_modifier);
}

template<class ModifierCompletion, typename Enable>
typename SimpleModifier<ModifierCompletion, Enable>::OutputIterator
SimpleModifier<ModifierCompletion, Enable>::OutputIterator::end() const {
  auto it = SimpleModifier<ModifierCompletion, Enable>::OutputIterator(
      this->_modifier);
  it._operationsIterator = _modifier._operations.end();

  return it;
}

template<typename ModifierCompletion, typename Enable>
void SimpleModifier<ModifierCompletion, Enable>::checkException() const {
  throwOperationResultException(_infos, std::get<OperationResult>(_results));
}

template<typename ModifierCompletion, typename Enable>
void SimpleModifier<ModifierCompletion, Enable>::resetResult() noexcept {
  std::lock_guard<std::mutex> guard(_resultMutex);
  _results = NoResult{};
}

template<typename ModifierCompletion, typename Enable>
void SimpleModifier<ModifierCompletion, Enable>::reset() {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  {
    std::unique_lock<std::mutex> guard(_resultMutex, std::try_to_lock);
    TRI_ASSERT(guard.owns_lock());
    TRI_ASSERT(!std::holds_alternative<Waiting>(_results));
  }
#endif
  _accumulator.reset();
  _operations.clear();
  resetResult();
}

template<typename ModifierCompletion, typename Enable>
void SimpleModifier<ModifierCompletion, Enable>::accumulate(
    InputAqlItemRow& row) {
  auto result = _completion.accumulate(_accumulator, row);
  _operations.push_back({result, row});
}

template<typename ModifierCompletion, typename Enable>
ExecutionState SimpleModifier<ModifierCompletion, Enable>::transact(
    transaction::Methods& trx) {
  std::unique_lock<std::mutex> guard(_resultMutex);
  if (std::holds_alternative<Waiting>(_results)) {
    return ExecutionState::WAITING;
  } else if (std::holds_alternative<OperationResult>(_results)) {
    return ExecutionState::DONE;
  } else if (auto* ex = std::get_if<std::exception_ptr>(&_results);
             ex != nullptr) {
    std::rethrow_exception(*ex);
  } else {
    TRI_ASSERT(std::holds_alternative<NoResult>(_results));
  }

  _results = NoResult{};

  auto result = _completion.transact(trx, _accumulator.closeAndGetContents());

  if (result.isReady()) {
    _results = std::move(result.waitAndGet());
    return ExecutionState::DONE;
  }

  _results = Waiting{};

  TRI_ASSERT(!ServerState::instance()->isSingleServer());
  TRI_ASSERT(_infos.engine() != nullptr);
  TRI_ASSERT(_infos.engine()->sharedState() != nullptr);

  // The guard has to be unlocked before "thenValue" is called, otherwise
  // locking the mutex there will cause a deadlock if the result is already
  // available.
  guard.unlock();

  std::move(result).thenFinal([self = this->shared_from_this(),
                               sqs = _infos.engine()->sharedState()](
                                  futures::Try<OperationResult>&& opRes) {
    sqs->executeAndWakeup([&]() noexcept {
      std::unique_lock<std::mutex> guard(self->_resultMutex);
      try {
        TRI_ASSERT(std::holds_alternative<Waiting>(self->_results));
        if (std::holds_alternative<Waiting>(self->_results)) {
          // get() will throw if opRes holds an exception, which is intended.
          self->_results = std::move(opRes.get());
        } else {
          // This can never happen.
          using namespace std::string_literals;
          auto state = std::visit(
              overload{[&](NoResult) { return "NoResults"s; },
                       [&](Waiting) { return "Waiting"s; },
                       [&](OperationResult const&) { return "Result"s; },
                       [&](std::exception_ptr const& ep) {
                         auto what = std::string{};
                         try {
                           std::rethrow_exception(ep);
                         } catch (std::exception const& ex) {
                           what = ex.what();
                         } catch (...) {
                           // Exception unknown, give up immediately.
                           LOG_TOPIC("4646a", FATAL, Logger::AQL)
                               << "Caught an exception while handling another "
                                  "one, giving up.";
                           FATAL_ERROR_ABORT();
                         }
                         return StringUtils::concatT("Exception: ", what);
                       }},
              self->_results);
          auto message = StringUtils::concatT(
              "Unexpected state when reporting modification result, expected "
              "'Waiting' but got: ",
              state);
          LOG_TOPIC("1f48d", ERR, Logger::AQL) << message;
          if (std::holds_alternative<std::exception_ptr>(self->_results)) {
            // Avoid overwriting an exception with another exception.
            LOG_TOPIC("2d310", FATAL, Logger::AQL)
                << "Caught an exception while handling another one, giving up.";
            FATAL_ERROR_ABORT();
          }
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL_AQL,
                                         std::move(message));
        }
      } catch (...) {
        auto exptr = std::current_exception();
        self->_results = exptr;
      }
      return true;
    });
  });

  return ExecutionState::WAITING;
}

template<typename ModifierCompletion, typename Enable>
size_t SimpleModifier<ModifierCompletion, Enable>::nrOfOperations() const {
  return _operations.size();
}

template<typename ModifierCompletion, typename Enable>
size_t SimpleModifier<ModifierCompletion, Enable>::nrOfDocuments() const {
  return _accumulator.nrOfDocuments();
}

template<typename ModifierCompletion, typename Enable>
size_t SimpleModifier<ModifierCompletion, Enable>::nrOfResults() const {
  if (auto* res = std::get_if<OperationResult>(&_results);
      res != nullptr && res->hasSlice() && res->slice().isArray()) {
    return res->slice().length();
  } else {
    return 0;
  }
}

template<typename ModifierCompletion, typename Enable>
size_t SimpleModifier<ModifierCompletion, Enable>::nrOfErrors() const {
  size_t nrOfErrors{0};

  for (auto const& pair : std::get<OperationResult>(_results).countErrorCodes) {
    nrOfErrors += pair.second;
  }
  return nrOfErrors;
}

template<typename ModifierCompletion, typename Enable>
size_t SimpleModifier<ModifierCompletion, Enable>::nrOfWritesExecuted() const {
  return nrOfDocuments() - nrOfErrors();
}

template<typename ModifierCompletion, typename Enable>
size_t SimpleModifier<ModifierCompletion, Enable>::nrOfWritesIgnored() const {
  return nrOfErrors();
}

template<typename ModifierCompletion, typename Enable>
ModificationExecutorInfos&
SimpleModifier<ModifierCompletion, Enable>::getInfos() const noexcept {
  return _infos;
}

template<typename ModifierCompletion, typename Enable>
size_t SimpleModifier<ModifierCompletion, Enable>::getBatchSize()
    const noexcept {
  return _batchSize;
}

template<typename ModifierCompletion, typename Enable>
bool SimpleModifier<ModifierCompletion, Enable>::resultAvailable() const {
  return (nrOfDocuments() > 0 && !_infos._options.silent);
}

template<typename ModifierCompletion, typename Enable>
VPackArrayIterator
SimpleModifier<ModifierCompletion, Enable>::getResultsIterator() const {
  if (resultAvailable()) {
    auto const& results = std::get<OperationResult>(_results);
    TRI_ASSERT(results.hasSlice() && results.slice().isArray());
    return VPackArrayIterator{results.slice()};
  }
  return VPackArrayIterator(VPackArrayIterator::Empty{});
}

template<typename ModifierCompletion, typename Enable>
bool SimpleModifier<ModifierCompletion, Enable>::hasResultOrException()
    const noexcept {
  // Note that this is never called while the modifier is running, that's why we
  // don't need to lock _resultMutex. This way possible unintended races might
  // be revealed by TSan.
  return std::visit(overload{
                        [](NoResult) { return false; },
                        [](Waiting) { return false; },
                        [](OperationResult const&) { return true; },
                        [](std::exception_ptr const&) { return true; },
                    },
                    _results);
}

template<typename ModifierCompletion, typename Enable>
bool SimpleModifier<ModifierCompletion,
                    Enable>::hasNeitherResultNorOperationPending()
    const noexcept {
  // Note that this is never called while the modifier is running, that's why we
  // don't need to lock _resultMutex. This way possible unintended races might
  // be revealed by TSan.
  return std::visit(overload{
                        [](NoResult) { return true; },
                        [](Waiting) { return false; },
                        [](OperationResult const&) { return false; },
                        [](std::exception_ptr const&) { return false; },
                    },
                    _results);
}

template<typename ModifierCompletion, typename Enable>
void SimpleModifier<ModifierCompletion, Enable>::stopAndClear() noexcept {
  _operations.clear();
}

template class ::arangodb::aql::SimpleModifier<InsertModifierCompletion>;
template class ::arangodb::aql::SimpleModifier<RemoveModifierCompletion>;
template class ::arangodb::aql::SimpleModifier<UpdateReplaceModifierCompletion>;
