////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_REST_HANDLER_REST_CURSOR_HANDLER_H
#define ARANGOD_REST_HANDLER_REST_CURSOR_HANDLER_H 1

#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Aql/QueryResult.h"
#include "RestHandler/RestVocbaseBaseHandler.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}
namespace aql {
class Query;
class QueryRegistry;
}

class ApplicationV8;
class Cursor;

////////////////////////////////////////////////////////////////////////////////
/// @brief cursor request handler
////////////////////////////////////////////////////////////////////////////////

class RestCursorHandler : public RestVocbaseBaseHandler {
 public:
  RestCursorHandler(
      rest::GeneralRequest*,
      std::pair<arangodb::ApplicationV8*, arangodb::aql::QueryRegistry*>*);

 public:
  virtual status_t execute() override;

  bool cancel() override;

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief processes the query and returns the results/cursor
  /// this method is also used by derived classes
  //////////////////////////////////////////////////////////////////////////////

  void processQuery(arangodb::velocypack::Slice const&);

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief register the currently running query
  //////////////////////////////////////////////////////////////////////////////

  void registerQuery(arangodb::aql::Query*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief unregister the currently running query
  //////////////////////////////////////////////////////////////////////////////

  void unregisterQuery();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief cancel the currently running query
  //////////////////////////////////////////////////////////////////////////////

  bool cancelQuery();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not the query was canceled
  //////////////////////////////////////////////////////////////////////////////

  bool wasCanceled();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief build options for the query as JSON
  //////////////////////////////////////////////////////////////////////////////

  arangodb::velocypack::Builder buildOptions(
      arangodb::velocypack::Slice const&) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief builds the "extra" attribute values from the result.
  /// note that the "extra" object will take ownership from the result for
  /// several values
  //////////////////////////////////////////////////////////////////////////////

  std::shared_ptr<arangodb::velocypack::Builder> buildExtra(
      arangodb::aql::QueryResult&) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief append the contents of the cursor into the response body
  //////////////////////////////////////////////////////////////////////////////

  void dumpCursor(arangodb::Cursor*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief create a cursor and return the first results
  //////////////////////////////////////////////////////////////////////////////

  void createCursor();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the next results from an existing cursor
  //////////////////////////////////////////////////////////////////////////////

  void modifyCursor();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief dispose an existing cursor
  //////////////////////////////////////////////////////////////////////////////

  void deleteCursor();

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief _applicationV8
  //////////////////////////////////////////////////////////////////////////////

  arangodb::ApplicationV8* _applicationV8;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief our query registry
  //////////////////////////////////////////////////////////////////////////////

  arangodb::aql::QueryRegistry* _queryRegistry;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief lock for currently running query
  //////////////////////////////////////////////////////////////////////////////

  Mutex _queryLock;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief currently running query
  //////////////////////////////////////////////////////////////////////////////

  arangodb::aql::Query* _query;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not the query was killed
  //////////////////////////////////////////////////////////////////////////////

  bool _queryKilled;
};
}

#endif
