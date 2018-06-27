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
#include "Cluster/CollectionLockState.h"
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
  return VPackSlice();
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

Result QueryResultCursor::dump(VPackBuilder& builder) {
  try {
    size_t const n = batchSize();
    // reserve an arbitrary number of bytes for the result to save
    // some reallocs
    // (not accurate, but the actual size is unknown anyway)
    builder.reserve(std::max<size_t>(1, std::min<size_t>(n, 10000)) * 32);

    VPackOptions const* oldOptions = builder.options;
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

    if (_result.extra.get() != nullptr) {
      builder.add("extra", _result.extra->slice());
    }

    builder.add("cached", VPackValue(_cached));

    if (!hasNext()) {
      // mark the cursor as deleted
      this->deleted();
    }
    builder.options = oldOptions;
  } catch (arangodb::basics::Exception const& ex) {
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL,
                  "internal error during QueryResultCursor::dump");
  }
  return TRI_ERROR_NO_ERROR;
}

QueryStreamCursor::QueryStreamCursor(
    TRI_vocbase_t& vocbase,
    CursorId id,
    std::string const& query,
    std::shared_ptr<VPackBuilder> bindVars,
    std::shared_ptr<VPackBuilder> opts,
    size_t batchSize,
    double ttl
)
    : Cursor(id, batchSize, ttl, /*hasCount*/ false),
      _guard(vocbase),
      _exportCount(-1) {
  TRI_ASSERT(QueryRegistryFeature::QUERY_REGISTRY != nullptr);
  auto prevLockHeaders = CollectionLockState::_noLockHeaders;
  TRI_DEFER(CollectionLockState::_noLockHeaders = prevLockHeaders);

  _query = std::make_unique<Query>(
    false,
    _guard.database(),
    aql::QueryString(query),
    std::move(bindVars),
    std::move(opts),
    arangodb::aql::PART_MAIN
  );
  _query->prepare(QueryRegistryFeature::QUERY_REGISTRY, aql::Query::DontCache);
  TRI_ASSERT(_query->state() == aql::QueryExecutionState::ValueType::EXECUTION);
        
  // we replaced the rocksdb export cursor with a stream AQL query
  // for this case we need to support printing the collection "count"
  if (_query->optionsSlice().hasKey("exportCollection")) {
    std::string cname = _query->optionsSlice().get("exportCollection").copyString();
    TRI_ASSERT(_query->trx()->status() == transaction::Status::RUNNING);
    OperationResult opRes = _query->trx()->count(cname, true);
    if (opRes.fail()) {
      THROW_ARANGO_EXCEPTION(opRes.result);
    }
    _exportCount = opRes.slice().getInt();
    VPackSlice limit = _query->bindParameters()->slice().get("limit");
    if (limit.isInteger()) {
      _exportCount = std::min(limit.getInt(), _exportCount);
    }
  }
  
  // If we have set _noLockHeaders, we need to unset it:
  if (CollectionLockState::_noLockHeaders != nullptr &&
      CollectionLockState::_noLockHeaders == _query->engine()->lockedShards()) {
    CollectionLockState::_noLockHeaders = nullptr;
  }
}

QueryStreamCursor::~QueryStreamCursor() {
  if (_query) { // cursor is canceled or timed-out
    auto prevLockHeaders = CollectionLockState::_noLockHeaders;
    CollectionLockState::_noLockHeaders = _query->engine()->lockedShards();
    TRI_DEFER(CollectionLockState::_noLockHeaders = prevLockHeaders);
    /*QueryResult result;
    _query->finalize(result);*/
    // Query destructor will  cleanup plan and abort transaction
    _query.reset();
  }
}

Result QueryStreamCursor::dump(VPackBuilder& builder) {
  TRI_ASSERT(batchSize() > 0);
  auto prevLockHeaders = CollectionLockState::_noLockHeaders;
  // If we had set _noLockHeaders, we need to reset it:
  CollectionLockState::_noLockHeaders = _query->engine()->lockedShards();
  TRI_DEFER(CollectionLockState::_noLockHeaders = prevLockHeaders);

  LOG_TOPIC(TRACE, Logger::QUERIES) << "executing query " << _id << ": '"
                                    << _query->queryString().extract(1024) << "'";

  VPackOptions const* oldOptions = builder.options;
  TRI_DEFER(builder.options = oldOptions);
  VPackOptions options = VPackOptions::Defaults;
  options.buildUnindexedArrays = true;
  options.buildUnindexedObjects = true;
  options.escapeUnicode = true;
  builder.options = &options;

  try {
    aql::ExecutionEngine* engine = _query->engine();
    TRI_ASSERT(engine != nullptr);

    // this is the RegisterId our results can be found in
    RegisterId const resultRegister = engine->resultRegister();
    std::unique_ptr<AqlItemBlock> value;

    bool hasMore = false;
    bool done = false;

    // reserve some space in Builder to avoid frequent reallocs
    builder.reserve(16 * 1024);
    builder.add("result", VPackValue(VPackValueType::Array, true));
    while (true) {
      // TODO MAX: We need to let the thread sleep here instead of while loop
      auto res = engine->getSome(batchSize());
      if (res.first == ExecutionState::WAITING) {
        _query->tempWaitForAsyncResponse();
      } else {
        if (res.first == ExecutionState::DONE) {
          done = true;
        }
        value.swap(res.second);
        break;
      }
    }

    // get one batch
    if (value != nullptr) {
      size_t const n = value->size();
      for (size_t i = 0; i < n; ++i) {
        AqlValue const& val = value->getValueReference(i, resultRegister);

        if (!val.isEmpty()) {
          val.toVelocyPack(_query->trx(), builder, false);
        }
      }
      // return used block: this will reset value to a nullptr
      engine->_itemBlockManager.returnBlock(std::move(value)); 
      if (!done) {
        // TODO MAX: We need to use hasMoreState() here instead, and sleep if
        // it returns WAITING
        hasMore = engine->hasMoreSync();
      }
    }
    builder.close();  // result

    builder.add("hasMore", VPackValue(hasMore));
    if (hasMore) {
      builder.add("id", VPackValue(std::to_string(id())));
    }
    if (_exportCount >= 0) { // this is coming from /_api/export
      builder.add("count", VPackValue(_exportCount));
    }
    builder.add("cached", VPackValue(false));

    if (!hasMore) {
      QueryResult result;
      _query->finalize(result); // will commit transaction
      if (result.extra && result.extra->slice().isObject()) {
        builder.add("extra", result.extra->slice());
      }
      _query.reset();
      this->deleted();
    }
  } catch (arangodb::basics::Exception const& ex) {
    this->deleted();
    return Result(ex.code(),
                  "AQL: " + ex.message() +
                      QueryExecutionState::toStringWithPrefix(_query->state()));
  } catch (std::bad_alloc const&) {
    this->deleted();
    return Result(TRI_ERROR_OUT_OF_MEMORY,
                  TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY) +
                      QueryExecutionState::toStringWithPrefix(_query->state()));
  } catch (std::exception const& ex) {
    this->deleted();
    return Result(
        TRI_ERROR_INTERNAL,
        ex.what() + QueryExecutionState::toStringWithPrefix(_query->state()));
  } catch (...) {
    this->deleted();
    return Result(TRI_ERROR_INTERNAL,
                  TRI_errno_string(TRI_ERROR_INTERNAL) +
                      QueryExecutionState::toStringWithPrefix(_query->state()));
  }

  return TRI_ERROR_NO_ERROR;
}


std::shared_ptr<transaction::Context> QueryStreamCursor::context() const {
  return _query->trx()->transactionContext();
}
