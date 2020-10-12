////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "QueryCursor.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlItemBlockSerializationFormat.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/Query.h"
#include "Aql/QueryRegistry.h"
#include "Basics/ScopeGuard.h"
#include "Logger/LogMacros.h"
#include "RestServer/QueryRegistryFeature.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Iterator.h>
#include <velocypack/Options.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

QueryResultCursor::QueryResultCursor(TRI_vocbase_t& vocbase, aql::QueryResult&& result,
                                     size_t batchSize, double ttl, bool hasCount)
    : Cursor(TRI_NewServerSpecificTick(), batchSize, ttl, hasCount),
      _guard(vocbase),
      _result(std::move(result)),
      _iterator(_result.data->slice()),
      _cached(_result.cached) {
  TRI_ASSERT(_result.data->slice().isArray());
}

VPackSlice QueryResultCursor::extra() const {
  if (_result.extra) {
    _result.extra->slice();
  }
  return {};
}

/// @brief check whether the cursor contains more data
bool QueryResultCursor::hasNext() {
  if (_iterator.valid()) {
    return true;
  }

  _isDeleted = true;
  return false;
}

/// @brief return the next element
VPackSlice QueryResultCursor::next() {
  TRI_ASSERT(_result.data != nullptr);
  TRI_ASSERT(_iterator.valid());
  VPackSlice slice = _iterator.value();
  _iterator.next();
  return slice;
}

/// @brief return the cursor size
size_t QueryResultCursor::count() const { return _iterator.size(); }

std::pair<ExecutionState, Result> QueryResultCursor::dump(VPackBuilder& builder) {
  // This cursor cannot block, result already there.
  auto res = dumpSync(builder);
  return {ExecutionState::DONE, res};
}

Result QueryResultCursor::dumpSync(VPackBuilder& builder) {
  try {
    size_t const n = batchSize();
    // reserve an arbitrary number of bytes for the result to save
    // some reallocs
    // (not accurate, but the actual size is unknown anyway)
    builder.reserve(std::max<size_t>(1, std::min<size_t>(n, 10000)) * 32);
    builder.add("result", VPackValue(VPackValueType::Array));
    for (size_t i = 0; i < n; ++i) {
      if (!hasNext()) {
        break;
      }
      builder.add(next());
    }
    builder.close();

    builder.add("hasMore", VPackValue(hasNext()));

    if (hasNext()) {
      builder.add("id", VPackValue(std::to_string(id())));
    }

    if (hasCount()) {
      builder.add("count", VPackValue(static_cast<uint64_t>(count())));
    }

    if (_result.extra != nullptr) {
      builder.add("extra", _result.extra->slice());
    }

    builder.add("cached", VPackValue(_cached));

    if (!hasNext()) {
      // mark the cursor as deleted
      this->setDeleted();
    }
  } catch (arangodb::basics::Exception const& ex) {
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL,
                  "internal error during QueryResultCursor::dump");
  }
  return {TRI_ERROR_NO_ERROR};
}

// .............................................................................
// QueryStreamCursor class
// .............................................................................

QueryStreamCursor::QueryStreamCursor(std::unique_ptr<arangodb::aql::Query> q,
                                     size_t batchSize, double ttl)
    : Cursor(TRI_NewServerSpecificTick(), batchSize, ttl, /*hasCount*/ false),
      _query(std::move(q)),
      _queryResultPos(0),
      _exportCount(-1),
      _finalization(false) {

  _query->prepareQuery(SerializationFormat::SHADOWROWS);
  TRI_ASSERT(_query->state() == aql::QueryExecutionState::ValueType::EXECUTION);
  _ctx = _query->newTrxContext();
  
  transaction::Methods trx(_ctx);
  TRI_ASSERT(trx.status() == transaction::Status::RUNNING);

  // we replaced the rocksdb export cursor with a stream AQL query
  // for this case we need to support printing the collection "count"
  // this is a hack for the export API only
  auto const& exportCollection = _query->queryOptions().exportCollection;
  if (!exportCollection.empty()) {
    OperationOptions opOptions(ExecContext::current());
    OperationResult opRes =
        trx.count(exportCollection, transaction::CountType::Normal, opOptions);
    if (opRes.fail()) {
      THROW_ARANGO_EXCEPTION(opRes.result);
    }
    _exportCount = opRes.slice().getInt();
    VPackSlice limit = _query->bindParameters()->slice().get("limit");
    if (limit.isInteger()) {
      _exportCount = (std::min)(limit.getInt(), _exportCount);
    }
  }
   
  // ensures the cursor is cleaned up as soon as the outer transaction ends
  // otherwise we just get issues because we might still try to use the trx
  TRI_ASSERT(trx.status() == transaction::Status::RUNNING);
  // things break if the Query outlives a V8 transaction
  _stateChangeCb = [this](transaction::Methods& /*trx*/, transaction::Status status) {
    if (status == transaction::Status::COMMITTED ||
        status == transaction::Status::ABORTED) {
      this->setDeleted();
    }
  };
  if (!trx.addStatusChangeCallback(&_stateChangeCb)) {
    _stateChangeCb = nullptr;
  }
}

