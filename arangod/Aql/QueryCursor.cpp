////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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
#include "Aql/ExecutionEngine.h"
#include "Aql/Query.h"
#include "Aql/QueryRegistry.h"
#include "Logger/Logger.h"
#include "RestServer/QueryRegistryFeature.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Iterator.h>
#include <velocypack/Options.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

QueryResultCursor::QueryResultCursor(
    TRI_vocbase_t& vocbase,
    CursorId id,
    aql::QueryResult&& result,
    size_t batchSize,
    double ttl,
    bool hasCount
)
    : Cursor(id, batchSize, ttl, hasCount),
      _guard(vocbase),
      _result(std::move(result)),
      _iterator(_result.result->slice()),
      _cached(_result.cached) {
  TRI_ASSERT(_result.result->slice().isArray());
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
  TRI_ASSERT(_result.result != nullptr);
  TRI_ASSERT(_iterator.valid());
  VPackSlice slice = _iterator.value();
  _iterator.next();
  return slice;
}

/// @brief return the cursor size
size_t QueryResultCursor::count() const { return _iterator.size(); }

std::pair<ExecutionState, Result> QueryResultCursor::dump(VPackBuilder& builder,
                                                          std::function<void()> const&) {
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

    VPackOptions const* oldOptions = builder.options;
    TRI_DEFER(builder.options = oldOptions);
    builder.options = _result.context->getVPackOptionsForDump();

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


QueryStreamCursor::QueryStreamCursor(
    TRI_vocbase_t& vocbase,
    CursorId id,
    std::string const& query,
    std::shared_ptr<VPackBuilder> bindVars,
    std::shared_ptr<VPackBuilder> opts,
    size_t batchSize,
    double ttl,
    bool contextOwnedByExterior)
    : Cursor(id, batchSize, ttl, /*hasCount*/ false),
      _guard(vocbase),
      _exportCount(-1),
      _queryResultPos(0) {
  auto registry = QueryRegistryFeature::registry();
  TRI_ASSERT(registry != nullptr);

  _query = std::make_unique<Query>(
    contextOwnedByExterior,
    _guard.database(),
    aql::QueryString(query),
    std::move(bindVars),
    std::move(opts),
    arangodb::aql::PART_MAIN
  );
  _query->prepare(registry);
  TRI_ASSERT(_query->state() == aql::QueryExecutionState::ValueType::EXECUTION);

  // we replaced the rocksdb export cursor with a stream AQL query
  // for this case we need to support printing the collection "count"
  if (_query->optionsSlice().hasKey("exportCollection")) {
    std::string cname = _query->optionsSlice().get("exportCollection").copyString();
    TRI_ASSERT(_query->trx()->status() == transaction::Status::RUNNING);
    OperationResult opRes = _query->trx()->count(cname, transaction::CountType::Normal);
    if (opRes.fail()) {
      THROW_ARANGO_EXCEPTION(opRes.result);
    }
    _exportCount = opRes.slice().getInt();
    VPackSlice limit = _query->bindParameters()->slice().get("limit");
    if (limit.isInteger()) {
      _exportCount = (std::min)(limit.getInt(), _exportCount);
    }
  }

  if (contextOwnedByExterior) {
    // things break if the Query outlives a V8 transaction
    _stateChangeCb = [this](transaction::Methods& trx,
                            transaction::Status status) {
      if ((status == transaction::Status::COMMITTED ||
          status == transaction::Status::ABORTED) &&
          !this->isUsed()) {
        this->setDeleted();
      }
    };
    if (!_query->trx()->addStatusChangeCallback(&_stateChangeCb)) {
      _stateChangeCb = nullptr;
    }
  }
}

QueryStreamCursor::~QueryStreamCursor() {
  if (_query) {  // cursor is canceled or timed-out
    cleanupStateCallback();
    
    while (!_queryResults.empty()) {
      _query->engine()->_itemBlockManager.returnBlock(std::move(_queryResults.front()));
      _queryResults.pop_front();
    }
    
    // now remove the continue handler we may have registered in the query
    _query->sharedState()->setContinueCallback();
    // Query destructor will cleanup plan and abort transaction
    _query.reset();
  }
}

void QueryStreamCursor::kill() {
  if (_query) {
    _query->kill();
  }
}

std::pair<ExecutionState, Result> QueryStreamCursor::dump(VPackBuilder& builder,
                                                          std::function<void()> const& ch) {
  TRI_ASSERT(batchSize() > 0);
  LOG_TOPIC(TRACE, Logger::QUERIES) << "executing query " << _id << ": '"
                                    << _query->queryString().extract(1024) << "'";

  // We will get a different RestHandler on every dump, so we need to update the Callback
  std::shared_ptr<SharedQueryState> ss = _query->sharedState();
  ss->setContinueHandler(ch);

  try {
    ExecutionState state = prepareDump();

    if (state == ExecutionState::WAITING) {
      return  {ExecutionState::WAITING, TRI_ERROR_NO_ERROR};
    }

    Result res = writeResult(builder);
    if (!res.ok()) {
      return {ExecutionState::DONE, res};
    }
    return {state, res};
  } catch (arangodb::basics::Exception const& ex) {
    this->setDeleted();
    return {ExecutionState::DONE, Result(ex.code(),
                  "AQL: " + ex.message() +
                      QueryExecutionState::toStringWithPrefix(_query->state()))};
  } catch (std::bad_alloc const&) {
    this->setDeleted();
    return {ExecutionState::DONE, Result(TRI_ERROR_OUT_OF_MEMORY,
                  TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY) +
                      QueryExecutionState::toStringWithPrefix(_query->state()))};
  } catch (std::exception const& ex) {
    this->setDeleted();
    return {ExecutionState::DONE, Result(
        TRI_ERROR_INTERNAL,
        ex.what() + QueryExecutionState::toStringWithPrefix(_query->state()))};
  } catch (...) {
    this->setDeleted();
    return {ExecutionState::DONE, Result(TRI_ERROR_INTERNAL,
                  TRI_errno_string(TRI_ERROR_INTERNAL) +
                      QueryExecutionState::toStringWithPrefix(_query->state()))};
  }
}

