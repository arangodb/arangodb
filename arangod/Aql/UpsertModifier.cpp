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

#include "UpsertModifier.h"

#include "Aql/AqlValue.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/Executor/ModificationExecutor.h"
#include "Aql/Executor/ModificationExecutorAccumulator.h"
#include "Aql/Executor/ModificationExecutorHelpers.h"
#include "Aql/QueryContext.h"
#include "Aql/SharedQueryState.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/application-exit.h"
#include "Logger/LogTopic.h"
#include "Logger/LogLevel.h"
#include "Logger/LogMacros.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

#include <absl/strings/str_cat.h>
#include <velocypack/Collection.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::aql::ModificationExecutorHelpers;
using namespace arangodb::basics;

UpsertModifier::OutputIterator::OutputIterator(UpsertModifier const& modifier)
    : _modifier(modifier),
      _operationsIterator(modifier._operations.begin()),
      _insertResultsIterator(modifier.getInsertResultsIterator()),
      _updateResultsIterator(modifier.getUpdateResultsIterator()) {}

UpsertModifier::OutputIterator& UpsertModifier::OutputIterator::next() {
  if (_operationsIterator->first ==
      UpsertModifier::OperationType::UpdateReturnIfAvailable) {
    ++_updateResultsIterator;
  } else if (_operationsIterator->first ==
             UpsertModifier::OperationType::InsertReturnIfAvailable) {
    ++_insertResultsIterator;
  }
  ++_operationsIterator;
  return *this;
}

UpsertModifier::OutputIterator& UpsertModifier::OutputIterator::operator++() {
  return next();
}

bool UpsertModifier::OutputIterator::operator!=(
    UpsertModifier::OutputIterator const& other) const noexcept {
  return _operationsIterator != other._operationsIterator;
}

ModifierOutput UpsertModifier::OutputIterator::operator*() const {
  // When we get the output of our iterator, we have to check whether the
  // operation in question was APPLY_UPDATE or APPLY_INSERT to determine which
  // of the results slices (UpdateReplace or Insert) we have to look in and
  // increment.
  if (_modifier.resultAvailable()) {
    VPackSlice elm;

    switch (_operationsIterator->first) {
      case UpsertModifier::OperationType::CopyRow:
        return ModifierOutput{_operationsIterator->second,
                              ModifierOutput::Type::CopyRow};
      case UpsertModifier::OperationType::SkipRow:
        return ModifierOutput{_operationsIterator->second,
                              ModifierOutput::Type::SkipRow};
      case UpsertModifier::OperationType::UpdateReturnIfAvailable:
        elm = *_updateResultsIterator;
        break;
      case UpsertModifier::OperationType::InsertReturnIfAvailable:
        elm = *_insertResultsIterator;
        break;
    }

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
    switch (_operationsIterator->first) {
      case UpsertModifier::OperationType::UpdateReturnIfAvailable:
      case UpsertModifier::OperationType::InsertReturnIfAvailable:
      case UpsertModifier::OperationType::CopyRow:
        return ModifierOutput{_operationsIterator->second,
                              ModifierOutput::Type::CopyRow};
      case UpsertModifier::OperationType::SkipRow:
        return ModifierOutput{_operationsIterator->second,
                              ModifierOutput::Type::SkipRow};
    }
  }

  // shut up compiler
  TRI_ASSERT(false);
  return ModifierOutput{_operationsIterator->second,
                        ModifierOutput::Type::SkipRow};
}

typename UpsertModifier::OutputIterator UpsertModifier::OutputIterator::begin()
    const {
  return UpsertModifier::OutputIterator(this->_modifier);
}

typename UpsertModifier::OutputIterator UpsertModifier::OutputIterator::end()
    const {
  auto it = UpsertModifier::OutputIterator(this->_modifier);
  it._operationsIterator = _modifier._operations.end();

  return it;
}

UpsertModifier::UpsertModifier(ModificationExecutorInfos& infos)
    : _infos(infos),
      _updateResults(Result(), infos._options),
      _insertResults(Result(), infos._options),
      // Batch size has to be 1 in case the upsert modifier sees its own
      // writes. otherwise it will use the default batching
      _batchSize(_infos._batchSize),
      _results(NoResult{}) {}

void UpsertModifier::reset() {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  {
    std::unique_lock<std::mutex> guard(_resultMutex, std::try_to_lock);
    TRI_ASSERT(guard.owns_lock());
    TRI_ASSERT(!std::holds_alternative<Waiting>(_results));
  }
#endif

  _insertAccumulator.reset();
  _insertResults.reset();
  _updateAccumulator.reset();
  _updateResults.reset();

  _operations.clear();

  resetResult();
}

void UpsertModifier::resetResult() noexcept {
  std::lock_guard<std::mutex> guard(_resultMutex);
  _results = NoResult{};
}

