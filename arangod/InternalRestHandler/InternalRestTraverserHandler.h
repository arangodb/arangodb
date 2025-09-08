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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RestHandler/RestVocbaseBaseHandler.h"

namespace arangodb {
namespace aql {
class QueryRegistry;
}
namespace graph {
class EdgeCursor;
}
namespace traverser {
class BaseEngine;
}

class InternalRestTraverserHandler : public RestVocbaseBaseHandler {
 public:
  explicit InternalRestTraverserHandler(ArangodServer&, GeneralRequest*,
                                        GeneralResponse*, aql::QueryRegistry*);

 public:
  auto executeAsync() -> futures::Future<futures::Unit> override;
  char const* name() const override final {
    return "InternalRestTraverserHandler";
  }
  RequestLane lane() const override final { return RequestLane::CLUSTER_AQL; }

 private:
  // @brief Query an existing Traverser Engine.
  void queryEngine();

  // @brief Destroy an existing Traverser Engine.
  void destroyEngine();

  ResultT<traverser::BaseEngine*> get_engine(uint64_t engineId);

  aql::QueryRegistry* _registry;
};

}  // namespace arangodb
