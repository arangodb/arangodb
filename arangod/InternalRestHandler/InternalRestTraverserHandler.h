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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_INTERNAL_REST_HANDLER_INTERNAL_TRAVERSER_HANDLER_H
#define ARANGOD_INTERNAL_REST_HANDLER_INTERNAL_TRAVERSER_HANDLER_H 1

#include "RestHandler/RestVocbaseBaseHandler.h"

namespace arangodb {
namespace aql {
class QueryRegistry;
}

class InternalRestTraverserHandler : public RestVocbaseBaseHandler {
 public:
  explicit InternalRestTraverserHandler(application_features::ApplicationServer&,
                                        GeneralRequest*, GeneralResponse*,
                                        aql::QueryRegistry*);

 public:
  RestStatus execute() override final;
  char const* name() const override final {
    return "InternalRestTraverserHandler";
  }
  RequestLane lane() const override final { return RequestLane::CLUSTER_AQL; }

 private:
  // @brief create a new Traverser Engine.
  void createEngine();

  // @brief Query an existing Traverser Engine.
  void queryEngine();

  // @brief Destroy an existing Traverser Engine.
  void destroyEngine();

 private:
  aql::QueryRegistry* _registry;
};
}  // namespace arangodb
#endif