UpsertModifier::OperationType UpsertModifier::updateReplaceCase(
    ModificationExecutorAccumulator& accu, AqlValue const& inDoc,
    AqlValue const& updateDoc) {
  if (writeRequired(_infos, inDoc.slice(), StaticStrings::Empty)) {
    CollectionNameResolver const& collectionNameResolver{
        _infos._query.resolver()};

    // We are only interested in the key from `inDoc`
    std::string key;
    Result result = getKey(collectionNameResolver, inDoc, key);

    if (!result.ok()) {
      if (!_infos._ignoreErrors) {
        THROW_ARANGO_EXCEPTION(result);
      }
      return UpsertModifier::OperationType::SkipRow;
    }

    if (updateDoc.isObject()) {
      VPackSlice toUpdate = updateDoc.slice();
      _keyDocBuilder.clear();

      buildKeyDocument(_keyDocBuilder, key);
      auto merger = VPackCollection::merge(toUpdate, _keyDocBuilder.slice(),
                                           false, false);
      accu.add(merger.slice());

      return UpsertModifier::OperationType::UpdateReturnIfAvailable;
    } else {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID,
                                     absl::StrCat("expecting 'Object', got: ",
                                                  updateDoc.slice().typeName(),
                                                  " while handling: UPSERT"));
    }
  } else {
    return UpsertModifier::OperationType::CopyRow;
  }
}

UpsertModifier::OperationType UpsertModifier::insertCase(
    ModificationExecutorAccumulator& accu, AqlValue const& insertDoc) {
  if (insertDoc.isObject()) {
    auto const& toInsert = insertDoc.slice();
    if (writeRequired(_infos, toInsert, StaticStrings::Empty)) {
      accu.add(toInsert);
      return UpsertModifier::OperationType::InsertReturnIfAvailable;
    } else {
      return UpsertModifier::OperationType::CopyRow;
    }
  } else {
    if (!_infos._ignoreErrors) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID,
                                     std::string("expecting 'Object', got: ") +
                                         insertDoc.slice().typeName() +
                                         " while handling: UPSERT");
    }
    return UpsertModifier::OperationType::SkipRow;
  }
}

bool UpsertModifier::resultAvailable() const {
  return (nrOfDocuments() > 0 && !_infos._options.silent);
}

VPackArrayIterator UpsertModifier::getUpdateResultsIterator() const {
  if (_updateResults.hasSlice() && _updateResults.slice().isArray()) {
    return VPackArrayIterator(_updateResults.slice());
  }
  return VPackArrayIterator(VPackArrayIterator::Empty{});
}

VPackArrayIterator UpsertModifier::getInsertResultsIterator() const {
  if (_insertResults.hasSlice() && _insertResults.slice().isArray()) {
    return VPackArrayIterator(_insertResults.slice());
  }
  return VPackArrayIterator(VPackArrayIterator::Empty{});
}

void UpsertModifier::accumulate(InputAqlItemRow& row) {
  RegisterId const inDocReg = _infos._input1RegisterId;
  RegisterId const insertReg = _infos._input2RegisterId;
  RegisterId const updateReg = _infos._input3RegisterId;

  UpsertModifier::OperationType result;

  // The document to be UPSERTed
  AqlValue const& inDoc = row.getValue(inDocReg);

  // if there is a document in the input register, we
  // update that document, otherwise, we insert
  if (inDoc.isObject()) {
    auto updateDoc = row.getValue(updateReg);
    result = updateReplaceCase(_updateAccumulator, inDoc, updateDoc);
  } else {
    auto insertDoc = row.getValue(insertReg);
    result = insertCase(_insertAccumulator, insertDoc);
  }
  _operations.emplace_back(result, row);
}

