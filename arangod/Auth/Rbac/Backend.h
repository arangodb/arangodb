////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Auth/Rbac/Actions.h"
#include "Basics/ResultT.h"
#include "Futures/Future.h"
#include "Transaction/MethodsApi.h"

namespace arangodb::rbac {

struct Backend {
  virtual ~Backend() = default;

  // types
  enum class Effect { Allow, Deny };
  struct ResponseItem {
    Effect effect;
    std::string message;
  };
  struct EvaluateResponseMany : ResponseItem {
    std::vector<ResponseItem> items;
  };

  struct RequestItem {
    // .action
    std::string action;
    // .resource
    std::string resource;
    // .context.parameters.attribute.values:
    std::vector<std::string> attributeValues;
  };
  struct RequestItems {
    std::vector<RequestItem> items;
  };

  // Batched token evaluation, in both an asynchronous and a synchronous form.
  // The synchronous form sets `skipScheduler` and blocks for the response; the
  // asynchronous form is currently unused but kept for a future async check().
  auto evaluateTokenMany(JwtToken const&, RequestItems const&)
      -> futures::Future<ResultT<EvaluateResponseMany>>;
  auto evaluateTokenManySync(JwtToken const&, RequestItems const&)
      -> ResultT<EvaluateResponseMany>;

 protected:
  // Implementation seam. `transaction::MethodsApi` selects the synchronous vs
  // asynchronous transport behaviour (see BackendImpl).
  virtual auto evaluateTokenManyImpl(JwtToken const&, RequestItems const&,
                                     transaction::MethodsApi api)
      -> futures::Future<ResultT<EvaluateResponseMany>> = 0;
};

}  // namespace arangodb::rbac
