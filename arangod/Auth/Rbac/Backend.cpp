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

#include "Backend.h"

namespace arangodb::rbac {

auto Backend::evaluateMany(Subject const& subject,
                           Backend::RequestItems const& items)
    -> futures::Future<ResultT<EvaluateResponseMany>> {
  return evaluateManyImpl(subject, items,
                          transaction::MethodsApi::Asynchronous);
}

auto Backend::evaluateManySync(Subject const& subject,
                               Backend::RequestItems const& items)
    -> ResultT<EvaluateResponseMany> {
  return evaluateManyImpl(subject, items, transaction::MethodsApi::Synchronous)
      .waitAndGet();
}

}  // namespace arangodb::rbac
