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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_REST_HANDLER_REST_SIMPLE_HANDLER_H
#define ARANGOD_REST_HANDLER_REST_SIMPLE_HANDLER_H 1

#include "Basics/Common.h"
#include "RestHandler/RestCursorHandler.h"

namespace arangodb {
namespace aql {
class QueryRegistry;
}  // namespace aql

class RestSimpleHandler : public RestCursorHandler {
 public:
  RestSimpleHandler(application_features::ApplicationServer&, GeneralRequest*,
                    GeneralResponse*, aql::QueryRegistry*);

 public:
  RestStatus execute() override final;
  char const* name() const override final { return "RestSimpleHandler"; }

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief handle the result returned by the query. This function is
  /// guaranteed
  ///        to not be interrupted and is guaranteed to get a complete
  ///        queryResult.
  //////////////////////////////////////////////////////////////////////////////

  RestStatus handleQueryResult() override final;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief handle result of a remove-by-keys query
  //////////////////////////////////////////////////////////////////////////////

  void handleQueryResultRemoveByKeys();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief handle result of a lookup-by-keys query
  //////////////////////////////////////////////////////////////////////////////

  void handleQueryResultLookupByKeys();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief execute a batch remove operation
  //////////////////////////////////////////////////////////////////////////////

  RestStatus removeByKeys(VPackSlice const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief execute a batch lookup operation
  //////////////////////////////////////////////////////////////////////////////

  RestStatus lookupByKeys(VPackSlice const&);

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief if the request was silent, only relevant for remove
  //////////////////////////////////////////////////////////////////////////////
  bool _silent;
};
}  // namespace arangodb

#endif
