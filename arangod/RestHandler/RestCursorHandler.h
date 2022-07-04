////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/QueryResult.h"
#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "RestHandler/RestVocbaseBaseHandler.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include "Scheduler/Scheduler.h"

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack
namespace aql {
class Query;
class QueryRegistry;
struct QueryResult;
}  // namespace aql

class Cursor;

////////////////////////////////////////////////////////////////////////////////
/// @brief cursor request handler
////////////////////////////////////////////////////////////////////////////////

class RestCursorHandler : public RestVocbaseBaseHandler {
 public:
  RestCursorHandler(ArangodServer&, GeneralRequest*, GeneralResponse*,
                    arangodb::aql::QueryRegistry*);

  ~RestCursorHandler();

 public:
  virtual RestStatus execute() override;
  char const* name() const override { return "RestCursorHandler"; }
  RequestLane lane() const override final { return RequestLane::CLIENT_AQL; }

  virtual RestStatus continueExecute() override;
  void shutdownExecute(bool isFinalized) noexcept override;

  void cancel() override final;

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief register the query either as streaming cursor or in _query
  /// the query is not executed here.
  /// this method is also used by derived classes
  //////////////////////////////////////////////////////////////////////////////

  RestStatus registerQueryOrCursor(arangodb::velocypack::Slice const& body);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Process the query registered in _query.
  /// The function is repeatable, so whenever we need to WAIT
  /// in AQL we can post a handler calling this function again.
  //////////////////////////////////////////////////////////////////////////////

  RestStatus processQuery();

  /// @brief returns the short id of the server which should handle this request
  ResultT<std::pair<std::string, bool>> forwardingTarget() override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief unregister the currently running query
  //////////////////////////////////////////////////////////////////////////////

  void unregisterQuery() noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief handle the result returned by the query. This function is
  /// guaranteed
  ///        to not be interrupted and is guaranteed to get a complete
  ///        queryResult.
  //////////////////////////////////////////////////////////////////////////////
  virtual RestStatus handleQueryResult(bool allowDirtyReads);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not the query was canceled
  //////////////////////////////////////////////////////////////////////////////

  bool wasCanceled();

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief register the currently running query
  //////////////////////////////////////////////////////////////////////////////

  void registerQuery(std::shared_ptr<arangodb::aql::Query> query);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief cancel the currently running query
  //////////////////////////////////////////////////////////////////////////////

  void cancelQuery();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief build options for the query as JSON
  ///        Will fill the _options Builder
  //////////////////////////////////////////////////////////////////////////////

  void buildOptions(arangodb::velocypack::Slice const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief append the contents of the cursor into the response body
  /// this function will also take care of the cursor and return it to the
  /// registry if required
  //////////////////////////////////////////////////////////////////////////////

  RestStatus generateCursorResult(rest::ResponseCode code);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief create a cursor and return the first results
  //////////////////////////////////////////////////////////////////////////////

  RestStatus createQueryCursor();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the next results from an existing cursor
  //////////////////////////////////////////////////////////////////////////////

  RestStatus modifyQueryCursor();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief dispose an existing cursor
  //////////////////////////////////////////////////////////////////////////////

  RestStatus deleteQueryCursor();

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief currently running query
  //////////////////////////////////////////////////////////////////////////////

  std::shared_ptr<arangodb::aql::Query> _query;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Reference to a queryResult, which is reused after waiting.
  //////////////////////////////////////////////////////////////////////////////

  aql::QueryResult _queryResult;

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief our query registry
  //////////////////////////////////////////////////////////////////////////////

  arangodb::aql::QueryRegistry* _queryRegistry;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief leased query cursor, may be set by query continuation
  //////////////////////////////////////////////////////////////////////////////
  Cursor* _cursor;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief lock for currently running query
  //////////////////////////////////////////////////////////////////////////////

  Mutex _queryLock;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not the query has already started executing
  //////////////////////////////////////////////////////////////////////////////

  bool _hasStarted;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not the query was killed
  //////////////////////////////////////////////////////////////////////////////

  bool _queryKilled;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief A shared pointer to the query options velocypack, s.t. we avoid
  ///        to reparse and set default options
  //////////////////////////////////////////////////////////////////////////////
  std::shared_ptr<arangodb::velocypack::Builder> _options;
};
}  // namespace arangodb
