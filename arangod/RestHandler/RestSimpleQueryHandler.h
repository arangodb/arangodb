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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RestHandler/RestCursorHandler.h"

namespace arangodb {
namespace aql {
class QueryRegistry;
}

class RestSimpleQueryHandler : public RestCursorHandler {
 public:
  RestSimpleQueryHandler(ArangodServer&, GeneralRequest*, GeneralResponse*,
                         arangodb::aql::QueryRegistry*);

 public:
  RestStatus execute() override final;
  char const* name() const override final { return "RestSimpleQueryHandler"; }

 private:
  futures::Future<RestStatus> allDocuments();
  futures::Future<RestStatus> allDocumentKeys();
  futures::Future<RestStatus> byExample();
};
}  // namespace arangodb