Result QueryStreamCursor::dumpSync(VPackBuilder& builder) {
  TRI_ASSERT(batchSize() > 0);
  LOG_TOPIC(TRACE, Logger::QUERIES) << "executing query " << _id << ": '"
                                    << _query->queryString().extract(1024) << "'";

  std::shared_ptr<SharedQueryState> ss = _query->sharedState();
  // We will get a different RestHandler on every dump, so we need to update the Callback
  ss->setContinueCallback();

  try {
    aql::ExecutionEngine* engine = _query->engine();
    TRI_ASSERT(engine != nullptr);

    std::unique_ptr<AqlItemBlock> value;

    ExecutionState state = ExecutionState::WAITING;

    while (state == ExecutionState::WAITING) {
      state = prepareDump();
      if (state == ExecutionState::WAITING) {
        ss->waitForAsyncResponse();
      }
    }

    return writeResult(builder);
  } catch (arangodb::basics::Exception const& ex) {
    this->setDeleted();
    return Result(ex.code(),
                  "AQL: " + ex.message() +
                      QueryExecutionState::toStringWithPrefix(_query->state()));
  } catch (std::bad_alloc const&) {
    this->setDeleted();
    return Result(TRI_ERROR_OUT_OF_MEMORY,
                  TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY) +
                      QueryExecutionState::toStringWithPrefix(_query->state()));
  } catch (std::exception const& ex) {
    this->setDeleted();
    return Result(
        TRI_ERROR_INTERNAL,
        ex.what() + QueryExecutionState::toStringWithPrefix(_query->state()));
  } catch (...) {
    this->setDeleted();
    return Result(TRI_ERROR_INTERNAL,
                  TRI_errno_string(TRI_ERROR_INTERNAL) +
                      QueryExecutionState::toStringWithPrefix(_query->state()));
  }
}

