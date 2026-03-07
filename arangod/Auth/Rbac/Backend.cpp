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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "Backend.h"


namespace arangodb::rbac {

auto Backend::evaluateTokenMany(Backend::JwtToken const& jwtToken,
                                Backend::RequestItems const& items)
    -> futures::Future<ResultT<EvaluateResponseMany>> {
  return evaluateTokenManyImpl(jwtToken, items,
                               transaction::MethodsApi::Asynchronous);
}

auto Backend::evaluateMany(Backend::PlainUser const& user,
                           Backend::RequestItems const& items)
    -> futures::Future<ResultT<EvaluateResponseMany>> {
  return evaluateManyImpl(user, items, transaction::MethodsApi::Asynchronous);
}

auto Backend::evaluateToken(Backend::JwtToken const& jwtToken,
                            Backend::RequestItem const& item)
    -> futures::Future<ResultT<EvaluateResponse>> {
  return evaluateTokenImpl(jwtToken, item,
                           transaction::MethodsApi::Asynchronous);
}

auto Backend::evaluate(Backend::PlainUser const& user,
                       Backend::RequestItem const& item)
    -> futures::Future<ResultT<EvaluateResponse>> {
  return evaluateImpl(user, item, transaction::MethodsApi::Asynchronous);
}

auto Backend::evaluateTokenManySync(Backend::JwtToken const& jwtToken,
                                    Backend::RequestItems const& items)
    -> ResultT<EvaluateResponseMany> {
  return evaluateTokenManyImpl(jwtToken, items,
                               transaction::MethodsApi::Synchronous)
      .waitAndGet();
}

auto Backend::evaluateManySync(Backend::PlainUser const& user,
                               Backend::RequestItems const& items)
    -> ResultT<EvaluateResponseMany> {
  return evaluateManyImpl(user, items, transaction::MethodsApi::Synchronous)
      .waitAndGet();
}

auto Backend::evaluateTokenSync(Backend::JwtToken const& jwtToken,
                                Backend::RequestItem const& item)
    -> ResultT<EvaluateResponse> {
  return evaluateTokenImpl(jwtToken, item, transaction::MethodsApi::Synchronous)
      .waitAndGet();
}

auto Backend::evaluateSync(Backend::PlainUser const& user,
                           Backend::RequestItem const& item)
    -> ResultT<EvaluateResponse> {
  return evaluateImpl(user, item, transaction::MethodsApi::Synchronous)
      .waitAndGet();
}

} // namespace arangodb::rbac

