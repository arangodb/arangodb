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

#ifndef ARANGOD_AQL_QUERY_CURSOR_H
#define ARANGOD_AQL_QUERY_CURSOR_H 1

#include "Aql/QueryResult.h"
#include "Aql/SharedAqlItemBlockPtr.h"
#include "Basics/Common.h"
#include "Transaction/Methods.h"
#include "Utils/Cursor.h"
#include "VocBase/vocbase.h"

#include <deque>

namespace arangodb {
namespace aql {

class AqlItemBlock;
enum class ExecutionState;
class Query;
class SharedAqlItemBlockPtr;

/// Cursor managing an entire query result in-memory
/// Should be used in conjunction with the RestCursorHandler
class QueryResultCursor final : public arangodb::Cursor {
 public:
  QueryResultCursor(TRI_vocbase_t& vocbase, aql::QueryResult&& result,
                    size_t batchSize, double ttl, bool hasCount);

  ~QueryResultCursor() = default;

  aql::QueryResult const* result() const { return &_result; }

  bool hasNext();

  arangodb::velocypack::Slice next();

  size_t count() const override final;

  std::pair<aql::ExecutionState, Result> dump(velocypack::Builder& result) override final;

  Result dumpSync(velocypack::Builder& result) override final;

  std::shared_ptr<transaction::Context> context() const override final {
    return _result.context;
  }

  /// @brief Returns a slice to read the extra values.
  /// Make sure the Cursor Object is not destroyed while reading this slice.
  /// If no extras are set this will return a NONE slice.
  arangodb::velocypack::Slice extra() const;

 private:
  DatabaseGuard _guard;
  aql::QueryResult _result;
  arangodb::velocypack::ArrayIterator _iterator;
  bool _cached;
};

/// Cursor managing a query from which it continuously gets
/// new results. Query, transaction and locks will live until
/// cursor is deleted (or query exhausted)
class QueryStreamCursor final : public arangodb::Cursor {
 public:
  QueryStreamCursor(std::unique_ptr<aql::Query> q, size_t batchSize, double ttl);

  ~QueryStreamCursor();

  void kill() override;

  size_t count() const override final { return 0; }

  std::pair<ExecutionState, Result> dump(velocypack::Builder& result) override final;

  Result dumpSync(velocypack::Builder& result) override final;
  
  /// Set wakeup callback on streaming cursor
  void setWakeupHandler(std::function<bool()> const& cb) override final;
  void resetWakeupHandler() override final;

  std::shared_ptr<transaction::Context> context() const override final;

 private:
  // Writes from _queryResults to builder. Removes copied blocks from
  // _queryResults and sets _queryResultPos appropriately. Relies on the caller
  // to have fetched more than batchSize() result rows (if possible) in order to
  // set hasMore reliably.
  Result writeResult(velocypack::Builder& builder);

  ExecutionState prepareDump();

  void cleanupStateCallback();
  void cleanupResources();

 private:
  std::deque<SharedAqlItemBlockPtr> _queryResults; /// buffered results
  std::shared_ptr<transaction::Context> _ctx; /// cache context
  std::unique_ptr<aql::Query> _query;

  /// index of the next to-be-returned row in _queryResults.front()
  size_t _queryResultPos;
  int64_t _exportCount;  // used by RocksDBRestExportHandler (<0 is not used)
  /// used when cursor is owned by V8 transaction
  transaction::Methods::StatusChangeCallback _stateChangeCb;
};

}  // namespace aql
}  // namespace arangodb

#endif