Result QueryStreamCursor::writeResult(VPackBuilder &builder) {
  try {
    VPackOptions const* oldOptions = builder.options;
    TRI_DEFER(builder.options = oldOptions);
    VPackOptions options = VPackOptions::Defaults;
    options.buildUnindexedArrays = true;
    options.buildUnindexedObjects = true;
    options.escapeUnicode = true;
    builder.options = &options;
    // reserve some space in Builder to avoid frequent reallocs
    builder.reserve(16 * 1024);
    builder.add("result", VPackValue(VPackValueType::Array, true));

    aql::ExecutionEngine* engine = _query->engine();
    TRI_ASSERT(engine != nullptr);
    // this is the RegisterId our results can be found in
    RegisterId const resultRegister = engine->resultRegister();

    size_t rowsWritten = 0;
    while(rowsWritten < batchSize() && !_queryResults.empty()) {
      std::unique_ptr<AqlItemBlock>& block = _queryResults.front();
      TRI_ASSERT(_queryResultPos < block->size());

      while (rowsWritten < batchSize() && _queryResultPos < block->size()) {
        AqlValue const& value = block->getValueReference(_queryResultPos, resultRegister);
        if (!value.isEmpty()) { // ignore empty blocks (e.g. from UpdateBlock)
          value.toVelocyPack(_query->trx(), builder, false);
          ++rowsWritten;
        }
        ++_queryResultPos;
      }

      if (_queryResultPos == block->size()) {
        // get next block
        TRI_ASSERT(_queryResultPos == block->size());
        engine->_itemBlockManager.returnBlock(std::move(block));
        _queryResults.pop_front();
        _queryResultPos = 0;
      }
    }

    TRI_ASSERT(_queryResults.empty() ||
               _queryResultPos < _queryResults.front()->size());

    builder.close();  // result

    // If there is a block left, there's at least one row left in it. On the
    // other hand, we rely on the caller to have fetched more than batchSize()
    // result rows if possible!
    bool hasMore = !_queryResults.empty();

    builder.add("hasMore", VPackValue(hasMore));
    if (hasMore) {
      builder.add("id", VPackValue(std::to_string(id())));
    }
    if (_exportCount >= 0) { // this is coming from /_api/export
      builder.add("count", VPackValue(_exportCount));
    }
    builder.add("cached", VPackValue(false));

    if (!hasMore) {
      std::shared_ptr<SharedQueryState> ss = _query->sharedState();
      ss->setContinueCallback();
      
      // cleanup before transaction is committet
      cleanupStateCallback();

      QueryResult result;
      ExecutionState state = _query->finalize(result); // will commit transaction
      while (state == ExecutionState::WAITING) {
        ss->waitForAsyncResponse();
        state = _query->finalize(result);
      }
      if (result.extra && result.extra->slice().isObject()) {
        builder.add("extra", result.extra->slice());
      }
      _query.reset();
      TRI_ASSERT(_queryResults.empty());
      this->setDeleted();
    }
  } catch (arangodb::basics::Exception const& ex) {
    this->setDeleted();
    return Result(ex.code(),
                  "AQL: " + ex.message() +
                      QueryExecutionState::toStringWithPrefix(_query->state()));
  } catch (std::bad_alloc const&) {
    this->setDeleted();
    return Result(TRI_ERROR_OUT_OF_MEMORY,
                  TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY) +
                      QueryExecutionState::toStringWithPrefix(_query->state()));
  } catch (std::exception const& ex) {
    this->setDeleted();
    return Result(
        TRI_ERROR_INTERNAL,
        ex.what() + QueryExecutionState::toStringWithPrefix(_query->state()));
  } catch (...) {
    this->setDeleted();
    return Result(TRI_ERROR_INTERNAL,
                  TRI_errno_string(TRI_ERROR_INTERNAL) +
                      QueryExecutionState::toStringWithPrefix(_query->state()));
  }

  return TRI_ERROR_NO_ERROR;
}


std::shared_ptr<transaction::Context> QueryStreamCursor::context() const {
  return _query->trx()->transactionContext();
}

ExecutionState QueryStreamCursor::prepareDump() {
  aql::ExecutionEngine* engine = _query->engine();
  TRI_ASSERT(engine != nullptr);

  size_t numBufferedRows = 0;
  for (auto const& it : _queryResults) {
    numBufferedRows += it->size();
  }
  numBufferedRows -= _queryResultPos;

  ExecutionState state = ExecutionState::HASMORE;
  // We want to fill a result of batchSize if possible and have at least
  // one row left (or be definitively DONE) to set "hasMore" reliably.
  while (state != ExecutionState::DONE && numBufferedRows <= batchSize()) {
    std::unique_ptr<AqlItemBlock> resultBlock;
    std::tie(state, resultBlock) = engine->getSome(batchSize());
    if (state == ExecutionState::WAITING) {
      break;
    }
    // resultBlock == nullptr => ExecutionState::DONE
    TRI_ASSERT(resultBlock != nullptr || state == ExecutionState::DONE);

    if (resultBlock != nullptr) {
      numBufferedRows += resultBlock->size();
      _queryResults.push_back(std::move(resultBlock));
    }
  }

  if (state == ExecutionState::WAITING) {
    // TODO add all results to the builder before going to sleep
  }

  return state;
}

void QueryStreamCursor::cleanupStateCallback() {
  TRI_ASSERT(_query);
  transaction::Methods* trx = _query->trx();
  if (trx && _stateChangeCb) {
    trx->removeStatusChangeCallback(&_stateChangeCb);
  }
}