QueryStreamCursor::~QueryStreamCursor() {
  if (!_query) {
    return;
  }

  _queryResults.clear();
  
  cleanupStateCallback();
  
  // Query destructor will cleanup plan and abort transaction
  _query.reset();
}

void QueryStreamCursor::kill() {
  if (_query) {
    _query->kill();
  }
}

std::pair<ExecutionState, Result> QueryStreamCursor::dump(VPackBuilder& builder) {
  TRI_ASSERT(batchSize() > 0);
  LOG_TOPIC("9af59", TRACE, Logger::QUERIES)
    << "executing query " << _id << ": '"
    << _query->queryString().extract(1024) << "'";

  try {
    ExecutionState state = prepareDump();
    if (state == ExecutionState::WAITING) {
      return {ExecutionState::WAITING, TRI_ERROR_NO_ERROR};
    }

    // writeResult will perform async finalize
    return {writeResult(builder), TRI_ERROR_NO_ERROR};
  } catch (arangodb::basics::Exception const& ex) {
    this->setDeleted();
    return {ExecutionState::DONE,
            Result(ex.code(), "AQL: " + ex.message() +
                                  QueryExecutionState::toStringWithPrefix(_query->state()))};
  } catch (std::bad_alloc const&) {
    this->setDeleted();
    return {ExecutionState::DONE,
            Result(TRI_ERROR_OUT_OF_MEMORY,
                   TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY) +
                       QueryExecutionState::toStringWithPrefix(_query->state()))};
  } catch (std::exception const& ex) {
    this->setDeleted();
    return {ExecutionState::DONE,
            Result(TRI_ERROR_INTERNAL,
                   ex.what() + QueryExecutionState::toStringWithPrefix(_query->state()))};
  } catch (...) {
    this->setDeleted();
    return {ExecutionState::DONE,
            Result(TRI_ERROR_INTERNAL,
                   TRI_errno_string(TRI_ERROR_INTERNAL) +
                       QueryExecutionState::toStringWithPrefix(_query->state()))};
  }
}

Result QueryStreamCursor::dumpSync(VPackBuilder& builder) {
  TRI_ASSERT(batchSize() > 0);
  LOG_TOPIC("9dada", TRACE, Logger::QUERIES)
      << "executing query " << _id << ": '"
      << _query->queryString().extract(1024) << "'";

  std::shared_ptr<SharedQueryState> ss = _query->sharedState();
  ss->resetWakeupHandler();

  try {
    aql::ExecutionEngine* engine = _query->rootEngine();
    TRI_ASSERT(engine != nullptr);

    SharedAqlItemBlockPtr value;

    ExecutionState state = ExecutionState::WAITING;
    while (state == ExecutionState::WAITING) {
      state = prepareDump();
      if (state == ExecutionState::WAITING) {
        ss->waitForAsyncWakeup();
      }
    }

    state = ExecutionState::WAITING;
    while (state == ExecutionState::WAITING) {
      state = writeResult(builder);
      if (state == ExecutionState::WAITING) {
        ss->waitForAsyncWakeup();
      }
    }
    
  } catch (arangodb::basics::Exception const& ex) {
    this->setDeleted();
    return Result(ex.code(), "AQL: " + ex.message() +
                                 QueryExecutionState::toStringWithPrefix(_query->state()));
  } catch (std::bad_alloc const&) {
    this->setDeleted();
    return Result(TRI_ERROR_OUT_OF_MEMORY,
                  TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY) +
                      QueryExecutionState::toStringWithPrefix(_query->state()));
  } catch (std::exception const& ex) {
    this->setDeleted();
    return Result(TRI_ERROR_INTERNAL,
                  ex.what() + QueryExecutionState::toStringWithPrefix(_query->state()));
  } catch (...) {
    this->setDeleted();
    return Result(TRI_ERROR_INTERNAL,
                  TRI_errno_string(TRI_ERROR_INTERNAL) +
                      QueryExecutionState::toStringWithPrefix(_query->state()));
  }
  
  return Result();
}

/// Set wakeup callback on streaming cursor
void QueryStreamCursor::setWakeupHandler(std::function<bool()> const& cb) {
  if (_query) {
    _query->sharedState()->setWakeupHandler(cb);
  }
}

void QueryStreamCursor::resetWakeupHandler() {
  if (_query) {
    _query->sharedState()->resetWakeupHandler();
  }
}