ExecutionState UpsertModifier::transact(transaction::Methods& trx) {
  std::unique_lock<std::mutex> guard(_resultMutex);

  if (std::holds_alternative<Waiting>(_results)) {
    return ExecutionState::WAITING;
  } else if (std::holds_alternative<HaveResult>(_results)) {
    return ExecutionState::DONE;
  } else if (auto* ex = std::get_if<std::exception_ptr>(&_results);
             ex != nullptr) {
    std::rethrow_exception(*ex);
  } else {
    TRI_ASSERT(std::holds_alternative<NoResult>(_results));
  }

  _results = NoResult{};

  auto fut = transactInternal(trx);

  if (fut.isReady()) {
    std::move(fut).waitAndGet();
    _results = HaveResult{};
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

  std::move(fut).thenFinal([self = this->shared_from_this(),
                            sqs = _infos.engine()->sharedState()](
                               futures::Try<futures::Unit>&& tryResult) {
    sqs->executeAndWakeup([&]() noexcept {
      std::unique_lock<std::mutex> guard(self->_resultMutex);
      try {
        TRI_ASSERT(std::holds_alternative<Waiting>(self->_results));
        if (std::holds_alternative<Waiting>(self->_results)) {
          // get() will throw if opRes holds an exception, which is intended.
          std::move(tryResult).get();
          self->_results = HaveResult{};
        } else {
          // This can never happen.
          using namespace std::string_literals;
          auto state = std::visit(
              overload{[&](NoResult) { return "NoResults"s; },
                       [&](Waiting) { return "Waiting"s; },
                       [&](HaveResult) { return "Result"s; },
                       [&](std::exception_ptr const& ep) {
                         auto what = std::string{};
                         try {
                           std::rethrow_exception(ep);
                         } catch (std::exception const& ex) {
                           what = ex.what();
                         } catch (...) {
                           // Exception unknown, give up immediately.
                           LOG_TOPIC("adb0e", FATAL, Logger::AQL)
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
          LOG_TOPIC("3b0e1", ERR, Logger::AQL) << message;
          if (std::holds_alternative<std::exception_ptr>(self->_results)) {
            // Avoid overwriting an exception with another exception.
            LOG_TOPIC("78c8b", FATAL, Logger::AQL)
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

futures::Future<futures::Unit> UpsertModifier::transactInternal(
    transaction::Methods& trx) {
  auto toInsert = _insertAccumulator.closeAndGetContents();
  if (toInsert.isArray() && toInsert.length() > 0) {
    auto future = [&] {
      auto guard = std::lock_guard(_trxMutex);
      if (!_trxAlive) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
      }
      return trx.insertAsync(_infos._aqlCollection->name(), toInsert,
                             _infos._options);
    }();
    _insertResults = co_await std::move(future);
    throwOperationResultException(_infos, _insertResults);
  }

  auto toUpdate = _updateAccumulator.closeAndGetContents();
  if (toUpdate.isArray() && toUpdate.length() > 0) {
    auto future = [&] {
      auto guard = std::lock_guard(_trxMutex);
      if (!_trxAlive) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
      }
      if (_infos._isReplace) {
        return trx.replaceAsync(_infos._aqlCollection->name(), toUpdate,
                                _infos._options);
      } else {
        return trx.updateAsync(_infos._aqlCollection->name(), toUpdate,
                               _infos._options);
      }
    }();
    _updateResults = co_await std::move(future);
    throwOperationResultException(_infos, _updateResults);
  }
}

size_t UpsertModifier::nrOfDocuments() const {
  return _insertAccumulator.nrOfDocuments() +
         _updateAccumulator.nrOfDocuments();
}

size_t UpsertModifier::nrOfOperations() const { return _operations.size(); }

size_t UpsertModifier::nrOfResults() const {
  size_t n{0};

  if (_insertResults.hasSlice() && _insertResults.slice().isArray()) {
    n += _insertResults.slice().length();
  }
  if (_updateResults.hasSlice() && _updateResults.slice().isArray()) {
    n += _updateResults.slice().length();
  }
  return n;
}

size_t UpsertModifier::nrOfErrors() const {
  size_t nrOfErrors{0};

  for (auto const& pair : _insertResults.countErrorCodes) {
    nrOfErrors += pair.second;
  }
  for (auto const& pair : _updateResults.countErrorCodes) {
    nrOfErrors += pair.second;
  }
  return nrOfErrors;
}

size_t UpsertModifier::nrOfWritesExecuted() const {
  return nrOfDocuments() - nrOfErrors();
}

size_t UpsertModifier::nrOfWritesIgnored() const { return nrOfErrors(); }

size_t UpsertModifier::getBatchSize() const { return _batchSize; }

bool UpsertModifier::hasResultOrException() const noexcept {
  // Note that this is never called while the modifier is running, that's why we
  // don't need to lock _resultMutex. This way possible unintended races might
  // be revealed by TSan.
  return std::visit(overload{
                        [](NoResult) { return false; },
                        [](Waiting) { return false; },
                        [](HaveResult) { return true; },
                        [](std::exception_ptr const&) { return true; },
                    },
                    _results);
}

bool UpsertModifier::hasNeitherResultNorOperationPending() const noexcept {
  // Note that this is never called while the modifier is running, that's why we
  // don't need to lock _resultMutex. This way possible unintended races might
  // be revealed by TSan.
  return std::visit(overload{
                        [](NoResult) { return true; },
                        [](Waiting) { return false; },
                        [](HaveResult) { return false; },
                        [](std::exception_ptr const&) { return false; },
                    },
                    _results);
}

void UpsertModifier::stopAndClear() noexcept {
  _operations.clear();
  auto guard = std::lock_guard(_trxMutex);
  // should be called only once, thus...
  TRI_ASSERT(_trxAlive);
  _trxAlive = false;
}
