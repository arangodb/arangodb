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

#ifndef ARANGOD_REST_HANDLER_REST_SIMPLE_QUERY_HANDLER_H
#define ARANGOD_REST_HANDLER_REST_SIMPLE_QUERY_HANDLER_H 1

#include "Basics/Common.h"
#include "RestHandler/RestCursorHandler.h"

namespace arangodb {
namespace aql {
class QueryRegistry;
}

class ApplicationV8;

////////////////////////////////////////////////////////////////////////////////
/// @brief cursor request handler
////////////////////////////////////////////////////////////////////////////////

class RestSimpleQueryHandler : public RestCursorHandler {
 public:
  RestSimpleQueryHandler(
      rest::HttpRequest*,
      std::pair<arangodb::ApplicationV8*, arangodb::aql::QueryRegistry*>*);

  RestSimpleQueryHandler(rest::GeneralRequest*,
                         std::pair<arangodb::ApplicationV8*,
                                   arangodb::aql::QueryRegistry*>*);

 public:
  status_t execute() override final;

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief return a cursor with all documents from the collection
  //////////////////////////////////////////////////////////////////////////////

  void allDocuments();
};
}

#endif