ExecutionState QueryStreamCursor::writeResult(VPackBuilder& builder) {

  // reserve some space in Builder to avoid frequent reallocs
  builder.reserve(16 * 1024);
  
  builder.add("result", VPackValue(VPackValueType::Array, true));

  aql::ExecutionEngine* engine = _query->rootEngine();
  TRI_ASSERT(engine != nullptr);
  // this is the RegisterId our results can be found in
  RegisterId const resultRegister = engine->resultRegister();

  size_t rowsWritten = 0;
  auto const& vopts = _query->vpackOptions();
  while (rowsWritten < batchSize() && !_queryResults.empty()) {
    SharedAqlItemBlockPtr& block = _queryResults.front();
    TRI_ASSERT(_queryResultPos < block->numRows());

    while (rowsWritten < batchSize() && _queryResultPos < block->numRows()) {
      AqlValue const& value = block->getValueReference(_queryResultPos, resultRegister);
      if (!value.isEmpty()) {  // ignore empty blocks (e.g. from UpdateBlock)
        value.toVelocyPack(&vopts, builder, /*resolveExternals*/false,
                           /*allowUnindexed*/true);
        ++rowsWritten;
      }
      ++_queryResultPos;
    }

    if (_queryResultPos == block->numRows()) {
      // get next block
      TRI_ASSERT(_queryResultPos == block->numRows());
      _queryResults.pop_front();
      _queryResultPos = 0;
    }
  }

  TRI_ASSERT(_queryResults.empty() || _queryResultPos < _queryResults.front()->numRows());

  builder.close();  // result
  
  // If there is a block left, there's at least one row left in it. On the
  // other hand, we rely on the caller to have fetched more than batchSize()
  // result rows if possible!
  const bool hasMore = !_queryResults.empty();

  builder.add("hasMore", VPackValue(hasMore));
  if (hasMore) {
    builder.add("id", VPackValue(std::to_string(id())));
  }
  if (_exportCount >= 0) {  // this is coming from /_api/export
    builder.add("count", VPackValue(_exportCount));
  }
  builder.add("cached", VPackValue(false));

  if (!hasMore) {
    TRI_ASSERT(!_extrasBuffer.empty());
    builder.add("extra", VPackSlice(_extrasBuffer.data()));
    _query.reset();
    this->setDeleted();
    return ExecutionState::DONE;
  }

  return ExecutionState::HASMORE;
}

std::shared_ptr<transaction::Context> QueryStreamCursor::context() const {
  TRI_ASSERT(_ctx != nullptr);
  return _ctx;
}

ExecutionState QueryStreamCursor::prepareDump() {
  if (_finalization) {
    return finalization();
  }
  
  aql::ExecutionEngine* engine = _query->rootEngine();
  TRI_ASSERT(engine != nullptr);

  size_t numRows = 0;
  for (auto const& it : _queryResults) {
    numRows += it->numEffectiveRows();
  }
  numRows -= _queryResultPos;
  
  // We want to fill a result of batchSize if possible and have at least
  // one row left (or be definitively DONE) to set "hasMore" reliably.
  ExecutionState state = ExecutionState::HASMORE;
  auto guard = scopeGuard([&] {
    if (_query) {
      _query->exitV8Context();
    }
  });
  
  while (state != ExecutionState::DONE && numRows <= batchSize()) {
    SharedAqlItemBlockPtr resultBlock;
    std::tie(state, resultBlock) = engine->getSome(batchSize());
    if (state == ExecutionState::WAITING) {
      break;
    }
    // resultBlock == nullptr => ExecutionState::DONE
    TRI_ASSERT(resultBlock != nullptr || state == ExecutionState::DONE);

    if (resultBlock != nullptr) {
      numRows += resultBlock->numEffectiveRows();
      _queryResults.push_back(std::move(resultBlock));
    }
  }

  // remember not to call getSome() again
  _finalization = (state == ExecutionState::DONE);
  if (_finalization) {
    cleanupStateCallback();
    return finalization();
  }

  return state;
}

ExecutionState QueryStreamCursor::finalization() {
  ExecutionState state = ExecutionState::DONE;
  if (_extrasBuffer.empty()) {
    VPackBuilder b(_extrasBuffer);
    // finalize(..) will commit transaction
    if ((state = _query->finalize(b)) == ExecutionState::WAITING) {
      return ExecutionState::WAITING;
    }
    TRI_ASSERT(b.slice().isObject());
  }
  TRI_ASSERT(state == ExecutionState::DONE);
  return state;
}

void QueryStreamCursor::cleanupStateCallback() {
  TRI_ASSERT(_query);
  transaction::Methods trx(_ctx);
  if (_stateChangeCb && trx.status() == transaction::Status::RUNNING) {
    trx.removeStatusChangeCallback(&_stateChangeCb);
    _stateChangeCb = nullptr;
  }
}
